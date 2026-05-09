// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: StringUtils.cs

#pragma once
#include "core_includes.h"
#include "forward_decs.g.h"
// String utilities: conversion between ints and Strings, etc.

#include "CS_String.h"
#include "value.h"
#include "value_string.h"
#include "value_list.h"
#include "value_map.h"
#include <sstream>

namespace MiniScript {

// DECLARATIONS

class StringUtils {
	public: static const String hexDigits;
	
	public: static String ToHex(UInt32 value);
	
	public: static String ToHex(Byte value);

	public: static Int32 ParseInt32(String str);

	public: static Double ParseDouble(String str);

	public: static Boolean IsNaN(Double x);

	public: static Boolean IsInfinity(Double x);

	public: static String ZeroPad(Int32 value, Int32 digits = 5);
	
	public: static String Spaces(Int32 count);

	public: static String SpacePad(String text, Int32 width);

	public: static String Str(List<String> list);

	public: static String Str(Char c);
	//--- The following is all to support a Format function, equivalent to
	//--- the one in C#.  The C++ one requires templates and helper functions.
	public: static String FormatList(const String& fmt, const List<String>& values);

	// --- stringify helpers ---
	public:
	inline static String makeString(uint8_t, const String& s) {
		return s;
	}
	inline static String makeString(const Char* s) {
		if (s) return String(s);
		return "";
	}
	inline static String makeString(Char c) {
		Char buf[2] = {c, '\0'};
		return String(buf);
	}
	// Value type: delegate to to_string (handles all types, raw string for strings)
	inline static String makeString(Value v) {
		return String(as_cstring(to_string(v, NULL)));
	}
	// Value type with VM context: enables short-name display for maps/lists
	inline static String makeString(Value v, void* vm) {
		return String(as_cstring(to_string(v, vm)));
	}
	// Value repr (strings quoted): delegate to value_repr
	inline static String makeRepr(const Value v) {
		return String(as_cstring(value_repr(v, NULL)));
	}

	// Generic fallback for numbers and streamable types.
	template <typename T>
	inline static String makeString(const T& v) {
		std::ostringstream oss; oss << v;
		const std::string tmp = oss.str();
		return String(tmp.c_str());
	}
	// Generic fallback with VM context (ignores vm for non-Value types).
	template <typename T>
	inline static String makeString(const T& v, void* vm) {
		(void)vm;
		return makeString(v);
	}

	template <typename... Args>
	inline static void addValues(List<String>& list, Args&&... args) {
		String tmp[] = { makeString(std::forward<Args>(args))... };
		for (const auto& s : tmp) list.Add(s);
	}
	// VM-aware addValues: passes vm to makeString for each argument.
	template <typename... Args>
	inline static void addValuesVM(List<String>& list, void* vm, Args&&... args) {
		String tmp[] = { makeString(std::forward<Args>(args), vm)... };
		for (const auto& s : tmp) list.Add(s);
	}

	public:
	template <typename... Args>
	inline static String Format(const String& fmt, Args&&... args) {
		List<String> vals;
		addValues(vals, std::forward<Args>(args)...);
		return StringUtils::FormatList(fmt, vals);
	}
	// VM-aware Format: use when formatting Values that may be maps/lists.
	template <typename... Args>
	inline static String FormatVM(const String& fmt, void* vm, Args&&... args) {
		List<String> vals;
		addValuesVM(vals, vm, std::forward<Args>(args)...);
		return StringUtils::FormatList(fmt, vals);
	}

	// Convert a Value to its representation (quoted string for strings, plain for others)
	// This is used when displaying values in source code form.
}; // end of struct StringUtils

// INLINE METHODS

inline Int32 StringUtils::ParseInt32(String str) {
	return std::stoi(str.c_str());
}
inline Double StringUtils::ParseDouble(String str) {
	return std::stod(str.c_str());
}
inline Boolean StringUtils::IsNaN(Double x) {
	return std::isnan(x);
}
inline Boolean StringUtils::IsInfinity(Double x) {
	return std::isinf(x);
}
inline String StringUtils::Str(Char c) {
	return String(c);
}

} // end of namespace MiniScript
