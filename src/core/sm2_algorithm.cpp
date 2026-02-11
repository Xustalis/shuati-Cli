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

} // namespace shuati
