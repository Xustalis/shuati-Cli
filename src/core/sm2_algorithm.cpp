#include "shuati/sm2_algorithm.hpp"
#include <algorithm>
#include <ctime>

namespace shuati {

ReviewItem SM2Algorithm::update(ReviewItem item, int quality) {
    // Clamp quality to valid range
    quality = std::clamp(quality, 0, 5);

    // Reset if quality < 3 (incorrect response)
    if (quality < 3) {
        item.repetitions = 0;
        item.interval = 1;
    } else {
        // Calculate new interval based on repetition count
        if (item.repetitions == 0) {
            item.interval = 1;
        } else if (item.repetitions == 1) {
            item.interval = 6;
        } else {
            item.interval = static_cast<int>(item.interval * item.ease_factor);
        }
        item.repetitions++;
    }

    // Update ease factor based on quality
    // Formula: EF' = EF + (0.1 - (5 - q) * (0.08 + (5 - q) * 0.02))
    item.ease_factor += 0.1 - (5 - quality) * (0.08 + (5 - quality) * 0.02);
    
    // Ensure ease factor doesn't drop below minimum
    if (item.ease_factor < MIN_EASE_FACTOR) {
        item.ease_factor = MIN_EASE_FACTOR;
    }

    // Calculate next review timestamp
    item.next_review = std::time(nullptr) + item.interval * ONE_DAY_SECONDS;

    return item;
}

int SM2Algorithm::auto_quality(const std::string& verdict, int time_ms, int time_limit_ms) {
    if (verdict == "AC") {
        if (time_limit_ms <= 0) return 4; // No time limit info, assume decent
        double ratio = static_cast<double>(time_ms) / time_limit_ms;
        if (ratio <= 0.3) return 5;      // Fast solve — excellent
        if (ratio <= 0.7) return 4;      // Moderate — good
        return 3;                        // Slow but correct — hesitant
    }
    if (verdict == "WA")  return 2;      // Wrong answer — significant difficulty
    if (verdict == "TLE") return 1;      // Time limit — serious issue
    // RE, CE, MLE, SE, etc.
    return 0;                            // Complete failure
}

} // namespace shuati
