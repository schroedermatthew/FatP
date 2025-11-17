#include "/mnt/project/CheckedArithmetic.h"
#include <iostream>
#include <cassert>
#include <cmath>

using namespace cpp_utilities;

void test_vector_operations() {
    std::cout << "Testing vector operations..." << std::endl;
    
    std::vector<int32_t> a = {1, 2, 3, 4};
    std::vector<int32_t> b = {5, 6, 7, 8};
    
    auto add_result = checked_add_vec<ThrowOnErrorPolicy>(a, b);
    assert(add_result[0] == 6);
    assert(add_result[1] == 8);
    
    auto sub_result = checked_sub_vec<ThrowOnErrorPolicy>(a, b);
    assert(sub_result[0] == -4);
    
    auto mul_result = checked_mul_vec<ThrowOnErrorPolicy>(a, b);
    assert(mul_result[0] == 5);
    assert(mul_result[1] == 12);
    
    auto div_result = checked_div_vec<ThrowOnErrorPolicy>(b, a);
    assert(div_result[0] == 5);
    assert(div_result[1] == 3);
    
    std::vector<double> fa = {1.5, 2.5, 3.5, 4.5};
    std::vector<double> fb = {0.5, 0.5, 0.5, 0.5};
    
    auto fadd_result = checked_add_vec_fp<ThrowOnErrorPolicy>(fa, fb);
    assert(std::abs(fadd_result[0] - 2.0) < 0.001);
    
    auto fsub_result = checked_sub_vec_fp<ThrowOnErrorPolicy>(fa, fb);
    assert(std::abs(fsub_result[0] - 1.0) < 0.001);
    
    auto fmul_result = checked_mul_vec_fp<ThrowOnErrorPolicy>(fa, fb);
    assert(std::abs(fmul_result[0] - 0.75) < 0.001);
    
    auto fdiv_result = checked_div_vec_fp<ThrowOnErrorPolicy>(fa, fb);
    assert(std::abs(fdiv_result[0] - 3.0) < 0.001);
    
    std::cout << "  Vector operations passed!" << std::endl;
}

void test_unary_operations() {
    std::cout << "Testing unary operations..." << std::endl;
    
    assert(checked_neg<ThrowOnErrorPolicy>(5) == -5);
    assert(checked_neg<ThrowOnErrorPolicy>(-5) == 5);
    
    auto neg_max = checked_neg<ReturnExpectedPolicy>(std::numeric_limits<int32_t>::min());
    assert(!neg_max.has_value());
    assert(neg_max.error() == MathError::Overflow);
    
    assert(checked_abs<ThrowOnErrorPolicy>(5) == 5);
    assert(checked_abs<ThrowOnErrorPolicy>(-5) == 5);
    
    auto abs_min = checked_abs<ReturnExpectedPolicy>(std::numeric_limits<int32_t>::min());
    assert(!abs_min.has_value());
    
    assert(checked_neg_fp<ThrowOnErrorPolicy>(3.14) == -3.14);
    assert(checked_abs_fp<ThrowOnErrorPolicy>(-2.71) > 2.7 && checked_abs_fp<ThrowOnErrorPolicy>(-2.71) < 2.72);
    
    assert(checked_not<ThrowOnErrorPolicy>(0) == -1);
    assert(checked_not<ThrowOnErrorPolicy>(-1) == 0);
    
    std::cout << "  Unary operations passed!" << std::endl;
}

