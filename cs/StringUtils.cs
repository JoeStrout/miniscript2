// String utilities: conversion between ints and Strings, etc.

using System;
using System.Collections.Generic;
// CPP: #include "IOHelper.g.h"
// CPP: #include "value_string.h"
// CPP: #include "value_list.h"
// CPP: #include "value_map.h"
// CPP: #include <sstream>
// CPP: #include <cctype>

namespace MiniScript {

    public static class StringUtils {
		public const String hexDigits = "0123456789ABCDEF";
		
		public static String ToHex(UInt32 value) {
			char[] hexChars = new char[8];
			for (Int32 i = 7; i >= 0; i--) {
				hexChars[i] = hexDigits[(int)(value & 0xF)];
				value >>= 4;
			}
			return new String(hexChars);
		}
		
		public static String ToHex(Byte value) {
			char[] hexChars = new char[2];
			for (Int32 i = 1; i >= 0; i--) {
				hexChars[i] = hexDigits[(int)(value & 0xF)];
				value >>= 4;
			}
			return new String(hexChars);
		}
		
		public static String ZeroPad(Int32 value, Int32 digits = 5) {
			return value.ToString("D" + digits.ToString()); // CPP: // set width and fill
			/*** BEGIN CPP_ONLY ***
			char format[] = "%05d";
			format[2] = '0' + digits;
			char buffer[20];
			snprintf(buffer, 20, format, value);
			return String(buffer);
			*** END CPP_ONLY ***/
		}
		
		public static String Spaces(Int32 count) {
			if (count < 1) return "";
			return new String(' ', count); // CPP:
			/*** BEGIN CPP_ONLY ***
			char* spaces = (char*)malloc(count + 1);
			memset(spaces, ' ', count);
			spaces[count] = '\0';
			String result(spaces);
			free(spaces);
			return result;
			*** END CPP_ONLY ***/
		}

		public static String SpacePad(String text, Int32 width) {
			if (text == null) text = "";
			if (text.Length >= width) return text.Substring(0, width);
			return text + Spaces(width - text.Length);
		}

		public static String Str(List<String> list) {
			if (list == null) return "null"; // CPP: // (null not possible)
			if (list.Count == 0) return "[]";
			return new String("[\"") + String.Join("\", \"", list) + "\"]";
		}

		// Convert a Value to its representation (quoted string for strings, plain for others)
		// This is used when displaying values in source code form.
		//*** BEGIN CS_ONLY ***
		public static String makeRepr(int pool, Value v) {
			if (ValueHelpers.is_string(v)) {
				String str = ValueHelpers.as_cstring(v);
				// Replace quotes: " becomes ""
				String escaped = str.Replace("\"", "\"\"");
				// Wrap in quotes
				return "\"" + escaped + "\"";
			}
			return v.ToString();
		}
		//*** END CS_ONLY ***

		//*** BEGIN CS_ONLY ***
		public static string Left(this string s, int n) {
			if (String.IsNullOrEmpty(s)) return "";
			return s.Substring(0, Math.Min(n, s.Length));
		}

		public static string Right(this string s, int n) {
			if (String.IsNullOrEmpty(s)) return "";
			return s.Substring(Math.Max(s.Length-n, 0));
		}
		
		// Usage: StringUtils.Format("Hello {0}, x={1}, {{braces}}", name, 42)
		public static string Format(String fmt, params object[] args)
		{
			if (String.IsNullOrEmpty(fmt)) return "";
			var sb = new System.Text.StringBuilder(fmt.Length + 16 * (args?.Length ?? 0));
			for (int i = 0; i < fmt.Length;) {
				char c = fmt[i];
				if (c == '{') {
					if (i + 1 < fmt.Length && fmt[i + 1] == '{') { 
						sb.Append('{'); i += 2; continue;
					}
					int j = i + 1, num = 0; bool any = false;
					while (j < fmt.Length && char.IsDigit(fmt[j])) {
						any = true;
						num = num * 10 + (fmt[j] - '0');
						j++;
					}
					if (!any || j >= fmt.Length || fmt[j] != '}') {
						throw new FormatException("Invalid placeholder; expected {n}.");
					}
					if (args == null || num < 0 || num >= args.Length) {
						throw new FormatException("Placeholder index out of range.");
					}
					sb.Append(args[num]?.ToString());
					i = j + 1;
				} else if (c == '}') {
					if (i + 1 < fmt.Length && fmt[i + 1] == '}') {
						sb.Append('}');
						i += 2;
						continue;
					}
					throw new FormatException("Single '}' in format string.");
				} else {
					sb.Append(c);
					i++;
				}
			}
			return sb.ToString();
		}
		//*** END CS_ONLY ***
		/*** BEGIN H_ONLY ***
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
		*** END H_ONLY ***/
		/*** BEGIN CPP_ONLY ***
		String StringUtils::FormatList(const String& fmt, const List<String>& values) {
			const int n = fmt.Length();
			const uint8_t pool = fmt.getPoolNum();
		
			List<String> parts(pool); // collect chunks, then Join
			int i = 0;
		
			while (i < n) {
				const char c = fmt[i];
		
				if (c == '{') {
					if (i + 1 < n && fmt[i + 1] == '{') {
						parts.Add(String("{", pool)); i += 2; continue;
					}
					int j = i + 1;
					if (j >= n || !std::isdigit(static_cast<unsigned char>(fmt[j])))
						throw std::runtime_error("Invalid placeholder; expected {n}.");
					int num = 0;
					while (j < n && std::isdigit(static_cast<unsigned char>(fmt[j]))) {
						num = num * 10 + (fmt[j] - '0'); ++j;
					}
					if (j >= n || fmt[j] != '}')
						throw std::runtime_error("Invalid placeholder; expected closing '}'.");
					// bounds check
					if (num < 0 || num >= values.Count())
						throw std::runtime_error("Placeholder index out of range.");
					parts.Add(values[num]);
					i = j + 1;
				}
				else if (c == '}') {
					if (i + 1 < n && fmt[i + 1] == '}') {
						parts.Add(String("}", pool)); i += 2; continue;
					}
					throw std::runtime_error("Single '}' in format string.");
				}
				else {
					// consume a run of non-brace chars as one chunk
					int start = i;
					while (i < n) {
						char ch = fmt[i];
						if (ch == '{' || ch == '}') break;
						++i;
					}
					parts.Add(fmt.Substring(start, i - start));
				}
			}
		
			return String::Join(String("", pool), parts, pool);  // join with empty separator
		}
		*** END CPP_ONLY ***/
	}

}
