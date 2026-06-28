#include "hashing.h"

#include "layer_defs.h"
#if LAYER_0_VIOLATIONS
#error "hashing.h (Layer 0) cannot depend on any higher layer"
#endif


// FNV-1a hash function for strings
// Returns 0 to indicate "not computed" is reserved, so we use 1 as minimum hash
uint32_t string_hash(const char* data, int len) {
	// Currently using FNV-1a.  ToDo: consider other hashing algorithms,
	// that maybe can use vector computation or something for speed.
    // FNV-1a constants
    const uint32_t FNV_PRIME = 0x01000193;
    const uint32_t FNV_OFFSET_BASIS = 0x811c9dc5;
    
    uint32_t hash = FNV_OFFSET_BASIS;
    for (int i = 0; i < len; i++) {
        hash ^= (unsigned char)data[i];
        hash *= FNV_PRIME;
    }
    
    // Ensure hash is never 0 (reserved for "not computed")
    return hash == 0 ? 1 : hash;
}