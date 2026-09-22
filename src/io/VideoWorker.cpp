#include "io/VideoWorker.h"
#include "core/CameraState.h"
#include <spdlog/spdlog.h>
#include <mutex>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

namespace {
// Backstop against a wedged read after the connection is already
// established (a dropped Wi-Fi link, a Pi-side process hang, etc). This
// is intentionally more generous than staleFrameMs -- staleFrameMs already
// flips the UI-facing isStreaming() to false quickly; this timeout only
// exists to force the underlying FFmpeg call to actually return so the
// reconnect loop can run, rather than blocking forever.
constexpr uint32_t kReadTimeoutFloorMs = 3000;

int64_t nowMs() {
    return std::chrono::duration_cast<Ms>(Clock::now().time_since_epoch()).count();
}
} // namespace

VideoWorker::VideoWorker(AppState& state, const AppConfig::VideoCfg& config)
    : m_state(state), m_config(config), m_userEnabled(config.enabled) {
    static std::once_flag s_networkInitFlag;
    std::call_once(s_networkInitFlag, [] {
        avformat_network_init();
        // The Pi-side ffmpeg remux emits continuous "Non-monotonic DTS"
        // warnings (rpicam-vid doesn't produce clean timestamps) -- expected
        // and harmless for a display-only decode path that doesn't rely on
        // PTS/DTS ordering, so drop FFmpeg's log level to avoid spamming
        // this app's console/log file with noise on every frame.
        av_log_set_level(AV_LOG_ERROR);
    });
}

VideoWorker::~VideoWorker() { stop(); }

void VideoWorker::start() {
    if (m_running.exchange(true)) return;
    m_thread = std::thread([this] { loop(); });
}

void VideoWorker::requestStop() {
    m_running = false;
}

void VideoWorker::stop() {
    requestStop();
    if (m_thread.joinable()) m_thread.join();
}

void VideoWorker::setEnabled(bool enabled) {
    m_userEnabled.store(enabled, std::memory_order_relaxed);
}

bool VideoWorker::isStreaming() const {
    QImage discard;
    return tryCurrentFrame(discard);
}

QImage VideoWorker::currentFrame() const {
    std::lock_guard<std::mutex> lk(m_frameMutex);
    return m_latestFrame;
}

bool VideoWorker::tryCurrentFrame(QImage& out) const {
    if (!m_connected.load(std::memory_order_acquire)) return false;
    std::lock_guard<std::mutex> lk(m_frameMutex);
    if (m_latestFrame.isNull()) return false;
    const auto ageMs = std::chrono::duration_cast<Ms>(Clock::now() - m_lastFrameTime).count();
    if (ageMs > static_cast<int64_t>(m_config.staleFrameMs)) return false;
    out = m_latestFrame;
    return true;
}

VideoWorker::Status VideoWorker::status(bool streaming) const {
    if (!m_userEnabled.load(std::memory_order_relaxed)) return Status::Disabled;
    return streaming ? Status::Streaming : Status::Connecting;
}

void VideoWorker::clearFrame() {
    std::lock_guard<std::mutex> lk(m_frameMutex);
    m_latestFrame = QImage();
}

void VideoWorker::publishStreaming(bool streaming) {
    std::lock_guard<std::mutex> lk(m_state.registryMutex);
    auto& camera = m_state.registry.get<CameraState>(m_state.ugv);
    if (!streaming) {
        camera = CameraState{};
        return;
    }

    camera.streaming = true;
    camera.width = m_codecCtx ? m_codecCtx->width : 0;
    camera.height = m_codecCtx ? m_codecCtx->height : 0;
    camera.transport = m_config.preferTcp ? "TCP" : "UDP";

    if (m_codecCtx) {
        camera.codec = avcodec_get_name(m_codecCtx->codec_id);
        if (m_codecCtx->codec) {
            if (const char* profile = av_get_profile_name(m_codecCtx->codec, m_codecCtx->profile)) {
                camera.profile = profile;
            }
        }
    }

    if (m_fmtCtx && m_videoStreamIndex >= 0) {
        const AVRational rate = av_guess_frame_rate(
            m_fmtCtx, m_fmtCtx->streams[m_videoStreamIndex], nullptr);
        if (rate.num > 0 && rate.den > 0) camera.advertisedFps = av_q2d(rate);
    }
}

void VideoWorker::publishStreamStats(double decodedFps, uint32_t bitrateKbps) {
    std::lock_guard<std::mutex> lk(m_state.registryMutex);
    auto& camera = m_state.registry.get<CameraState>(m_state.ugv);
    if (!camera.streaming) return;
    camera.decodedFps = decodedFps;
    camera.bitrateKbps = bitrateKbps;
}

void VideoWorker::armDeadline(uint32_t timeoutMs) {
    m_ioDeadlineMs = nowMs() + static_cast<int64_t>(timeoutMs);
}

bool VideoWorker::shouldAbort() const {
    if (!m_running.load(std::memory_order_relaxed)) return true;
    if (!m_userEnabled.load(std::memory_order_relaxed)) return true;
    return nowMs() > m_ioDeadlineMs;
}

