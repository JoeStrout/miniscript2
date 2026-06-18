// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: PRNG.cs

#pragma once
#include "core_includes.h"
#include "forward_decs.g.h"
// PRNG.cs - Standardized pseudo-random number generator.
// Algorithm: xoshiro256+ (https://prng.di.unimi.it/xoshiro256plus.c),
// seeded via splitmix64.  Identical UInt64 arithmetic in C# and C++ means
// a given seed yields the same sequence on every platform.

namespace MiniScript {

// DECLARATIONS

class PRNG {
	private: static UInt64 _s0;
	private: static UInt64 _s1;
	private: static UInt64 _s2;
	private: static UInt64 _s3;
	private: static Boolean _seeded;

	// Seed from a single 64-bit value, expanded to the full 256-bit
	// state with splitmix64 as recommended by the xoshiro authors.
	// (splitmix64 is a bijection, so the resulting state is never all-zero,
	// even for a seed of 0 -- so any seed value is valid.)
	public: static void Seed(UInt64 seed);

	// Next random double in [0.0, 1.0), advancing the generator.
	public: static Double Next();

	private: static UInt64 Rotl(UInt64 x, Int32 k);

	private: static UInt64 SplitMix64(UInt64 z);

	// Non-deterministic seed, used only when the program never calls rnd(seed).
	private: static UInt64 DefaultSeed();
}; // end of struct PRNG

// INLINE METHODS

} // end of namespace MiniScript
