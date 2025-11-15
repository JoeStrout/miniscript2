// This header defines the Math class, which is our equivalent of
// the C# Math class, and is used with C++ code transpiled from C#.

#pragma once
#include <algorithm>
#include <cstdint>

// This module is part of Layer 3B (Host C# Compatibility Layer)
#define CORE_LAYER_3B

// Static Math class - equivalent to C# Math
class Math {
public:
    // Min functions - equivalent to C# Math.Min
    static int32_t Min(int32_t a, int32_t b) {
        return std::min(a, b);
    }

    static int64_t Min(int64_t a, int64_t b) {
        return std::min(a, b);
    }

    static float Min(float a, float b) {
        return std::min(a, b);
    }

    static double Min(double a, double b) {
        return std::min(a, b);
    }

    // Max functions - equivalent to C# Math.Max
    static int32_t Max(int32_t a, int32_t b) {
        return std::max(a, b);
    }

    static int64_t Max(int64_t a, int64_t b) {
        return std::max(a, b);
    }

    static float Max(float a, float b) {
        return std::max(a, b);
    }

    static double Max(double a, double b) {
        return std::max(a, b);
    }

    // Additional common math functions can be added here as needed
    // static double Abs(double x) { return std::abs(x); }
    // static double Floor(double x) { return std::floor(x); }
    // static double Ceil(double x) { return std::ceil(x); }
    // etc.

private:
    // Private constructor to prevent instantiation (static class)
    Math() = delete;
    Math(const Math&) = delete;
    Math& operator=(const Math&) = delete;
};