int VideoWorker::interruptCallback(void* opaque) {
    return static_cast<VideoWorker*>(opaque)->shouldAbort() ? 1 : 0;
}

void VideoWorker::interruptibleSleep(uint32_t ms) {
    const auto end = Clock::now() + Ms(ms);
    while (m_running.load() && Clock::now() < end) {
        std::this_thread::sleep_for(Ms(50));
    }
}

bool VideoWorker::openStream() {
    m_fmtCtx = avformat_alloc_context();
    if (!m_fmtCtx) {
        spdlog::error("VideoWorker: avformat_alloc_context failed");
        return false;
    }
    m_fmtCtx->interrupt_callback.callback = &VideoWorker::interruptCallback;
    m_fmtCtx->interrupt_callback.opaque   = this;

    AVDictionary* opts = nullptr;
    // Select the transport explicitly. Leaving FFmpeg's transport on
    // "automatic" can fall back to TCP, where a slow frame consumer builds
    // seconds of stale video behind the socket's reliable byte stream. UDP
    // is the intended teleoperation mode: a late packet can be discarded
    // instead of delaying every frame after it.
    av_dict_set(&opts, "rtsp_transport", m_config.preferTcp ? "tcp" : "udp", 0);

    // Keep the demuxer and UDP socket shallow while still giving Windows
    // enough kernel receive space for an encoded I-frame burst. In
    // particular, disable RTP reordering: waiting for a missing Wi-Fi packet
    // is useful for playback, but visibly increases control-camera latency.
    av_dict_set(&opts, "fflags", "nobuffer", 0);
    av_dict_set(&opts, "avioflags", "direct", 0);
    av_dict_set(&opts, "flags", "low_delay", 0);
    av_dict_set(&opts, "max_delay", "0", 0);
    av_dict_set(&opts, "reorder_queue_size", "0", 0);
    av_dict_set(&opts, "buffer_size", "2097152", 0);

    armDeadline(m_config.openTimeoutMs);
    int ret = avformat_open_input(&m_fmtCtx, m_config.url.c_str(), nullptr, &opts);
    av_dict_free(&opts);
    if (ret < 0) {
        char errbuf[128];
        av_strerror(ret, errbuf, sizeof(errbuf));
        spdlog::warn("VideoWorker: failed to open '{}': {}", m_config.url, errbuf);
        return false;
    }

    armDeadline(m_config.openTimeoutMs);
    if (avformat_find_stream_info(m_fmtCtx, nullptr) < 0) {
        spdlog::warn("VideoWorker: avformat_find_stream_info failed for '{}'", m_config.url);
        return false;
    }

    const AVCodec* decoder = nullptr;
    m_videoStreamIndex = av_find_best_stream(m_fmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);
    if (m_videoStreamIndex < 0 || !decoder) {
        spdlog::warn("VideoWorker: no video stream found in '{}'", m_config.url);
        return false;
    }

    m_codecCtx = avcodec_alloc_context3(decoder);
    if (!m_codecCtx) {
        spdlog::error("VideoWorker: avcodec_alloc_context3 failed");
        return false;
    }
    if (avcodec_parameters_to_context(m_codecCtx, m_fmtCtx->streams[m_videoStreamIndex]->codecpar) < 0) {
        spdlog::warn("VideoWorker: avcodec_parameters_to_context failed");
        return false;
    }
    // Single-threaded decode: frame-parallel decoding trades latency for
    // throughput by decoding several frames ahead in parallel, which is
    // the wrong tradeoff for a live teleoperation feed. 720p H.264 is
    // cheap enough on one core.
    m_codecCtx->thread_count = 1;
    m_codecCtx->flags |= AV_CODEC_FLAG_LOW_DELAY;

    if (avcodec_open2(m_codecCtx, decoder, nullptr) < 0) {
        spdlog::warn("VideoWorker: avcodec_open2 failed");
        return false;
    }

    m_frame = av_frame_alloc();
    m_pkt   = av_packet_alloc();
    if (!m_frame || !m_pkt) {
        spdlog::error("VideoWorker: frame/packet allocation failed");
        return false;
    }

    spdlog::info("VideoWorker: connected to '{}' ({}x{})", m_config.url,
                 m_codecCtx->width, m_codecCtx->height);
    return true;
}

void VideoWorker::closeStream() {
    // av_packet_free/av_frame_free/avcodec_free_context/avformat_close_input
    // are all documented null-safe (no-op on a null/already-null pointer)
    // and self-nulling (they null out the pointer they're given) -- no
    // manual if-guard needed. sws_freeContext is null-safe too but does NOT
    // self-null, so it still needs the explicit reset after.
    sws_freeContext(m_swsCtx);
    m_swsCtx = nullptr;
    av_packet_free(&m_pkt);
    av_frame_free(&m_frame);
    avcodec_free_context(&m_codecCtx);
    avformat_close_input(&m_fmtCtx);
    m_videoStreamIndex = -1;
    m_swsW = 0;
    m_swsH = 0;
    m_swsSrcFmt = -1;
}

