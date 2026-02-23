// This header defines the Math class, which is our equivalent of
// the C# Math class, and is used with C++ code transpiled from C#.

#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

// This module is part of Layer 2B (Host C# Compatibility Layer)
#define CORE_LAYER_2B

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

	static double Round(double x) {
		return std::round(x);
	}
	
	static double Abs(double x) {
		return std::abs(x);
	}
	
	static double Floor(double x) {
		return std::floor(x);
	}
	
	static double Ceiling(double x) {
		return std::ceil(x);
	}
	
	static double Pow(double x, double y) {
		return std::pow(x, y);
	}
	
	static double Cos(double x) {
		return std::cos(x);
	}

	static double Sin(double x) {
		return std::sin(x);
	}

	static double Tan(double x) {
		return std::tan(x);
	}

	static double Acos(double x) {
		return std::acos(x);
	}

	static double Asin(double x) {
		return std::asin(x);
	}

	static double Atan(double x) {
		return std::atan(x);
	}

	static double Atan2(double y, double x) {
		return std::atan2(y, x);
	}

	static double Sqrt(double x) {
		return std::sqrt(x);
	}

	static double Log(double x) {
		return std::log(x);
	}

	static int Sign(double x) {
		if (x > 0) return 1;
		if (x < 0) return -1;
		return 0;
	}

	static double Round(double x, int decimals) {
		double factor = std::pow(10.0, decimals);
		return std::round(x * factor) / factor;
	}

	static constexpr double PI = 3.14159265358979323846;

    // Additional common math functions can be added here as needed

private:
    // Private constructor to prevent instantiation (static class)
    Math() = delete;
    Math(const Math&) = delete;
    Math& operator=(const Math&) = delete;
};
