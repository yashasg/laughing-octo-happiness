#include "status_monitor.h"

#include <efsw/efsw.hpp>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <chrono>
#include <thread>
#include <unordered_map>
#include <cstdlib>

namespace fs = std::filesystem;
using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Event-type → status mapping (mirrors Python _STATUS_MAP)
// ---------------------------------------------------------------------------
static const std::unordered_map<std::string, CopilotStatus> STATUS_MAP = {
    {"assistant.turn_start",  CopilotStatus::BUSY},
    {"tool.execution_start",  CopilotStatus::BUSY},
    {"assistant.message",     CopilotStatus::BUSY},
    {"hook.start",            CopilotStatus::BUSY},
    {"assistant.turn_end",    CopilotStatus::WAITING},
    {"session.task_complete", CopilotStatus::IDLE},
};

// ---------------------------------------------------------------------------
// File-watch listener — routes relevant fs events back via a std::function
// callback, with a 150 ms debounce guard.
// ---------------------------------------------------------------------------
class StatusFileListener : public efsw::FileWatchListener {
public:
    explicit StatusFileListener(std::function<void()> callback)
        : m_callback(std::move(callback)), m_last_ms(0) {}

    void handleFileAction(efsw::WatchID /*id*/, const std::string& /*dir*/,
                          const std::string& filename, efsw::Action action,
                          std::string /*old*/) override {
        // Ignore deletes — the poll loop handles session-end detection
        if (action == efsw::Actions::Delete) return;
        if (ends_with(filename, ".jsonl") || ends_with(filename, ".lock"))
            debounce();
    }

private:
    static bool ends_with(const std::string& s, const std::string& suffix) {
        if (s.size() < suffix.size()) return false;
        return s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    void debounce() {
        int64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        int64_t last = m_last_ms.load(std::memory_order_relaxed);
        if (now_ms - last < 150) return;
        m_last_ms.store(now_ms, std::memory_order_relaxed);
        m_callback();
    }

    std::function<void()> m_callback;
    std::atomic<int64_t>  m_last_ms;
};

// ---------------------------------------------------------------------------
// Module-scope listener storage.
// There is exactly one StatusMonitor instance per process; a static pointer
// lets us attach the listener's lifetime to start()/stop() without changing
// the header (which fixes the unique_ptr<FileWatcher> type to the default
// deleter, preventing a custom-deleter bundle).
// ---------------------------------------------------------------------------
static StatusFileListener* s_listener = nullptr;

// ---------------------------------------------------------------------------
// StatusMonitor
// ---------------------------------------------------------------------------

StatusMonitor::StatusMonitor(Callback on_change)
    : m_on_change(std::move(on_change))
{
    const char* home = std::getenv("HOME");
    m_state_dir = std::string(home ? home : "") + "/.copilot/session-state";
}

StatusMonitor::~StatusMonitor() {
    stop();
}

void StatusMonitor::start() {
    if (m_running.exchange(true)) return; // already running

    check_and_notify();

    if (fs::exists(m_state_dir)) {
        if (s_listener) { delete s_listener; s_listener = nullptr; }
        s_listener = new StatusFileListener([this]() { check_and_notify(); });

        m_watcher = std::make_unique<efsw::FileWatcher>();
        m_watcher->addWatch(m_state_dir, s_listener, /*recursive=*/true);
        m_watcher->watch();
    } else {
        std::cerr << "[StatusMonitor] State dir not found: " << m_state_dir << "\n";
    }

    m_poll_thread = std::make_unique<std::thread>(&StatusMonitor::poll_loop, this);
}

void StatusMonitor::stop() {
    if (!m_running.exchange(false)) return; // already stopped

    // Destroy watcher — stops its background thread before we clean up listener
    m_watcher.reset();
    if (s_listener) { delete s_listener; s_listener = nullptr; }

    if (m_poll_thread && m_poll_thread->joinable()) {
        m_poll_thread->join();
        m_poll_thread.reset();
    }
}

// ---------------------------------------------------------------------------
// Thread-safe public getters
// ---------------------------------------------------------------------------

CopilotStatus StatusMonitor::status() const {
    return m_status.load();
}

std::string StatusMonitor::status_text() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_status_text;
}

std::string StatusMonitor::model_name() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_model_name;
}

size_t StatusMonitor::context_bytes() const {
    return m_context_bytes.load();
}

// ---------------------------------------------------------------------------
// Background poll loop — sleeps in 50 ms chunks so stop() is responsive
// ---------------------------------------------------------------------------
void StatusMonitor::poll_loop() {
    constexpr int CHUNK_MS = 50;
    int elapsed_ms = 0;
    while (m_running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(CHUNK_MS));
        elapsed_ms += CHUNK_MS;
        if (!m_running.load()) break;
        if (elapsed_ms >= POLL_INTERVAL_MS) {
            elapsed_ms = 0;
            check_and_notify();
        }
    }
}

// ---------------------------------------------------------------------------
// Core check — parse state, fire callback if status or text changed.
// Lock is released BEFORE invoking the callback to prevent deadlock when the
// callback's thread tries to call public getters.
// ---------------------------------------------------------------------------
void StatusMonitor::check_and_notify() {
    CopilotStatus new_status = CopilotStatus::IDLE;
    std::string   new_text;
    bool          changed = false;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        CopilotStatus prev_status = m_status.load();
        std::string   prev_text   = m_status_text;

        std::string session = find_active_session();
        if (!session.empty()) {
            parse_status(session);
        } else {
            m_status.store(CopilotStatus::IDLE);
            m_status_text.clear();
            m_context_bytes.store(0);
        }

        new_status = m_status.load();
        new_text   = m_status_text;
        changed    = (new_status != prev_status || new_text != prev_text);
    }

    if (changed && m_on_change) {
        try {
            m_on_change(new_status, new_text);
        } catch (const std::exception& e) {
            std::cerr << "[StatusMonitor] Callback error: " << e.what() << "\n";
        }
    }
}

