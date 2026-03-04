#pragma once

#include <string>
#include <functional>
#include <vector>

namespace shuati {

/**
 * @brief XML-based streaming parser for structured AI output.
 *
 * Recognizes three semantic tags in AI streaming output:
 *   <cot>...</cot>        — Chain of Thought (hidden from user)
 *   <hint>...</hint>      — User-visible coaching content
 *   <memory_op>...</memory_op> — Internal memory operation JSON
 *
 * Uses a state machine to handle cross-chunk tag splitting safely.
 * Falls back to emitting raw text when no tags are detected.
 */
class XmlStreamParser {
public:
    enum class Tag { None, Cot, Hint, MemoryOp };

    struct Callbacks {
        std::function<void(const std::string&)> on_hint;       // User-visible content
        std::function<void(const std::string&)> on_cot;        // Hidden CoT content
        std::function<void(const std::string&)> on_memory_op;  // Memory operation JSON
        std::function<void(const std::string&)> on_raw;        // Untagged content (fallback)
    };

    explicit XmlStreamParser(Callbacks cb) : cb_(std::move(cb)) {}

    /**
     * @brief Feed a chunk of streaming text. Parsed segments are emitted via callbacks.
     */
    void feed(const std::string& chunk) {
        buffer_ += chunk;
        process();
    }

    /**
     * @brief Flush remaining buffer after streaming is complete.
     */
    void flush() {
        // If we're inside a tag that was never closed, emit what we have
        if (current_tag_ != Tag::None && !tag_content_.empty()) {
            emit_content(current_tag_, tag_content_);
            tag_content_.clear();
            current_tag_ = Tag::None;
        }
        // Emit any remaining buffer as raw
        if (!buffer_.empty()) {
            if (cb_.on_raw) cb_.on_raw(buffer_);
            buffer_.clear();
        }
    }

    /**
     * @brief Get accumulated content for a tag type (useful after flush).
     */
    Tag current_tag() const { return current_tag_; }

private:
    Callbacks cb_;
    std::string buffer_;
    std::string tag_content_;
    Tag current_tag_ = Tag::None;

    struct TagInfo {
        std::string open;
        std::string close;
        Tag tag;
    };

    static const std::vector<TagInfo>& tag_table() {
        static const std::vector<TagInfo> table = {
            {"<cot>",       "</cot>",       Tag::Cot},
            {"<hint>",      "</hint>",      Tag::Hint},
            {"<memory_op>", "</memory_op>", Tag::MemoryOp},
        };
        return table;
    }

    void process() {
        while (!buffer_.empty()) {
            if (current_tag_ == Tag::None) {
                // Look for any opening tag
                size_t earliest_pos = std::string::npos;
                const TagInfo* earliest_tag = nullptr;

                for (const auto& ti : tag_table()) {
                    size_t pos = buffer_.find(ti.open);
                    if (pos != std::string::npos && pos < earliest_pos) {
                        earliest_pos = pos;
                        earliest_tag = &ti;
                    }
                }

                if (earliest_tag) {
                    // Emit raw text before the tag
                    if (earliest_pos > 0) {
                        if (cb_.on_raw) cb_.on_raw(buffer_.substr(0, earliest_pos));
                    }
                    current_tag_ = earliest_tag->tag;
                    buffer_.erase(0, earliest_pos + earliest_tag->open.size());
                    tag_content_.clear();
                } else {
                    // No tag found. Check if buffer might contain a partial tag start.
                    // Keep last N chars as safety margin (longest open tag is "<memory_op>" = 12).
                    const size_t SAFETY = 12;
                    if (buffer_.size() > SAFETY) {
                        size_t emit_len = buffer_.size() - SAFETY;
                        if (cb_.on_raw) cb_.on_raw(buffer_.substr(0, emit_len));
                        buffer_.erase(0, emit_len);
                    }
                    break; // Need more data
                }
            } else {
                // Inside a tag — look for closing tag
                std::string close_tag;
                for (const auto& ti : tag_table()) {
                    if (ti.tag == current_tag_) {
                        close_tag = ti.close;
                        break;
                    }
                }

                size_t close_pos = buffer_.find(close_tag);
                if (close_pos != std::string::npos) {
                    tag_content_ += buffer_.substr(0, close_pos);
                    emit_content(current_tag_, tag_content_);
                    buffer_.erase(0, close_pos + close_tag.size());
                    tag_content_.clear();
                    current_tag_ = Tag::None;
                } else {
                    // Tag not closed yet, accumulate and wait
                    // Keep safety margin for partial close tag
                    const size_t SAFETY = 14; // longest close tag is "</memory_op>" = 13
                    if (buffer_.size() > SAFETY) {
                        size_t accum_len = buffer_.size() - SAFETY;
                        tag_content_ += buffer_.substr(0, accum_len);
                        buffer_.erase(0, accum_len);
                    }
                    break; // Need more data
                }
            }
        }
    }

    void emit_content(Tag tag, const std::string& content) {
        switch (tag) {
            case Tag::Hint:     if (cb_.on_hint)      cb_.on_hint(content);      break;
            case Tag::Cot:      if (cb_.on_cot)        cb_.on_cot(content);       break;
            case Tag::MemoryOp: if (cb_.on_memory_op)  cb_.on_memory_op(content); break;
            default: break;
        }
    }
};

} // namespace shuati