void test_advanced_math() {
    std::cout << "Testing advanced math operations..." << std::endl;
    
    assert(checked_pow<ThrowOnErrorPolicy>(2, 3) == 8);
    assert(checked_pow<ThrowOnErrorPolicy>(3, 4) == 81);
    assert(checked_pow<ThrowOnErrorPolicy>(5, 0) == 1);
    
    auto pow_result = checked_pow_fp<ThrowOnErrorPolicy>(2.0, 3.0);
    assert(std::abs(pow_result - 8.0) < 0.001);
    
    auto sqrt_result = checked_sqrt_fp<ThrowOnErrorPolicy>(16.0);
    assert(std::abs(sqrt_result - 4.0) < 0.001);
    
    auto sqrt_neg = checked_sqrt_fp<ReturnExpectedPolicy>(-1.0);
    assert(!sqrt_neg.has_value());
    assert(sqrt_neg.error() == MathError::InvalidArgument);
    
    auto fma_result = checked_fma_fp<ThrowOnErrorPolicy>(2.0, 3.0, 4.0);
    assert(std::abs(fma_result - 10.0) < 0.001);
    
    std::cout << "  Advanced math passed!" << std::endl;
}

void test_type_conversion() {
    std::cout << "Testing type conversions..." << std::endl;
    
    auto i32 = checked_cast<int32_t, int64_t, ThrowOnErrorPolicy>(100);
    assert(i32 == 100);
    
    auto overflow = checked_cast<int8_t, int32_t, ReturnExpectedPolicy>(1000);
    assert(!overflow.has_value());
    assert(overflow.error() == MathError::Overflow);
    
    auto saturate = checked_cast<int8_t, int32_t, SaturatingPolicy>(1000);
    assert(saturate == 127);
    
    auto float_to_int = checked_cast<int32_t, double, ThrowOnErrorPolicy>(42.7);
    assert(float_to_int == 42);
    
    std::cout << "  Type conversions passed!" << std::endl;
}

void test_range_operations() {
    std::cout << "Testing range operations..." << std::endl;
    
    assert(checked_clamp<ThrowOnErrorPolicy>(5, 0, 10) == 5);
    assert(checked_clamp<ThrowOnErrorPolicy>(-5, 0, 10) == 0);
    assert(checked_clamp<ThrowOnErrorPolicy>(15, 0, 10) == 10);
    
    auto in_range = checked_in_range<ReturnExpectedPolicy>(5, 0, 10);
    assert(in_range.has_value());
    assert(*in_range == true);
    
    auto out_range = checked_in_range<ReturnExpectedPolicy>(15, 0, 10);
    assert(out_range.has_value());
    assert(*out_range == false);
    
    std::cout << "  Range operations passed!" << std::endl;
}

void test_fp_specific() {
    std::cout << "Testing FP-specific operations..." << std::endl;
    
    auto mod_result = checked_mod_fp<ThrowOnErrorPolicy>(7.5, 2.0);
    assert(std::abs(mod_result - 1.5) < 0.001);
    
    assert(checked_floor_fp<ThrowOnErrorPolicy>(3.7) == 3.0);
    assert(checked_floor_fp<ThrowOnErrorPolicy>(-3.7) == -4.0);
    
    assert(checked_ceil_fp<ThrowOnErrorPolicy>(3.2) == 4.0);
    assert(checked_ceil_fp<ThrowOnErrorPolicy>(-3.2) == -3.0);
    
    assert(checked_trunc_fp<ThrowOnErrorPolicy>(3.7) == 3.0);
    assert(checked_trunc_fp<ThrowOnErrorPolicy>(-3.7) == -3.0);
    
    assert(checked_round_fp<ThrowOnErrorPolicy>(3.4) == 3.0);
    assert(checked_round_fp<ThrowOnErrorPolicy>(3.6) == 4.0);
    
    std::cout << "  FP-specific operations passed!" << std::endl;
}

