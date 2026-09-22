#pragma once
#include "config/AppConfig.h"
#include "core/AppState.h"
#include "core/ChronoTypes.h"
#include <QImage>
#include <thread>
#include <atomic>
#include <mutex>
#include <cstdint>

// FFmpeg types are opaque here -- avformat.h/avcodec.h are C headers that
// don't play well included alongside Qt/moc, and no other translation unit
// needs them. Only VideoWorker.cpp includes the real libav* headers.
struct AVFormatContext;
struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct SwsContext;

// Decode-only RTSP client for the Raspberry Pi's live camera feed
// (rpicam-vid -> ffmpeg -> MediaMTX -> rtsp://roverpi.local:8554/ugv).
// Mirrors SerialWorker's shape: owns its own std::thread, start()/stop(),
// never blocks the caller.
//
// Threading contract: all FFmpeg state lives entirely on the worker
// thread. The only cross-thread surface is m_frameMutex, which guards a
// single "latest frame" slot -- frames are never queued, so a slow
// consumer drops backlog instead of falling behind. This mutex is
// deliberately separate from AppState::registryMutex: video decode must
// never be able to contend with (and stall) the 50 Hz serial/control loop.
// Accordingly, AppState/CameraState is only touched on connect/disconnect
// transitions (see publishStreaming()), never per-frame.
//
// Failure behavior: any open/read/decode error marks the stream
// not-streaming, closes cleanly, sleeps reconnectDelayMs, and retries.
// A staleness watchdog independently flips isStreaming() to false if no
// new frame has landed within staleFrameMs, even while the underlying
// socket still looks open -- this catches a hung Pi-side encoder, not
// just a dropped TCP connection. Known to survive the Pi ffmpeg remux's
// continuous non-monotonic DTS warnings (rpicam-vid does not emit clean
// timestamps) without special-casing: frames are decoded and displayed
// as they arrive, with no PTS/DTS ordering assumed.
class VideoWorker {
public:
    VideoWorker(AppState& state, const AppConfig::VideoCfg& config);
    ~VideoWorker();

    VideoWorker(const VideoWorker&) = delete;
    VideoWorker& operator=(const VideoWorker&) = delete;

    void start();
    void stop();

    // Signals the worker thread to unwind (aborts any in-flight blocking
    // FFmpeg call via the interrupt callback) without joining. Callers that
    // need to stop several workers with an upper-bounded total wait should
    // call requestStop() on all of them first, then stop() (or join
    // directly) on each -- see MainWindow::closeEvent.
    void requestStop();

    // User-facing on/off switch (dashboard camera toggle), independent of
    // m_config.enabled's startup default. When disabled, the worker stops
    // attempting to (re)connect and clears any current frame/streaming
    // state, exactly like a disconnect.
    void setEnabled(bool enabled);

    // True only when the stream is open AND a frame has landed within
    // staleFrameMs -- see class comment.
    bool isStreaming() const;

    // Cheap: QImage is copy-on-write, and the worker thread only ever
    // replaces m_latestFrame with a freshly-allocated QImage (never
    // mutates an existing one in place), so this copy-under-lock never
    // races with pixel data the caller is still reading.
    QImage currentFrame() const;

    // Single-lock equivalent of isStreaming() + currentFrame(): does the
    // staleness check and frame copy under one m_frameMutex acquisition.
    // Returns false (and leaves out untouched) if not currently streaming.
    bool tryCurrentFrame(QImage& out) const;

    // Coarse status for UI badges (VideoPanel's "NO SIGNAL"/"Loading..."
    // overlay): Disabled when the user has toggled the feed off, Connecting
    // when enabled but nothing is streaming yet (covers both the initial
    // open attempt and the retry-after-failure wait), Streaming once a
    // fresh frame is actually landing. Takes the caller's already-computed
    // tryCurrentFrame() result instead of re-checking m_frameMutex itself,
    // so calling both in the same paint pass costs one lock, not two.
    enum class Status { Disabled, Connecting, Streaming };
    Status status(bool streaming) const;

private:
    void loop();
    bool openStream();
    void closeStream();
    void decodeLoop();
    void storeFrame(const AVFrame* frame);
    void armDeadline(uint32_t timeoutMs);
    bool shouldAbort() const;
    void interruptibleSleep(uint32_t ms);
    void clearFrame();
    void publishStreaming(bool streaming);

    static int interruptCallback(void* opaque);

    AppState&                m_state;
    const AppConfig::VideoCfg& m_config;

    std::thread       m_thread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_connected{false};
    std::atomic<bool> m_userEnabled;

    // Deadline for the current blocking FFmpeg call, expressed as Clock
    // (steady_clock) milliseconds-since-epoch. Checked by
    // interruptCallback() so a wedged socket/decoder can't hang stop() or
    // the reconnect loop indefinitely. Plain int64_t, not atomic: it's only
    // ever written (armDeadline) and read (shouldAbort) from the worker
    // thread itself -- decode is single-threaded (thread_count=1) and
    // interrupt_callback is invoked synchronously on the same blocking
    // call, never from another thread.
    int64_t m_ioDeadlineMs = 0;

    mutable std::mutex m_frameMutex;
    QImage              m_latestFrame;
    Clock::time_point   m_lastFrameTime{};

    // FFmpeg state -- owned exclusively by the worker thread, only ever
    // touched inside loop()/openStream()/closeStream()/decodeLoop().
    AVFormatContext* m_fmtCtx           = nullptr;
    AVCodecContext*  m_codecCtx         = nullptr;
    SwsContext*      m_swsCtx           = nullptr;
    AVFrame*         m_frame            = nullptr;
    AVPacket*        m_pkt              = nullptr;
    int              m_videoStreamIndex = -1;
    int              m_swsW             = 0;
    int              m_swsH             = 0;
    int              m_swsSrcFmt        = -1; // AVPixelFormat, kept as int to avoid the enum in this header
};
