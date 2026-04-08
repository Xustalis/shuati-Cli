#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include <ftxui/component/screen_interactive.hpp>

namespace shuati {
namespace tui {

/// Thread-safe UTF-8 stream buffer.
/// Accumulates chunks, trims incomplete multi-byte sequences at the tail,
/// and flushes at a configurable interval.
class Utf8StreamBuffer {
public:
    explicit Utf8StreamBuffer(int flush_interval_ms = 80)
        : flush_interval_ms_(flush_interval_ms) {}

    /// Append raw bytes. Returns non-empty string when ready to flush.
    std::string append(const std::string& chunk);

    /// Force-flush everything remaining (call at end of stream).
    std::string flush_all();

private:
    std::string buf_;
    std::chrono::steady_clock::time_point last_flush_ =
        std::chrono::steady_clock::now();
    int flush_interval_ms_;

    /// Returns the number of valid UTF-8 bytes (trims incomplete trailing seq).
    static int valid_utf8_prefix_len(const std::string& s);
};

/// Unified asynchronous task executor for the TUI.
///
/// Replaces all manual std::thread + mutex + done_flag patterns with a single,
/// well-tested abstraction.  All callbacks are dispatched to the FTXUI main
/// thread via screen.Post(), eliminating cross-thread state mutation.
///
/// Usage:
///   runner.submit("hint", work_fn, on_chunk, on_done, on_error);
///   runner.cancel();            // user pressed Esc
///   runner.shutdown();          // TUI exiting
class TaskRunner {
public:
    using StreamCb = std::function<void(const std::string&)>;
    using WorkFn   = std::function<void(StreamCb emit,
                                        std::atomic_bool& cancel)>;

    explicit TaskRunner(ftxui::ScreenInteractive& screen);
    ~TaskRunner();

    TaskRunner(const TaskRunner&) = delete;
    TaskRunner& operator=(const TaskRunner&) = delete;

    /// Submit async work.  If a task is already running, it is cancelled first.
    ///
    /// @param name       Human-readable task name (for logging).
    /// @param work       The work function, runs on a background thread.
    ///                   `emit` sends UTF-8 text to the UI (buffered).
    ///                   `cancel` is set to true when the user requests abort.
    /// @param on_chunk   Called on the MAIN thread with buffered output.
    /// @param on_done    Called on the MAIN thread when the task finishes.
    /// @param on_error   Called on the MAIN thread if the task throws.
    void submit(std::string name,
                WorkFn work,
                StreamCb on_chunk,
                std::function<void(bool cancelled)> on_done,
                std::function<void(std::string)> on_error);

    /// Cancel the current task (sets cancel flag; non-blocking).
    void cancel();

    /// @return shared cancel flag for the current task (may be null).
    std::shared_ptr<std::atomic_bool> cancel_flag() const;

    /// @return true if a task is currently executing.
    bool is_busy() const { return busy_ ? busy_->load() : false; }

    /// Orderly shutdown: cancel + join with timeout.
    void shutdown();

private:
    void join_previous();

    ftxui::ScreenInteractive& screen_;
    std::shared_ptr<std::atomic_bool> alive_;

    std::thread worker_;
    std::shared_ptr<std::atomic_bool> cancel_;
    std::shared_ptr<std::atomic_bool> busy_;
    mutable std::mutex mu_;
};

}  // namespace tui
}  // namespace shuati
