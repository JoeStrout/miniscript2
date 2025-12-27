// CS_List.cpp - Implementation file for CS_List.h

#include "CS_List.h"

#include "layer_defs.h"
#if LAYER_2B_HIGHER
#error "CS_List.cpp (Layer 2B) cannot depend on higher layers (3)"
#endif
#if LAYER_2B_ASIDE
#error "CS_List.cpp (Layer 2B - host) cannot depend on A-side layers (2A)"
#endif

// CS_List is header-only, so this file just enforces layer checks
