#include "status_monitor.h"
#include "event_parser.h"

// Prevent Windows SDK headers from redefining names that raylib already defines.
#ifdef _WIN32
// Forward-declare MSG/LPMSG so that headers pulled in transitively by
// windows.h (winscard.h → wtypes.h → ole2.h → oleidl.h) can reference
// LPMSG even though NOUSER has excluded winuser.h (which normally defines it).
// NOUSER is still required to prevent winuser.h from declaring
// ShowCursor(BOOL) / LoadImage() which conflict with the same-named raylib
// functions already declared via status_monitor.h → config.h → raylib.h.
struct tagMSG;
typedef struct tagMSG MSG;
typedef MSG *LPMSG;
#  define NOGDI   // excludes Rectangle and other GDI names
#  define NOUSER  // excludes ShowCursor, LoadImage and other USER names
#endif
#define DMON_IMPL
#include <dmon/dmon.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <chrono>
#include <thread>
#include <cstdlib>

namespace fs = std::filesystem;
using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Event-type → status mapping (mirrors Python _STATUS_MAP)
// NOTE: STATUS_MAP and parse logic live in event_parser.cpp.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// dmon file-watch callback — called from dmon's background thread.
// Routes .jsonl and .lock change events to the monitor, with a 150 ms debounce.
// ---------------------------------------------------------------------------
static void dmon_watch_cb(dmon_watch_id /*watch_id*/, dmon_action action,
                          const char* /*rootdir*/, const char* filepath,
                          const char* /*oldfilepath*/, void* user)
{
    // Ignore deletes — the poll loop handles session-end detection
    if (action == DMON_ACTION_DELETE) return;

    // filepath is relative to rootdir; extract just the filename portion
    const char* filename = filepath;
    for (const char* p = filepath; *p; ++p)
        if (*p == '/' || *p == '\\') filename = p + 1;

    auto ends_with = [](const char* s, const char* suffix) -> bool {
        size_t slen = strlen(s), sfxlen = strlen(suffix);
        if (slen < sfxlen) return false;
        return strcmp(s + slen - sfxlen, suffix) == 0;
    };

    if (!ends_with(filename, ".jsonl") && !ends_with(filename, ".lock")) return;

    static std::atomic<int64_t> s_last_ms{0};
    int64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    int64_t last = s_last_ms.load(std::memory_order_relaxed);
    if (now_ms - last < 150) return;
    if (!s_last_ms.compare_exchange_strong(last, now_ms, std::memory_order_relaxed))
        return;  // another thread won the debounce race

    auto* monitor = static_cast<StatusMonitor*>(user);
    monitor->check_and_notify();
}

// ---------------------------------------------------------------------------
// StatusMonitor
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Resolve the user's home directory portably.
// Windows: prefers USERPROFILE, falls back to HOMEDRIVE+HOMEPATH.
// Unix/macOS: uses HOME.
// ---------------------------------------------------------------------------
static std::string resolve_home_dir()
{
#ifdef _WIN32
    if (const char* up = std::getenv("USERPROFILE"); up && *up)
        return up;
    const char* hd = std::getenv("HOMEDRIVE");
    const char* hp = std::getenv("HOMEPATH");
    if (hd && *hd && hp && *hp)
        return std::string(hd) + hp;
#else
    if (const char* home = std::getenv("HOME"); home && *home)
        return home;
#endif
    return {};
}

StatusMonitor::StatusMonitor(Callback on_change)
    : m_on_change(std::move(on_change))
{
    std::string home_dir = resolve_home_dir();
    if (!home_dir.empty())
        m_state_dir = (fs::path(home_dir) / ".copilot" / "session-state").string();
}

StatusMonitor::~StatusMonitor() {
    stop();
}

void StatusMonitor::start() {
    if (m_running.exchange(true)) return; // already running

    check_and_notify();

    if (fs::exists(m_state_dir)) {
        dmon_init();
        m_dmon_initialized = true;
        dmon_watch_id id = dmon_watch(m_state_dir.c_str(), dmon_watch_cb,
                                      DMON_WATCHFLAGS_RECURSIVE, this);
        m_watch_id = id.id;
        if (m_watch_id == 0) {
            std::cerr << "[StatusMonitor] dmon_watch() failed; falling back to poll-only.\n";
        }
    } else {
        std::cerr << "[StatusMonitor] State dir not found: " << m_state_dir << "\n";
    }

    m_poll_thread = std::make_unique<std::thread>(&StatusMonitor::poll_loop, this);
}

void StatusMonitor::stop() {
    if (!m_running.exchange(false)) return; // already stopped

    if (m_watch_id != 0) {
        dmon_unwatch({m_watch_id});
        m_watch_id = 0;
    }
    if (m_dmon_initialized) {
        dmon_deinit();
        m_dmon_initialized = false;
    }

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

std::string StatusMonitor::idle_text() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_idle_text;
}

std::string StatusMonitor::model_name() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_model_name;
}

size_t StatusMonitor::context_bytes() const {
    return m_context_bytes.load();
}

size_t StatusMonitor::current_tokens() const {
    return m_current_tokens.load();
}

