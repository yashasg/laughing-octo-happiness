#pragma once

#include "config.h"
#include <string>
#include <mutex>
#include <atomic>
#include <thread>
#include <functional>
#include <memory>
#include <cstdint>

/// Monitors ~/.copilot/session-state/ for session activity.
/// Runs a filesystem watcher + polling fallback on background threads.
/// All public getters are thread-safe.
class StatusMonitor {
public:
    using Callback = std::function<void(CopilotStatus, const std::string&)>;

    explicit StatusMonitor(Callback on_change);
    ~StatusMonitor();

    void start();
    void stop();

    // Thread-safe getters (called from main/render thread)
    CopilotStatus status() const;
    std::string   status_text() const;
    std::string   model_name() const;
    size_t        context_bytes() const;
    size_t        current_tokens() const;

    void check_and_notify();  // also called from dmon's background thread

private:
    void poll_loop();
    std::string find_active_session() const;
    void parse_status(const std::string& session_dir);
    void scan_for_model(const std::string& events_path);

    Callback m_on_change;

    mutable std::mutex m_mutex;
    std::atomic<CopilotStatus> m_status{CopilotStatus::IDLE};
    std::string m_status_text;
    std::string m_model_name;
    std::atomic<size_t> m_context_bytes{0};
    std::atomic<size_t> m_current_tokens{0};

    std::atomic<bool> m_running{false};
    std::unique_ptr<std::thread> m_poll_thread;
    uint32_t m_watch_id{0};         // dmon_watch_id.id; 0 = not watching
    bool m_dmon_initialized{false}; // true iff dmon_init() has been called

    std::string m_state_dir;  // resolved path to ~/.copilot/session-state
};