void VideoWorker::storeFrame(const AVFrame* frame) {
    const int w = frame->width;
    const int h = frame->height;
    const int fmt = frame->format;
    if (w <= 0 || h <= 0) return;

    if (!m_swsCtx || w != m_swsW || h != m_swsH || fmt != m_swsSrcFmt) {
        sws_freeContext(m_swsCtx); // null-safe no-op if not yet allocated
        m_swsCtx = sws_getContext(w, h, static_cast<AVPixelFormat>(fmt), w, h, AV_PIX_FMT_BGRA,
                                   SWS_BILINEAR, nullptr, nullptr, nullptr);
        m_swsW = w;
        m_swsH = h;
        m_swsSrcFmt = fmt;
        if (!m_swsCtx) {
            spdlog::warn("VideoWorker: sws_getContext failed for {}x{} fmt={}", w, h, fmt);
            return;
        }
    }

    // AV_PIX_FMT_BGRA's in-memory byte order (B,G,R,A) matches
    // QImage::Format_RGB32's in-memory layout on little-endian, so
    // sws_scale can write straight into the QImage's own buffer.
    QImage img(w, h, QImage::Format_RGB32);
    if (img.isNull()) return;

    uint8_t* dstData[4]    = {img.bits(), nullptr, nullptr, nullptr};
    int      dstLinesize[4] = {static_cast<int>(img.bytesPerLine()), 0, 0, 0};
    sws_scale(m_swsCtx, frame->data, frame->linesize, 0, h, dstData, dstLinesize);

    std::lock_guard<std::mutex> lk(m_frameMutex);
    m_latestFrame   = std::move(img);
    m_lastFrameTime = Clock::now();
}

void VideoWorker::decodeLoop() {
    const uint32_t readTimeoutMs =
        std::max<uint32_t>(m_config.staleFrameMs * 3u, kReadTimeoutFloorMs);
    auto statsStart = Clock::now();
    uint64_t encodedBytes = 0;
    uint32_t decodedFrames = 0;

    while (m_running.load() && m_userEnabled.load(std::memory_order_relaxed)) {
        armDeadline(readTimeoutMs);
        int ret = av_read_frame(m_fmtCtx, m_pkt);
        if (ret < 0) {
            if (ret != AVERROR_EOF && m_running.load(std::memory_order_relaxed)
                && m_userEnabled.load(std::memory_order_relaxed)) {
                char errbuf[128];
                av_strerror(ret, errbuf, sizeof(errbuf));
                spdlog::warn("VideoWorker: av_read_frame error: {}", errbuf);
            }
            return;
        }

        if (m_pkt->stream_index != m_videoStreamIndex) {
            av_packet_unref(m_pkt);
            continue;
        }

        encodedBytes += static_cast<uint64_t>(std::max(m_pkt->size, 0));

        ret = avcodec_send_packet(m_codecCtx, m_pkt);
        av_packet_unref(m_pkt);
        if (ret < 0 && ret != AVERROR(EAGAIN)) {
            char errbuf[128];
            av_strerror(ret, errbuf, sizeof(errbuf));
            spdlog::warn("VideoWorker: avcodec_send_packet error: {}", errbuf);
            return;
        }

        while (true) {
            ret = avcodec_receive_frame(m_codecCtx, m_frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
            if (ret < 0) {
                char errbuf[128];
                av_strerror(ret, errbuf, sizeof(errbuf));
                spdlog::warn("VideoWorker: avcodec_receive_frame error: {}", errbuf);
                return;
            }
            storeFrame(m_frame);
            ++decodedFrames;
            av_frame_unref(m_frame);
        }

        const auto now = Clock::now();
        const auto statsElapsedMs = std::chrono::duration_cast<Ms>(now - statsStart).count();
        if (statsElapsedMs >= 1000) {
            const double seconds = static_cast<double>(statsElapsedMs) / 1000.0;
            const double fps = static_cast<double>(decodedFrames) / seconds;
            const auto kbps = static_cast<uint32_t>(
                (static_cast<double>(encodedBytes) * 8.0) / (seconds * 1000.0));
            publishStreamStats(fps, kbps);
            statsStart = now;
            encodedBytes = 0;
            decodedFrames = 0;
        }
    }
}

void VideoWorker::loop() {
    while (m_running.load()) {
        if (!m_userEnabled.load(std::memory_order_relaxed) || m_config.url.empty()) {
            if (m_connected.exchange(false, std::memory_order_acq_rel)) {
                clearFrame();
                publishStreaming(false);
            }
            interruptibleSleep(500);
            continue;
        }

        if (!openStream()) {
            closeStream();
            m_connected = false;
            interruptibleSleep(m_config.reconnectDelayMs);
            continue;
        }

        m_connected = true;
        publishStreaming(true);

        decodeLoop();

        m_connected = false;
        clearFrame();
        publishStreaming(false);

        closeStream();

        if (!m_running.load()) break;
        interruptibleSleep(m_config.reconnectDelayMs);
    }

    if (m_connected.exchange(false)) {
        clearFrame();
        publishStreaming(false);
    }
    closeStream();
}
