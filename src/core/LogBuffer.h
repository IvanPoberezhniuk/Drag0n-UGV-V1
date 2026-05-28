#pragma once
#include <string>
#include <deque>
#include <vector>
#include <mutex>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>

struct LogEntry {
    spdlog::level::level_enum level;
    std::string               message;
    int64_t                   timestampMs;
};

class LogBuffer {
public:
    static constexpr size_t kMaxEntries = 2000;

    void append(spdlog::level::level_enum lvl, std::string msg, int64_t tsMs) {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (m_entries.size() >= kMaxEntries) m_entries.pop_front();
        m_entries.push_back({lvl, std::move(msg), tsMs});
    }

    std::vector<LogEntry> snapshot() const {
        std::lock_guard<std::mutex> lk(m_mutex);
        return {m_entries.begin(), m_entries.end()};
    }

    void clear() {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_entries.clear();
    }

private:
    mutable std::mutex   m_mutex;
    std::deque<LogEntry> m_entries;
};

// spdlog sink that feeds into LogBuffer — thread-safe via base_sink<std::mutex>
class UiLogSink : public spdlog::sinks::base_sink<std::mutex> {
public:
    explicit UiLogSink(LogBuffer& buf) : m_buf(buf) {}

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        spdlog::memory_buf_t formatted;
        this->formatter_->format(msg, formatted);
        auto tsMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            msg.time.time_since_epoch()).count();
        m_buf.append(msg.level,
            std::string(formatted.data(), formatted.size()), tsMs);
    }
    void flush_() override {}

private:
    LogBuffer& m_buf;
};
