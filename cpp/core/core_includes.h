// Includes and type definitions needed by all our transpiled C# --> C++ files.
#ifndef CORE_INCLUDES_H
#define CORE_INCLUDES_H

// Memory debugging support
#define MEM_DEBUG 0

#include "CS_List.h"
#include "CS_String.h"

#include <cstdint>

// This module is part of Layer 3B (Host C# Compatibility Layer)
#define CORE_LAYER_3B

// Data types which, in C#, are all defined in System.
// C# code should use these instead of the shortcuts (int, byte, long, etc.)
using Byte    = uint8_t;   // use this instead of byte
using SByte   = int8_t;    // sbyte

using Int16   = int16_t;   // short
using UInt16  = uint16_t;  // ushort

using Int32   = int32_t;   // int
using UInt32  = uint32_t;  // uint

using Int64   = int64_t;   // long
using UInt64  = uint64_t;  // ulong

// The following will work with both the shortcut names,
// and the full System names.
using Char    = char;
using Single  = float;
using Double  = double;
using Boolean = bool;

// Min/Max values for these types:
const Byte ByteMinValue = 0;
const Byte ByteMaxValue = 255;
const SByte SByteMinValue = -128;
const SByte SByteMaxValue = 127;
const Int16 Int16MinValue = -32768;
const Int16 Int16MaxValue = 32767;
const UInt16 UInt16MinValue = 0;
const UInt16 UInt16MaxValue = 65535;
const Int32 Int32MinValue = -2147483648;
const Int32 Int32MaxValue = 2147483647;
const UInt32 UInt32MinValue = 0;
const UInt32 UInt32MaxValue = 4294967295U;

#endif // CORE_INCLUDES_H