// ---------------------------------------------------------------------------
// find_active_session — returns path of the subdir that:
//   • is a directory
//   • contains at least one inuse.*.lock file
//   • contains events.jsonl
//   • has the most-recent events.jsonl mtime
// Called while m_mutex is already held by check_and_notify().
// ---------------------------------------------------------------------------
std::string StatusMonitor::find_active_session() const {
    std::string best;
    std::filesystem::file_time_type best_mtime{};

    try {
        if (!fs::exists(m_state_dir)) return {};

        for (const auto& entry : fs::directory_iterator(m_state_dir)) {
            try {
                if (!entry.is_directory()) continue;

                // Look for inuse.*.lock
                bool has_lock = false;
                for (const auto& sub : fs::directory_iterator(entry.path())) {
                    const std::string fname = sub.path().filename().string();
                    // Pattern: "inuse." + at least one char + ".lock"
                    if (fname.size() >= 12 &&
                        fname.rfind("inuse.", 0) == 0 &&
                        fname.size() > 6 + 5 &&
                        fname.substr(fname.size() - 5) == ".lock") {
                        has_lock = true;
                        break;
                    }
                }
                if (!has_lock) continue;

                fs::path events_file = entry.path() / "events.jsonl";
                if (!fs::exists(events_file)) continue;

                auto mtime = fs::last_write_time(events_file);
                if (best.empty() || mtime > best_mtime) {
                    best_mtime = mtime;
                    best = entry.path().string();
                }
            } catch (...) { continue; }
        }
    } catch (...) { return {}; }

    return best;
}

// ---------------------------------------------------------------------------
// parse_status — tail-reads the last TAIL_READ_BYTES of events.jsonl and
// walks lines in reverse to determine status, intent text, and model name.
// Called while m_mutex is already held.
// ---------------------------------------------------------------------------
void StatusMonitor::parse_status(const std::string& session_dir) {
    fs::path events_file = fs::path(session_dir) / "events.jsonl";

    try {
        auto file_size = fs::file_size(events_file);
        m_context_bytes.store(file_size);

        if (file_size == 0) {
            m_status.store(CopilotStatus::IDLE);
            m_status_text.clear();
            return;
        }

        // Read last TAIL_READ_BYTES
        std::ifstream fh(events_file, std::ios::binary);
        if (!fh.is_open()) return;

        size_t seek_pos = (file_size > TAIL_READ_BYTES)
                          ? static_cast<size_t>(file_size - TAIL_READ_BYTES) : 0;
        fh.seekg(static_cast<std::streamoff>(seek_pos));

        std::string tail((std::istreambuf_iterator<char>(fh)),
                          std::istreambuf_iterator<char>());
        fh.close();

        // Split into lines
        std::vector<std::string> lines;
        {
            std::istringstream ss(tail);
            std::string line;
            while (std::getline(ss, line)) {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                lines.push_back(std::move(line));
            }
        }

        // Discard partial leading line when we seeked into the middle
        if (seek_pos > 0 && !lines.empty())
            lines.erase(lines.begin());

        CopilotStatus status      = CopilotStatus::IDLE;
        std::string   status_text;
        bool          found_status = false;
        bool          found_text   = false;

        for (auto it = lines.rbegin(); it != lines.rend(); ++it) {
            const std::string& line = *it;
            if (line.empty()) continue;

            json event;
            try { event = json::parse(line); } catch (...) { continue; }

            const std::string event_type = event.value("type", std::string{});
            const json        data       = event.value("data", json::object());

            if (!found_status) {
                auto sit = STATUS_MAP.find(event_type);
                if (sit != STATUS_MAP.end()) {
                    status = sit->second;
                    found_status = true;
                }
            }

            if (!found_text && event_type == "tool.execution_start") {
                if (data.value("toolName", std::string{}) == "report_intent") {
                    auto args = data.value("arguments", json::object());
                    std::string intent = args.value("intent", std::string{});
                    if (!intent.empty()) {
                        status_text = std::move(intent);
                        found_text  = true;
                    }
                }
            }

            if (m_model_name.empty()) {
                if (event_type == "session.start") {
                    m_model_name = data.value("selectedModel", std::string{});
                } else if (event_type == "tool.execution_complete") {
                    std::string model = data.value("model", std::string{});
                    if (!model.empty()) m_model_name = std::move(model);
                }
            }

            if (found_status && found_text && !m_model_name.empty()) break;
        }

        m_status.store(status);
        m_status_text = std::move(status_text);

        if (m_model_name.empty())
            scan_for_model(events_file.string());

    } catch (const std::exception& e) {
        std::cerr << "[StatusMonitor] parse_status error: " << e.what() << "\n";
    } catch (...) {}
}

// ---------------------------------------------------------------------------
// scan_for_model — reads from the top of events.jsonl (up to 4 KB) looking
// for a session.start event that carries selectedModel.
// Called while m_mutex is already held.
// ---------------------------------------------------------------------------
void StatusMonitor::scan_for_model(const std::string& events_path) {
    try {
        std::ifstream fh(events_path);
        if (!fh.is_open()) return;

        std::string line;
        while (std::getline(fh, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;

            json event;
            try { event = json::parse(line); } catch (...) { continue; }

            if (event.value("type", std::string{}) == "session.start") {
                auto data  = event.value("data", json::object());
                std::string model = data.value("selectedModel", std::string{});
                if (!model.empty()) m_model_name = std::move(model);
                return;
            }

            if (fh.tellg() > 4096) return;
        }
    } catch (...) {}
}
