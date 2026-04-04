#include <gtest/gtest.h>
#include "event_parser.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// RAII temporary directory
// ---------------------------------------------------------------------------
struct TempDir {
    fs::path path;

    TempDir() {
        auto ts = std::chrono::steady_clock::now().time_since_epoch().count();
        path = fs::temp_directory_path()
             / ("copilot-buddy-test-" + std::to_string(ts));
        fs::create_directories(path);
    }

    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static void make_session(const fs::path& state_dir, const std::string& name,
                          bool with_lock, bool with_events) {
    fs::path session = state_dir / name;
    fs::create_directories(session);
    if (with_lock) {
        std::ofstream(session / "inuse.pid123.lock").close();
    }
    if (with_events) {
        std::ofstream(session / "events.jsonl").close();
    }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------
TEST(FindActiveSession, NonexistentDir) {
    EXPECT_TRUE(find_active_session("/nonexistent/path/copilot-buddy-xyz").empty());
}

TEST(FindActiveSession, EmptyDir) {
    TempDir tmp;
    EXPECT_TRUE(find_active_session(tmp.path.string()).empty());
}

TEST(FindActiveSession, SessionWithNoLockFile) {
    TempDir tmp;
    make_session(tmp.path, "sess1", /*with_lock=*/false, /*with_events=*/true);
    EXPECT_TRUE(find_active_session(tmp.path.string()).empty());
}

TEST(FindActiveSession, SessionWithNoEventsFile) {
    TempDir tmp;
    make_session(tmp.path, "sess1", /*with_lock=*/true, /*with_events=*/false);
    EXPECT_TRUE(find_active_session(tmp.path.string()).empty());
}

TEST(FindActiveSession, ValidSession) {
    TempDir tmp;
    make_session(tmp.path, "sess1", /*with_lock=*/true, /*with_events=*/true);
    std::string result = find_active_session(tmp.path.string());
    EXPECT_FALSE(result.empty());
    EXPECT_NE(result.find("sess1"), std::string::npos);
}

TEST(FindActiveSession, SkipsRegularFilesInStateDir) {
    TempDir tmp;
    // Regular file in state dir — should not be treated as a session
    std::ofstream(tmp.path / "not-a-dir.txt").close();
    EXPECT_TRUE(find_active_session(tmp.path.string()).empty());
}

TEST(FindActiveSession, ReturnsSessionOverSessionWithoutLock) {
    TempDir tmp;
    make_session(tmp.path, "no-lock",   /*with_lock=*/false, /*with_events=*/true);
    make_session(tmp.path, "with-lock", /*with_lock=*/true,  /*with_events=*/true);
    std::string result = find_active_session(tmp.path.string());
    EXPECT_NE(result.find("with-lock"), std::string::npos);
}

TEST(FindActiveSession, PicksMostRecentByMtime) {
    TempDir tmp;
    make_session(tmp.path, "older", /*with_lock=*/true, /*with_events=*/true);

    // Small sleep to guarantee a different filesystem mtime for "newer"
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    make_session(tmp.path, "newer", /*with_lock=*/true, /*with_events=*/true);

    // Touch newer's events.jsonl to ensure it has a later mtime
    { std::ofstream f(tmp.path / "newer" / "events.jsonl"); f << "\n"; }

    std::string result = find_active_session(tmp.path.string());
    EXPECT_NE(result.find("newer"), std::string::npos);
}

TEST(FindActiveSession, LockFileNameTooShortIsIgnored) {
    TempDir tmp;
    fs::path session = tmp.path / "sess1";
    fs::create_directories(session);
    // "inuse.lock" is exactly 10 chars — too short (needs >= 12)
    std::ofstream(session / "inuse.lock").close();
    std::ofstream(session / "events.jsonl").close();
    EXPECT_TRUE(find_active_session(tmp.path.string()).empty());
}
