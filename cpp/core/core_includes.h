// Includes and type definitions needed by all our transpiled C# --> C++ files.
#ifndef CORE_INCLUDES_H
#define CORE_INCLUDES_H

// Memory debugging support
#define MEM_DEBUG 0

#include "CS_List.h"
#include "CS_String.h"
#include "CS_Dictionary.h"

#include <cstdint>
#include <vector>
#include <memory>
#include <functional>

// C++11 compatibility: make_unique was added in C++14
#if __cplusplus < 201402L
namespace std {
    template<typename T, typename... Args>
    unique_ptr<T> make_unique(Args&&... args) {
        return unique_ptr<T>(new T(std::forward<Args>(args)...));
    }
}
#endif

// This module is part of Layer 2B (Host C# Compatibility Layer)
#define CORE_LAYER_2B

// Disable the "unused parameter" warning.  Some of our transpiled classes
// (particularly the ASTNode and Parselet hierarchies) make extensive use
// of virtual methods, and in many cases, don't need all the parameters,
// but C# does not allow us to leave such parameters unnamed.
#pragma GCC diagnostic ignored "-Wunused-parameter"

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

// And, for where we actually need a null in both C# and C++:
#define null nullptr

// Boolean wrapper class that adds a user-defined conversion step
// This makes pointer-to-Boolean conversion (via bool) rank lower than
// exact matches in overload resolution, preventing ambiguity with String overloads
struct Boolean {
	bool value;
	Boolean() : value(false) {}
	Boolean(bool b) : value(b) {}
	operator bool() const { return value; }
    // Prevent accidental "string literal" (or other pointer) -> Boolean
    template<typename T> Boolean(T*) = delete;
};

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