// ---------------------------------------------------------------------------
// Adaptive poll interval — fast when BUSY, slow otherwise
// ---------------------------------------------------------------------------
int StatusMonitor::current_poll_interval() const {
    return (m_status.load() == CopilotStatus::BUSY) ? POLL_BUSY_MS : POLL_IDLE_MS;
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
        if (elapsed_ms >= current_poll_interval()) {
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
    // Phase 1: Filesystem I/O without holding the mutex.
    // find_active_session() only reads the immutable m_state_dir.
    // parse_session() returns a value — no shared state is mutated.
    std::string session = find_active_session();
    ParsedState parsed;

    if (!session.empty()) {
        parsed = parse_session(session);

        // Update heartbeat timestamp when we successfully read a session
        m_last_event_time_ms.store(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count(),
            std::memory_order_relaxed);
    } else {
        // No active session — mark as DISCONNECTED
        parsed.status = CopilotStatus::DISCONNECTED;
        parsed.status_text.clear();
    }

    // Phase 2: Lock briefly to compare previous state and apply update.
    CopilotStatus new_status;
    std::string   new_text;
    bool          changed;
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        CopilotStatus prev_status = m_status.load();
        std::string   prev_text   = m_status_text;

        m_status.store(parsed.status);
        m_status_text = std::move(parsed.status_text);
        m_context_bytes.store(parsed.context_bytes);
        m_current_tokens.store(parsed.current_tokens);

        if (!session.empty()) {
            if (!parsed.idle_text.empty()) m_idle_text = std::move(parsed.idle_text);
            if (!parsed.model_name.empty()) m_model_name = std::move(parsed.model_name);
        } else {
            m_idle_text.clear();
        }

        // Heartbeat timeout: if BUSY for too long without new events, force IDLE
        if (m_status.load() == CopilotStatus::BUSY) {
            int64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            int64_t last = m_last_event_time_ms.load(std::memory_order_relaxed);
            if (last > 0 && (now_ms - last) > static_cast<int64_t>(HEARTBEAT_TIMEOUT_S) * 1000) {
                m_status.store(CopilotStatus::IDLE);
            }
        }

        new_status = m_status.load();
        new_text   = m_status_text;
        changed    = (new_status != prev_status || new_text != prev_text);
    }

    // Phase 3: Fire callback outside the lock to prevent deadlock.
    if (changed && m_on_change) {
        try {
            m_on_change(new_status, new_text);
        } catch (const std::exception& e) {
            std::cerr << "[StatusMonitor] Callback error: " << e.what() << "\n";
        }
    }
}

// ---------------------------------------------------------------------------
// find_active_session — delegates to the free function in event_parser.cpp.
// Safe to call without m_mutex: only reads the immutable m_state_dir.
// ---------------------------------------------------------------------------
std::string StatusMonitor::find_active_session() const {
    return ::find_active_session(m_state_dir);
}

// ---------------------------------------------------------------------------
// scan_for_model_from_top — reads the first 4 KB of events.jsonl looking for
// a session.start event that carries selectedModel.
// ---------------------------------------------------------------------------
static std::string scan_for_model_from_top(const std::string& events_path) {
    try {
        std::ifstream fh(events_path);
        if (!fh.is_open()) return {};

        std::string line;
        while (std::getline(fh, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;

            json event;
            try { event = json::parse(line); } catch (...) { continue; }

            if (event.value("type", std::string{}) == "session.start") {
                auto data  = event.value("data", json::object());
                std::string model = data.value("selectedModel", std::string{});
                if (!model.empty()) return model;
                return {};
            }

            if (fh.tellg() > 4096) return {};
        }
    } catch (...) {}
    return {};
}

// ---------------------------------------------------------------------------
// parse_session — tail-reads events.jsonl and derives the full session state.
// Pure function: reads from disk, returns a value, mutates no member state.
// ---------------------------------------------------------------------------
StatusMonitor::ParsedState StatusMonitor::parse_session(const std::string& session_dir) const {
    ParsedState state;
    fs::path events_file = fs::path(session_dir) / "events.jsonl";

    try {
        auto file_size = fs::file_size(events_file);
        state.context_bytes = file_size;

        if (file_size == 0) return state;

        // Read last TAIL_READ_BYTES
        std::ifstream fh(events_file, std::ios::binary);
        if (!fh.is_open()) return state;

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

        ParseResult pr = parse_events(lines);
        state.status        = pr.status;
        state.status_text   = std::move(pr.status_text);
        state.idle_text     = std::move(pr.idle_text);
        state.model_name    = std::move(pr.model_name);
        state.current_tokens = pr.current_tokens + pr.output_tokens_since;

        if (state.model_name.empty()) {
            state.model_name = scan_for_model_from_top(events_file.string());
        }
    } catch (const std::exception& e) {
        std::cerr << "[StatusMonitor] parse_session error: " << e.what() << "\n";
        state.status = CopilotStatus::DISCONNECTED;
    } catch (...) {
        state.status = CopilotStatus::DISCONNECTED;
    }

    return state;
}
