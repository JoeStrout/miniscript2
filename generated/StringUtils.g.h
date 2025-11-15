// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: StringUtils.cs

#ifndef __STRINGUTILS_H
#define __STRINGUTILS_H

// String utilities: conversion between ints and Strings, etc.

#include "core_includes.h"
#include "IOHelper.g.h"
#include "value_string.h"
#include "value_list.h"
#include "value_map.h"
#include <sstream>
#include <cctype>

namespace MiniScript {

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

		// --- stringify helpers -> String in the same pool as fmt ---
		public:
		inline static String makeString(uint8_t, const String& s) { 
			return s;
		}
		inline static String makeString(uint8_t pool, const char* s) {
			return String(s ? s : "", pool);
		}
		inline static String makeString(uint8_t pool, char c) {
			char buf[2] = {c, '\0'};
			return String(buf, pool);
		}
		// Value type support
		inline static String makeString(uint8_t pool, Value v) {
			if (is_null(v)) return String("null", pool);
			if (is_int(v)) return makeString(pool, as_int(v));
			if (is_double(v)) return makeString(pool, as_double(v));
			if (is_string(v)) {
				const char* str = as_cstring(v);
				return str ? String(str, pool) : String("<str?>", pool);
			}
			if (is_list(v)) {
				std::ostringstream oss;
				oss << "[";
				// ToDo: watch out for recursion, or maybe just limit our depth in
				// general.  I think MS1.0 limits nesting to 16 levels deep.  But
				// whatever we do, we shouldn't just crash with a stack overflow.
				for (int i = 0; i < list_count(v); i++) {
					oss << (i != 0 ? ", " : "") << makeString(pool, list_get(v, i)).c_str();
				}
				oss << "]";
				return String(oss.str().c_str(), pool);
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
					oss << makeRepr(pool, key).c_str() << ": "
					    << makeRepr(pool, value).c_str();
				}
				oss << "}";
				return String(oss.str().c_str(), pool);
			}
			if (is_funcref(v)) {
				std::ostringstream oss;
				oss << "FuncRef(" << funcref_index(v) << ")";
				return String(oss.str().c_str(), pool);
			}
			std::ostringstream oss;
			oss << "<value:0x" << std::hex << v << ">";
			return String(oss.str().c_str(), pool);
		}
		inline static String makeRepr(uint8_t pool, const Value v) {
			if (is_string(v)) {
				const char* str = as_cstring(v);
				if (!str) {
					return String("\"\"", pool);
				}
				String s = String(str, pool);
				// Replace quotes: " becomes ""
				String escaped = s.Replace(String("\"", pool), String("\"\"", pool));
				// Wrap in quotes
				return String("\"", pool) + escaped + String("\"", pool);
			}
			return makeString(pool, v);
		}

		// Generic fallback for numbers and streamable types.
		template <typename T>
		inline static String makeString(uint8_t pool, const T& v) {
			std::ostringstream oss; oss << v;
			const std::string tmp = oss.str();
			return String(tmp.c_str(), pool);
		}
		
		template <typename... Args>
		inline static void addValues(List<String>& list, uint8_t pool, Args&&... args) {
			String tmp[] = { makeString(pool, std::forward<Args>(args))... };
			for (const auto& s : tmp) list.Add(s);
		}
		
		public: 
		template <typename... Args>
		inline static String Format(const String& fmt, Args&&... args) {
			const uint8_t pool = fmt.getPoolNum();
			List<String> vals(pool);
			addValues(vals, pool, std::forward<Args>(args)...);
			return StringUtils::FormatList(fmt, vals);
		}
	}; // end of class StringUtils

}

#endif // __STRINGUTILS_H
