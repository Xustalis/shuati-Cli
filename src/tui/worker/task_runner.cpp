#include "task_runner.hpp"
#include <algorithm>

namespace shuati {
namespace tui {

// ─── Utf8StreamBuffer ─────────────────────────────────────────────

int Utf8StreamBuffer::valid_utf8_prefix_len(const std::string& s) {
    int len = static_cast<int>(s.size());
    if (len == 0) return 0;
    // Walk backwards from the end to find the start of the last char.
    int pos = len - 1;
    while (pos >= 0 && (static_cast<unsigned char>(s[pos]) & 0xC0) == 0x80)
        --pos;
    if (pos < 0) return 0;
    unsigned char lead = static_cast<unsigned char>(s[pos]);
    int expected = 1;
    if ((lead & 0xE0) == 0xC0) expected = 2;
    else if ((lead & 0xF0) == 0xE0) expected = 3;
    else if ((lead & 0xF8) == 0xF0) expected = 4;
    if (pos + expected > len) return pos;  // incomplete → trim
    return len;
}

std::string Utf8StreamBuffer::append(const std::string& chunk) {
    if (chunk.empty()) return {};
    buf_ += chunk;
    auto now = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now - last_flush_).count();
    if (ms < flush_interval_ms_) return {};
    last_flush_ = now;
    int valid = valid_utf8_prefix_len(buf_);
    if (valid <= 0) return {};
    std::string out = buf_.substr(0, valid);
    buf_ = buf_.substr(valid);
    return out;
}

std::string Utf8StreamBuffer::flush_all() {
    std::string out;
    std::swap(out, buf_);
    return out;
}

// ─── TaskRunner ───────────────────────────────────────────────────

TaskRunner::TaskRunner(ftxui::ScreenInteractive& screen)
    : screen_(screen),
      alive_(std::make_shared<std::atomic_bool>(true)),
      busy_(std::make_shared<std::atomic_bool>(false)) {}

TaskRunner::~TaskRunner() { shutdown(); }

void TaskRunner::join_previous() {
    if (worker_.joinable()) worker_.join();
}

void TaskRunner::cancel() {
    std::lock_guard<std::mutex> lk(mu_);
    if (cancel_) cancel_->store(true);
}

std::shared_ptr<std::atomic_bool> TaskRunner::cancel_flag() const {
    std::lock_guard<std::mutex> lk(mu_);
    return cancel_;
}

void TaskRunner::submit(
    std::string /*name*/,
    WorkFn work,
    StreamCb on_chunk,
    std::function<void(bool cancelled)> on_done,
    std::function<void(std::string)> on_error)
{
    // Cancel and join any existing task first.
    cancel();
    join_previous();

    auto cf = std::make_shared<std::atomic_bool>(false);
    {
        std::lock_guard<std::mutex> lk(mu_);
        cancel_ = cf;
    }
    busy_->store(true);

    auto alive = alive_;
    auto cancel = cf;
    auto busy_ptr = busy_;
    ftxui::ScreenInteractive* target_screen = &screen_;

    worker_ = std::thread([=]() {
        Utf8StreamBuffer utf8_buf(80);

        // Build a buffered emit function.
        auto emit = [&](const std::string& chunk) {
            if (!alive->load() || cancel->load()) return;
            auto ready = utf8_buf.append(chunk);
            if (ready.empty()) return;
            target_screen->Post([=]() {
                if (!alive->load()) return;
                on_chunk(ready);
            });
            target_screen->PostEvent(ftxui::Event::Custom);
        };

        // Run the work function.
        try {
            work(emit, *cancel);
        } catch (const std::exception& e) {
            if (alive->load() && !cancel->load()) {
                auto err = std::string(e.what());
                target_screen->Post([=]() {
                    if (!alive->load()) return;
                    on_error(err);
                });
            }
        } catch (...) {
            // Swallow unknown exceptions to prevent thread crash.
        }

        // Flush remaining UTF-8 buffer.
        bool was_cancelled = !alive->load() || cancel->load();
        auto remainder = utf8_buf.flush_all();

        target_screen->Post([=]() {
            if (!alive->load()) return;
            if (!remainder.empty() && !was_cancelled) {
                on_chunk(remainder);
            }
            on_done(was_cancelled);
            busy_ptr->store(false);
        });
    });
}

void TaskRunner::shutdown() {
    alive_->store(false);
    cancel();
    // Wait up to 2 seconds, then detach.
    if (worker_.joinable()) {
        auto deadline = std::chrono::steady_clock::now()
                      + std::chrono::seconds(2);
        // Spin-wait for the thread to finish.
        while (std::chrono::steady_clock::now() < deadline) {
            if (!busy_->load()) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        if (busy_->load()) {
            worker_.detach();
        } else {
            worker_.join();
        }
    }
}

}  // namespace tui
}  // namespace shuati
