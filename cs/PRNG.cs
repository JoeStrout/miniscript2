// PRNG.cs - Standardized pseudo-random number generator.
// Algorithm: xoshiro256+ (https://prng.di.unimi.it/xoshiro256plus.c),
// seeded via splitmix64.  Identical UInt64 arithmetic in C# and C++ means
// a given seed yields the same sequence on every platform.

using System;
// CPP: #include <chrono>

namespace MiniScript {

public static class PRNG {

	private static UInt64 _s0 = 0;
	private static UInt64 _s1 = 0;
	private static UInt64 _s2 = 0;
	private static UInt64 _s3 = 0;
	private static Boolean _seeded = false;

	// Seed from a single 64-bit value, expanded to the full 256-bit
	// state with splitmix64 as recommended by the xoshiro authors.
	// (splitmix64 is a bijection, so the resulting state is never all-zero,
	// even for a seed of 0 -- so any seed value is valid.)
	public static void Seed(UInt64 seed) {
		UInt64 x = seed;
		x = x + 0x9E3779B97F4A7C15UL; _s0 = SplitMix64(x);
		x = x + 0x9E3779B97F4A7C15UL; _s1 = SplitMix64(x);
		x = x + 0x9E3779B97F4A7C15UL; _s2 = SplitMix64(x);
		x = x + 0x9E3779B97F4A7C15UL; _s3 = SplitMix64(x);
		_seeded = true;
	}

	// Next random double in [0.0, 1.0), advancing the generator.
	public static Double Next() {
		if (!_seeded) Seed(DefaultSeed());
		UInt64 result = _s0 + _s3;            // xoshiro256+ output
		UInt64 t = _s1 << 17;
		_s2 = _s2 ^ _s0;
		_s3 = _s3 ^ _s1;
		_s1 = _s1 ^ _s2;
		_s0 = _s0 ^ _s3;
		_s2 = _s2 ^ t;
		_s3 = Rotl(_s3, 45);
		// Use the top 53 bits for a double in [0,1):  (result >> 11) * 2^-53.
		return (Double)(result >> 11) * (1.0 / 9007199254740992.0);
	}

	private static UInt64 Rotl(UInt64 x, Int32 k) {
		return (x << k) | (x >> (64 - k));
	}

	private static UInt64 SplitMix64(UInt64 z) {
		z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9UL;
		z = (z ^ (z >> 27)) * 0x94D049BB133111EBUL;
		return z ^ (z >> 31);
	}

	// Non-deterministic seed, used only when the program never calls rnd(seed).
	private static UInt64 DefaultSeed() {
		//*** BEGIN CS_ONLY ***
		return (UInt64)DateTime.Now.Ticks;
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		return (UInt64)std::chrono::high_resolution_clock::now().time_since_epoch().count();
		*** END CPP_ONLY ***/
	}
}

}
