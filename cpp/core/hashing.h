// Hashing functions used by the MiniScript runtime (or host).

#ifndef HASHING_H
#define HASHING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// This file is part of Layer 0 (foundation utilities)
#define CORE_LAYER_0


extern uint32_t string_hash(const char* data, int len);

static inline uint32_t uint64_hash(uint64_t value) {
	// For now: treat value as 8 bytes and call string_hash.
	// ToDo: Find some more efficient uint64_t hasher.
    return string_hash((const char*)&value, sizeof(uint64_t));
}



#ifdef __cplusplus
}
#endif

#endif
