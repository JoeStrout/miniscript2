// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: PRNG.cs

#include "PRNG.g.h"
#include <chrono>

namespace MiniScript {

UInt64 PRNG::_s0 = 0;
UInt64 PRNG::_s1 = 0;
UInt64 PRNG::_s2 = 0;
UInt64 PRNG::_s3 = 0;
Boolean PRNG::_seeded = Boolean(false);
void PRNG::Seed(UInt64 seed) {
	UInt64 x = seed;
	x = x + 0x9E3779B97F4A7C15UL; _s0 = SplitMix64(x);
	x = x + 0x9E3779B97F4A7C15UL; _s1 = SplitMix64(x);
	x = x + 0x9E3779B97F4A7C15UL; _s2 = SplitMix64(x);
	x = x + 0x9E3779B97F4A7C15UL; _s3 = SplitMix64(x);
	_seeded = Boolean(true);
}
Double PRNG::Next() {
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
UInt64 PRNG::Rotl(UInt64 x,Int32 k) {
	return (x << k) | (x >> (64 - k));
}
UInt64 PRNG::SplitMix64(UInt64 z) {
	z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9UL;
	z = (z ^ (z >> 27)) * 0x94D049BB133111EBUL;
	return z ^ (z >> 31);
}
UInt64 PRNG::DefaultSeed() {
	return (UInt64)std::chrono::high_resolution_clock::now().time_since_epoch().count();
}

} // end of namespace MiniScript
