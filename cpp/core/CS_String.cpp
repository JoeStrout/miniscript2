// CS_String.cpp - non-inline implementations for CS_String.h

#include "CS_String.h"

#include "layer_defs.h"
#if LAYER_3B_HIGHER
#error "CS_String.cpp (Layer 3B) cannot depend on higher layers (4)"
#endif
#if LAYER_3B_ASIDE
#error "CS_String.cpp (Layer 3B - host) cannot depend on A-side layers (2A, 3A)"
#endif

// Define the static member
uint8_t String::defaultPool = 0;
