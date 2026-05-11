// String utilities: conversion between ints and Strings, etc.

using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using static System.Runtime.CompilerServices.MethodImplOptions;
using static MiniScript.ValueHelpers;
// H: #include "CS_String.h"
// H: #include "value.h"
// H: #include "value_string.h"
// H: #include "value_list.h"
// H: #include "value_map.h"
// H: #include <sstream>
// CPP: #include <cctype>
// CPP: #include <cmath>
// CPP: #include "IOHelper.g.h"

namespace MiniScript {

public static class StringUtils {
	public const String hexDigits = "0123456789ABCDEF";
	
	public static String ToHex(UInt32 value) {
		Char[] hexChars = new Char[8]; // CPP: char hexChars[9]{};
		for (Int32 i = 7; i >= 0; i--) {
			hexChars[i] = hexDigits[(int)(value & 0xF)];
			value >>= 4;
		}
		return new String(hexChars);
	}
	
	public static String ToHex(Byte value) {
		Char[] hexChars = new Char[2]; // CPP: char hexChars[3]{};
		for (Int32 i = 1; i >= 0; i--) {
			hexChars[i] = hexDigits[(int)(value & 0xF)];
			value >>= 4;
		}
		return new String(hexChars);
	}

	[MethodImpl(AggressiveInlining)]
	public static Int32 ParseInt32(String str) {
		return Int32.Parse(str);	// CPP: return std::stoi(str.c_str());
	}

	[MethodImpl(AggressiveInlining)]
	public static Double ParseDouble(String str) {
		return Double.Parse(str);	// CPP: return std::stod(str.c_str());
	}

	[MethodImpl(AggressiveInlining)]
	public static Boolean IsNaN(Double x) {
		return double.IsNaN(x);	// CPP: return std::isnan(x);
	}

	[MethodImpl(AggressiveInlining)]
	public static Boolean IsInfinity(Double x) {
		return double.IsInfinity(x);	// CPP: return std::isinf(x);
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
		if (text == null) text = ""; // CPP: // (null not possible)
		if (text.Length >= width) return text.Substring(0, width);
		return text + Spaces(width - text.Length);
	}

	public static String Str(List<String> list) {
		if (list == null) return "null"; // CPP: // (null not possible)
		if (list.Count == 0) return "[]";
		return new String("[\"") + String.Join("\", \"", list) + "\"]";
	}

	[MethodImpl(AggressiveInlining)]
	public static String Str(Char c) {
		return c.ToString();	// CPP: return String(c);
	}

	// Convert a Value to its representation (quoted string for strings, plain for others)
	// This is used when displaying values in source code form.
	//*** BEGIN CS_ONLY ***
	public static String Left(this String s, int n) {
		if (String.IsNullOrEmpty(s)) return "";
		return s.Substring(0, Math.Min(n, s.Length));
	}

	public static String Right(this String s, int n) {
		if (String.IsNullOrEmpty(s)) return "";
		return s.Substring(Math.Max(s.Length-n, 0));
	}

	public static String makeRepr(Value v) {
		if (ValueHelpers.is_string(v)) {
			String str = ValueHelpers.as_cstring(v);
			// Replace quotes: " becomes ""
			String escaped = str.Replace("\"", "\"\"");
			// Wrap in quotes
			return "\"" + escaped + "\"";
		}
		return to_String(v);
	}
	
	// Usage: StringUtils.Format("Hello {0}, x={1}, {{braces}}", name, 42)
	// ToDo: maybe remove this, make use of string interpolation instead.
	public static string Format(String fmt, params object[] args)
	{
		if (String.IsNullOrEmpty(fmt)) return "";
		var sb = new System.Text.StringBuilder(fmt.Length + 16 * (args?.Length ?? 0));
		for (int i = 0; i < fmt.Length;) {
			Char c = fmt[i];
			if (c == '{') {
				if (i + 1 < fmt.Length && fmt[i + 1] == '{') { 
					sb.Append('{'); i += 2; continue;
				}
				int j = i + 1, num = 0; bool any = false;
				while (j < fmt.Length && Char.IsDigit(fmt[j])) {
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
	*** END H_ONLY ***/
	/*** BEGIN CPP_ONLY ***
	String StringUtils::FormatList(const String& fmt, const List<String>& values) {
		const int n = fmt.Length();
	
		List<String> parts; // collect chunks, then Join
		int i = 0;
	
		while (i < n) {
			const Char c = fmt[i];
	
			if (c == '{') {
				if (i + 1 < n && fmt[i + 1] == '{') {
					parts.Add(String("{")); i += 2; continue;
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
					parts.Add(String("}")); i += 2; continue;
				}
				throw std::runtime_error("Single '}' in format string.");
			}
			else {
				// consume a run of non-brace chars as one chunk
				int start = i;
				while (i < n) {
					Char ch = fmt[i];
					if (ch == '{' || ch == '}') break;
					++i;
				}
				parts.Add(fmt.Substring(start, i - start));
			}
		}
	
		return String::Join(String(""), parts);  // join with empty separator
	}
	*** END CPP_ONLY ***/
}

}