void test_existing_operations() {
    std::cout << "Testing existing operations..." << std::endl;
    
    assert(checked_add<ThrowOnErrorPolicy>(5, 3) == 8);
    assert(checked_sub<ThrowOnErrorPolicy>(5, 3) == 2);
    assert(checked_mul<ThrowOnErrorPolicy>(5, 3) == 15);
    assert(checked_div<ThrowOnErrorPolicy>(15, 3) == 5);
    assert(checked_mod<ThrowOnErrorPolicy>(17, 5) == 2);
    
    auto overflow_add = checked_add<ReturnExpectedPolicy>(
        std::numeric_limits<int32_t>::max(), 1);
    assert(!overflow_add.has_value());
    
    auto div_zero = checked_div<ReturnExpectedPolicy>(10, 0);
    assert(!div_zero.has_value());
    assert(div_zero.error() == MathError::DivByZero);
    
    assert(checked_add_fp<ThrowOnErrorPolicy>(1.5, 2.5) == 4.0);
    assert(checked_sub_fp<ThrowOnErrorPolicy>(5.5, 2.5) == 3.0);
    assert(checked_mul_fp<ThrowOnErrorPolicy>(2.5, 4.0) == 10.0);
    assert(checked_div_fp<ThrowOnErrorPolicy>(10.0, 2.0) == 5.0);
    
    assert(checked_and<ThrowOnErrorPolicy>(0b1010, 0b1100) == 0b1000);
    assert(checked_or<ThrowOnErrorPolicy>(0b1010, 0b1100) == 0b1110);
    assert(checked_xor<ThrowOnErrorPolicy>(0b1010, 0b1100) == 0b0110);
    
    assert(checked_left_shift<ThrowOnErrorPolicy>(1, 3) == 8);
    assert(checked_right_shift<ThrowOnErrorPolicy>(16, 2) == 4);
    
    assert(checked_inc<ThrowOnErrorPolicy>(5) == 6);
    assert(checked_dec<ThrowOnErrorPolicy>(5) == 4);
    
    std::cout << "  Existing operations passed!" << std::endl;
}

void test_policies() {
    std::cout << "Testing different policies..." << std::endl;
    
    try {
        checked_add<ThrowOnErrorPolicy>(std::numeric_limits<int32_t>::max(), 1);
        assert(false);
    } catch (const LogicContractError&) {
    }
    
    auto expected_result = checked_add<ReturnExpectedPolicy>(
        std::numeric_limits<int32_t>::max(), 1);
    assert(!expected_result.has_value());
    assert(expected_result.error() == MathError::Overflow);
    
    auto saturated = checked_add<SaturatingPolicy>(
        std::numeric_limits<int32_t>::max(), 1);
    assert(saturated == std::numeric_limits<int32_t>::max());
    
    std::cout << "  Policy tests passed!" << std::endl;
}

void test_constexpr_operations() {
    std::cout << "Testing compile-time operations..." << std::endl;
    
    constexpr auto add_result = static_math::add<int, 5, 3>();
    static_assert(add_result == 8, "static add failed");
    
    constexpr auto sub_result = static_math::sub<int, 10, 3>();
    static_assert(sub_result == 7, "static sub failed");
    
    constexpr auto mul_result = static_math::mul<int, 5, 3>();
    static_assert(mul_result == 15, "static mul failed");
    
    constexpr auto div_result = static_math::div<int, 15, 3>();
    static_assert(div_result == 5, "static div failed");
    
    constexpr auto mod_result = static_math::mod<int, 17, 5>();
    static_assert(mod_result == 2, "static mod failed");
    
    constexpr auto shift_left = static_math::left_shift<int, 1, 3>();
    static_assert(shift_left == 8, "static left shift failed");
    
    constexpr auto shift_right = static_math::right_shift<int, 16, 2>();
    static_assert(shift_right == 4, "static right shift failed");
    
    std::cout << "  Compile-time operations passed!" << std::endl;
}

int main() {
    std::cout << "Running comprehensive CheckedArithmetic tests..." << std::endl << std::endl;
    
    try {
        test_existing_operations();
        test_vector_operations();
        test_unary_operations();
        test_advanced_math();
        test_type_conversion();
        test_range_operations();
        test_fp_specific();
        test_policies();
        test_constexpr_operations();
        
        std::cout << std::endl << "ALL TESTS PASSED!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
