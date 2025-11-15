// CS_List.cpp - Implementation file for CS_List.h

#include "CS_List.h"

#include "layer_defs.h"
#if LAYER_3B_HIGHER
#error "CS_List.cpp (Layer 3B) cannot depend on higher layers (4)"
#endif
#if LAYER_3B_ASIDE
#error "CS_List.cpp (Layer 3B - host) cannot depend on A-side layers (2A, 3A)"
#endif

// CS_List is header-only, so this file just enforces layer checks
