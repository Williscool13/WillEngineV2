//
// Created by William on 2024-08-18.
//

#ifndef MATH_CONSTANTS_H
#define MATH_CONSTANTS_H

namespace math_constants {
    // Basic mathematical constants
    constexpr double PI = 3.14159265358979323846;
    constexpr double TAU = 2.0 * PI;
    constexpr double E = 2.71828182845904523536;

    // Angles
    constexpr double DEGREES_TO_RADIANS = PI / 180.0;
    constexpr double RADIANS_TO_DEGREES = 180.0 / PI;

    // Common angles
    constexpr double HALF_PI = PI / 2.0;
    constexpr double QUARTER_PI = PI / 4.0;

    // Square roots
    constexpr double SQRT_2 = 1.41421356237309504880;
    constexpr double SQRT_3 = 1.73205080756887729352;

    // Golden ratio
    constexpr double PHI = 1.61803398874989484820;

    // Useful for color calculations
    constexpr double ONE_OVER_255 = 1.0 / 255.0;

    // For normalization of 16-bit values
    constexpr double ONE_OVER_65535 = 1.0 / 65535.0;

    // Epsilon (small value for float comparisons)
    constexpr double EPSILON = 1e-6;

    // Maximum and minimum values for common types
    constexpr int INT_MAX = 2147483647;
    constexpr int INT_MIN = -2147483648;
    constexpr float FLOAT_MAX = 3.402823466e+38F;
    constexpr float FLOAT_MIN = 1.175494351e-38F;
}

#endif //MATH_CONSTANTS_H
