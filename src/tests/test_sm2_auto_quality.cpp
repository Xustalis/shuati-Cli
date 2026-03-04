#include <iostream>
#include <cassert>
#include <cmath>
#include <string>
#include "shuati/sm2_algorithm.hpp"

using namespace shuati;

void test_auto_quality_ac_fast() {
    // AC with time_ratio <= 0.3 => quality 5
    int q = SM2Algorithm::auto_quality("AC", 300, 2000);
    assert(q == 5);
    std::cout << "  [PASS] AC fast (300/2000) => quality " << q << std::endl;
}

void test_auto_quality_ac_medium() {
    // AC with time_ratio <= 0.7 => quality 4
    int q = SM2Algorithm::auto_quality("AC", 1200, 2000);
    assert(q == 4);
    std::cout << "  [PASS] AC medium (1200/2000) => quality " << q << std::endl;
}

void test_auto_quality_ac_slow() {
    // AC with time_ratio > 0.7 => quality 3
    int q = SM2Algorithm::auto_quality("AC", 1800, 2000);
    assert(q == 3);
    std::cout << "  [PASS] AC slow (1800/2000) => quality " << q << std::endl;
}

void test_auto_quality_ac_no_limit() {
    // AC with no time limit info => quality 4
    int q = SM2Algorithm::auto_quality("AC", 500, 0);
    assert(q == 4);
    std::cout << "  [PASS] AC no limit => quality " << q << std::endl;
}

void test_auto_quality_wa() {
    int q = SM2Algorithm::auto_quality("WA", 100, 2000);
    assert(q == 2);
    std::cout << "  [PASS] WA => quality " << q << std::endl;
}

void test_auto_quality_tle() {
    int q = SM2Algorithm::auto_quality("TLE", 2100, 2000);
    assert(q == 1);
    std::cout << "  [PASS] TLE => quality " << q << std::endl;
}

void test_auto_quality_re() {
    int q = SM2Algorithm::auto_quality("RE", 50, 2000);
    assert(q == 0);
    std::cout << "  [PASS] RE => quality " << q << std::endl;
}

void test_auto_quality_ce() {
    int q = SM2Algorithm::auto_quality("CE", 0, 2000);
    assert(q == 0);
    std::cout << "  [PASS] CE => quality " << q << std::endl;
}

void test_auto_quality_mle() {
    int q = SM2Algorithm::auto_quality("MLE", 500, 2000);
    assert(q == 0);
    std::cout << "  [PASS] MLE => quality " << q << std::endl;
}

void test_sm2_update_integration() {
    // Verify auto_quality result works with SM2 update
    ReviewItem item;
    item.problem_id = "test_problem";
    item.interval = 1;
    item.ease_factor = 2.5;
    item.repetitions = 0;

    int quality = SM2Algorithm::auto_quality("AC", 200, 2000); // => 5
    assert(quality == 5);
    
    auto updated = SM2Algorithm::update(item, quality);
    assert(updated.repetitions == 1);
    assert(updated.interval == 1); // First rep
    assert(updated.ease_factor > 2.5); // Should increase with quality 5
    
    std::cout << "  [PASS] SM2 integration: interval=" << updated.interval 
              << ", ef=" << updated.ease_factor << std::endl;
}

int main() {
    std::cout << "=== SM2 Auto Quality Tests ===" << std::endl;
    
    test_auto_quality_ac_fast();
    test_auto_quality_ac_medium();
    test_auto_quality_ac_slow();
    test_auto_quality_ac_no_limit();
    test_auto_quality_wa();
    test_auto_quality_tle();
    test_auto_quality_re();
    test_auto_quality_ce();
    test_auto_quality_mle();
    test_sm2_update_integration();
    
    std::cout << "\nAll SM2 auto quality tests passed!" << std::endl;
    return 0;
}
