#pragma once

#include <string>
#include <functional>

namespace shuati {

/**
 * StreamFilter: Robust filter for AI streaming output.
 *
 * Detects and suppresses internal system operation tags like:
 *   <!-- SYSTEM_OP: ... -->
 *
 * Uses a state machine approach instead of fragile fixed-size buffer logic.
 * Feed chunks via feed(), get safe-to-print text back.
 */
class StreamFilter {
public:
    using OutputCallback = std::function<void(const std::string&)>;

    explicit StreamFilter(OutputCallback on_output)
        : on_output_(std::move(on_output)), suppressed_(false) {}

    /**
     * Feed a chunk of streaming text. Safe content is emitted via the callback.
     */
    void feed(const std::string& chunk) {
        if (suppressed_) return;

        buffer_ += chunk;

        // Scan for the start of a system operation marker
        const std::string marker_start = "<!-- SYSTEM_OP";
        size_t marker_pos = buffer_.find(marker_start);

        if (marker_pos != std::string::npos) {
            // Emit everything before the marker
            if (marker_pos > 0) {
                on_output_(buffer_.substr(0, marker_pos));
            }
            // Suppress everything from here on (marker + rest is internal data)
            suppressed_ = true;
            // Save the tail for potential extraction of the JSON payload by caller
            tail_ = buffer_.substr(marker_pos);
            buffer_.clear();
            return;
        }

        // No marker found yet. Retain a safety tail to handle split markers.
        // The marker_start is 14 chars, so keep at least that many in buffer.
        const size_t SAFETY_TAIL = 20;
        if (buffer_.size() > SAFETY_TAIL) {
            size_t emit_len = buffer_.size() - SAFETY_TAIL;
            on_output_(buffer_.substr(0, emit_len));
            buffer_.erase(0, emit_len);
        }
    }

    /**
     * Flush remaining buffer. Call after streaming is complete.
     * Only emits if no marker was found in the remaining content.
     */
    void flush() {
        if (!suppressed_ && !buffer_.empty()) {
            // Final check: don't emit if buffer looks like a partial marker
            if (buffer_.find("<!--") == std::string::npos) {
                on_output_(buffer_);
            }
            buffer_.clear();
        }
    }

    /**
     * Returns the tail content captured after the SYSTEM_OP marker.
     * Useful for extracting the JSON payload for memory operations.
     */
    const std::string& captured_tail() const { return tail_; }

    /**
     * Whether the stream was suppressed (marker was found).
     */
    bool is_suppressed() const { return suppressed_; }

private:
    OutputCallback on_output_;
    std::string buffer_;
    std::string tail_;
    bool suppressed_;
};

} // namespace shuati
