#pragma once

#include "shuati/database.hpp"

namespace shuati {

/**
 * @brief SM-2 (SuperMemo 2) Spaced Repetition Algorithm
 * 
 * Implements the classic SM-2 algorithm for optimal review scheduling.
 * Quality ranges from 0 (complete blackout) to 5 (perfect recall).
 */
class SM2Algorithm {
public:
    /**
     * @brief Update review schedule based on quality assessment
     * @param item Current review item state
     * @param quality Self-assessment (0-5): 0=blackout, 3=correct with hesitation, 5=perfect
     * @return Updated review item with new interval and ease factor
     */
    static ReviewItem update(ReviewItem item, int quality);

private:
    static constexpr double MIN_EASE_FACTOR = 1.3;
    static constexpr int ONE_DAY_SECONDS = 86400;
};

} // namespace shuati
