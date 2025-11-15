// CS_Math.cpp - Implementation file for CS_Math.h

#include "CS_Math.h"

#include "layer_defs.h"
#if LAYER_3B_HIGHER
#error "CS_Math.cpp (Layer 3B) cannot depend on higher layers (4)"
#endif
#if LAYER_3B_ASIDE
#error "CS_Math.cpp (Layer 3B - host) cannot depend on A-side layers (2A, 3A)"
#endif

// CS_Math is header-only, so this file just enforces layer checks
