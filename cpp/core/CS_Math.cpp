// CS_Math.cpp - Implementation file for CS_Math.h

#include "CS_Math.h"

#include "layer_defs.h"
#if LAYER_2B_HIGHER
#error "CS_Math.cpp (Layer 2B) cannot depend on higher layers (3)"
#endif
#if LAYER_2B_ASIDE
#error "CS_Math.cpp (Layer 2B - host) cannot depend on A-side layers (2A)"
#endif

// CS_Math is header-only, so this file just enforces layer checks
