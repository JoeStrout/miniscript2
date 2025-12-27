// CS_String.cpp - non-inline implementations for CS_String.h

#include "CS_String.h"

#include "layer_defs.h"
#if LAYER_2B_HIGHER
#error "CS_String.cpp (Layer 2B) cannot depend on higher layers (3)"
#endif
#if LAYER_2B_ASIDE
#error "CS_String.cpp (Layer 2B - host) cannot depend on A-side layers (2A)"
#endif


StringStorageSPtr ss_createShared(int byteLen) {
    if (byteLen <= 0) return nullptr;	// (empty strings represented as null

    size_t totalSize = sizeof(StringStorage) + byteLen + 1;
    StringStorage* rawPtr = (StringStorage*)malloc(totalSize);
    if (!rawPtr) return NULL;

    rawPtr->lenB = byteLen;
    rawPtr->lenC = -1;  // Will be computed when needed
    rawPtr->hash = 0;   // Will be computed when needed
    rawPtr->data[byteLen] = '\0';  // Ensure null termination

    return std::shared_ptr<StringStorage>(rawPtr, [](StringStorage* p) { ::free(p); });
}

StringStorageSPtr ss_createShared(const char *cstr, int byteLen) {
	if (byteLen < 0) byteLen = strlen(cstr);
    if (byteLen <= 0) return nullptr;

    size_t totalSize = sizeof(StringStorage) + byteLen + 1;
    StringStorage* rawPtr = (StringStorage*)malloc(totalSize);
    if (!rawPtr) return NULL;

    rawPtr->lenB = byteLen;
    rawPtr->lenC = -1;  // Will be computed when needed
    rawPtr->hash = 0;   // Will be computed when needed
    memcpy(rawPtr->data, cstr, byteLen);
    rawPtr->data[byteLen] = '\0';  // Ensure null termination

    return std::shared_ptr<StringStorage>(rawPtr, [](StringStorage* p) { ::free(p); });
}

StringStorageSPtr String::FindOrCreate(const char *cstr, int byteLen) {
	if (byteLen < 0) byteLen = strlen(cstr);
	if (byteLen < 128) {
		// ToDo: look for this string already interned, or else intern it now
	}
	return ss_createShared(cstr, byteLen);
}

//================================================================================
// String Interpolation Implementation
//================================================================================

namespace InterpImpl {
	// Helper to format a double with a format specifier (e.g., "0.00" -> 2 decimal places)
	String FormatDouble(double val, const char* formatSpec) {
		// Parse format like "0.00" to determine decimal places
		int decimalPlaces = -1;
		if (formatSpec) {
			const char* dot = strchr(formatSpec, '.');
			if (dot) {
				// Count zeros after the decimal point
				decimalPlaces = 0;
				const char* p = dot + 1;
				while (*p == '0') {
					decimalPlaces++;
					p++;
				}
			}
		}

		// Format the double
		char buf[64];
		if (decimalPlaces >= 0) {
			char fmt[16];
			snprintf(fmt, sizeof(fmt), "%%.%df", decimalPlaces);
			snprintf(buf, sizeof(buf), fmt, val);
		} else {
			snprintf(buf, sizeof(buf), "%g", val);  // General format (no trailing zeros)
		}

		return String(buf);
	}

	// Base case: no more arguments
	inline void InterpRecursive(String& result, const char* format, int& pos) {
		// Append remaining format string
		if (format[pos]) {
			result += String(format + pos);
		}
	}

	// Helper to convert argument to string with optional format spec
	// Overloads for different types (C++14 compatible)
	inline String ConvertArg(double val, const char* formatSpec) {
		if (formatSpec && *formatSpec) {
			return FormatDouble(val, formatSpec);
		}
		return ArgToString(val);
	}

	inline String ConvertArg(float val, const char* formatSpec) {
		if (formatSpec && *formatSpec) {
			return FormatDouble(val, formatSpec);
		}
		return ArgToString(val);
	}

	template<typename T>
	inline String ConvertArg(T val, const char* /*formatSpec*/) {
		return ArgToString(val);
	}

	// Recursive case: process one argument
	template<typename T, typename... Rest>
	void InterpRecursive(String& result, const char* format, int& pos, T first, Rest... rest) {
		// Find next {}
		while (format[pos]) {
			if (format[pos] == '{') {
				// Check for format specifier
				int start = pos + 1;
				int end = start;
				while (format[end] && format[end] != '}') end++;

				if (format[end] == '}') {
					// Extract format spec (if any)
					char formatSpec[32] = {0};
					if (end > start) {
						int len = end - start;
						if (len >= (int)sizeof(formatSpec)) len = sizeof(formatSpec) - 1;
						strncpy(formatSpec, format + start, len);
						formatSpec[len] = '\0';
					}

					// Convert argument to string using overload resolution
					String argStr = ConvertArg(first, formatSpec);

					result += argStr;
					pos = end + 1;

					// Process remaining arguments
					InterpRecursive(result, format, pos, rest...);
					return;
				}
			}

			// Not a placeholder, append this character
			result += format[pos];
			pos++;
		}

		// If we get here, we ran out of format string before using all arguments
		// Just ignore remaining arguments
	}
}

// Main Interp function
template<typename... Args>
String Interp(const char* format, Args... args) {
	if (!format) return String();

	String result;
	int pos = 0;
	InterpImpl::InterpRecursive(result, format, pos, args...);
	return result;
}

// Explicit instantiations for common types to avoid template bloat
template String Interp(const char*, int);
template String Interp(const char*, double);
template String Interp(const char*, const char*);
template String Interp(const char*, String);
template String Interp(const char*, int, int);
template String Interp(const char*, double, double);
template String Interp(const char*, String, String);
template String Interp(const char*, const char*, const char*);
template String Interp(const char*, String, int);
template String Interp(const char*, String, double);
