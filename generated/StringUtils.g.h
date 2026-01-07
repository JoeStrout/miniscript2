// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: StringUtils.cs

#pragma once
#include "core_includes.h"
// String utilities: conversion between ints and Strings, etc.

#include "CS_String.h"
#include "IOHelper.g.h"
#include "value_string.h"
#include "value_list.h"
#include "value_map.h"
#include <sstream>
#include <cctype>

namespace MiniScript {

// FORWARD DECLARATIONS

struct CallInfo;
class CallInfoStorage;
struct VMVis;
class VMVisStorage;
struct Assembler;
class AssemblerStorage;
struct FuncDef;
class FuncDefStorage;
struct App;
class AppStorage;
struct Lexer;
class LexerStorage;

// DECLARATIONS




class StringUtils {
	public: static const String hexDigits;
	
	public: static String ToHex(UInt32 value);
	
	public: static String ToHex(Byte value);
	
	public: static String ZeroPad(Int32 value, Int32 digits = 5);
	
	public: static String Spaces(Int32 count);

	public: static String SpacePad(String text, Int32 width);

	public: static String Str(List<String> list);

	// Convert a Value to its representation (quoted string for strings, plain for others)
	// This is used when displaying values in source code form.
	//--- The following is all to support a Format function, equivalent to
	//--- the one in C#.  The C++ one requires templates and helper functions.
	public: static String FormatList(const String& fmt, const List<String>& values);

	// --- stringify helpers ---
	public:
	inline static String makeString(uint8_t, const String& s) { 
		return s;
	}
	inline static String makeString(const Char* s) {
		return String(s ? s : "");
	}
	inline static String makeString(Char c) {
		Char buf[2] = {c, '\0'};
		return String(buf);
	}
	// Value type support
	inline static String makeString(Value v) {
		if (is_null(v)) return String("null");
		if (is_int(v)) return makeString(as_int(v));
		if (is_double(v)) return makeString(as_double(v));
		if (is_string(v)) {
			const Char* str = as_cstring(v);
			return str ? String(str) : String("<str?>");
		}
		if (is_list(v)) {
			std::ostringstream oss;
			oss << "[";
			// ToDo: watch out for recursion, or maybe just limit our depth in
			// general.  I think MS1.0 limits nesting to 16 levels deep.  But
			// whatever we do, we shouldn't just crash with a stack overflow.
			for (int i = 0; i < list_count(v); i++) {
				oss << (i != 0 ? ", " : "") << makeString(list_get(v, i)).c_str();
			}
			oss << "]";
			return String(oss.str().c_str());
		}
		if (is_map(v)) {
			std::ostringstream oss;
			oss << "{";
			// ToDo: watch out for recursion, or maybe just limit our depth in
			// general.  I think MS1.0 limits nesting to 16 levels deep.  But
			// whatever we do, we shouldn't just crash with a stack overflow.
			MapIterator iter = map_iterator(v); 
			Value key, value;
			bool first = true;
			while (map_iterator_next(&iter, &key, &value)) {
				if (first) first = false; else oss << ", ";
				oss << makeRepr(key).c_str() << ": "
					<< makeRepr(value).c_str();
			}
			oss << "}";
			return String(oss.str().c_str());
		}
		if (is_funcref(v)) {
			std::ostringstream oss;
			oss << "FuncRef(" << funcref_index(v) << ")";
			return String(oss.str().c_str());
		}
		std::ostringstream oss;
		oss << "<value:0x" << std::hex << v << ">";
		return String(oss.str().c_str());
	}
	inline static String makeRepr(const Value v) {
		if (is_string(v)) {
			const Char* str = as_cstring(v);
			if (!str) {
				return String("\"\"");
			}
			String s = String(str);
			// Replace quotes: " becomes ""
			String escaped = s.Replace(String("\""), String("\"\""));
			// Wrap in quotes
			return String("\"") + escaped + String("\"");
		}
		return makeString(v);
	}

	// Generic fallback for numbers and streamable types.
	template <typename T>
	inline static String makeString(const T& v) {
		std::ostringstream oss; oss << v;
		const std::string tmp = oss.str();
		return String(tmp.c_str());
	}
	
	template <typename... Args>
	inline static void addValues(List<String>& list, Args&&... args) {
		String tmp[] = { makeString(std::forward<Args>(args))... };
		for (const auto& s : tmp) list.Add(s);
	}
	
	public: 
	template <typename... Args>
	inline static String Format(const String& fmt, Args&&... args) {
		List<String> vals;
		addValues(vals, std::forward<Args>(args)...);
		return StringUtils::FormatList(fmt, vals);
	}
}; // end of struct StringUtils












// INLINE METHODS

} // end of namespace MiniScript
