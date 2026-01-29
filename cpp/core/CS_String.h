// This header defines the String class, which is our equivalent of
// a C# String, and is used with C++ code transpiled from C#.
// Memory management is done via std::shared_ptr.
//
// (Called ms_string.h because String.h conflicts with a standard
// C++ library header.)

#pragma once
#include <cstdint>
#include <cstring>
#include <cstdlib>  // For malloc/free
#include "StringStorage.h"
#include "CS_List.h"
#include <cstdio> // for debugging
#include <stdio.h>
#include <memory>
#include <vector>

// This module is part of Layer 2B (Host C# Compatibility Layer)
#define CORE_LAYER_2B

// Forward declaration to avoid circular dependency
template<typename T> class List;

using StringStorageSPtr = std::shared_ptr<StringStorage>;

// Helper methods to create a std::shared_ptr to a new StringStorage
StringStorageSPtr ss_createShared(int byteLen);
StringStorageSPtr ss_createShared(const char *cstr, int byteLen=-1);

// Lightweight String class - thin wrapper around shared_ptr<StringStorage>
class String {
private:
	StringStorageSPtr ref = nullptr;	// shared pointer to actual data

	// Helper method to either intern (or find already interned) the given string,
	// or make a fresh new storage for it (depending on its size).
	static StringStorageSPtr FindOrCreate(const char *cstr, int byteLen=-1);
	
	// Helper method to create a String from a StringStorage freshly made with malloc
	static String fromMallocStorage(StringStorage* rawPtr) {
		String s;
		s.ref = std::shared_ptr<StringStorage>(rawPtr, [](StringStorage* p) { ::free(p); });
		return s;
	}

	// Safely wrap StringStorage* returned from ss_* functions.
	// Many ss_* functions may return their input pointer (cast away const) for efficiency
	// when the operation would be a no-op (e.g., toLower on an already lowercase string).
	// This helper detects that case and reuses the existing String instead of creating
	// a new shared_ptr to already-managed memory (which would cause double-free).
	static String wrapStorage(StringStorage* result, const String& original) {
		if (!result) return String();  // NULL result = empty string
		if (result == original.getStorageRaw()) return original;  // Same storage, reuse
		return fromMallocStorage(result);  // New storage, wrap it
	}

	// Overload for two possible original strings (for binary operations like concat)
	static String wrapStorage(StringStorage* result, const String& original1, const String& original2) {
		if (!result) return String();
		if (result == original1.getStorageRaw()) return original1;
		if (result == original2.getStorageRaw()) return original2;
		return fromMallocStorage(result);
	}

    // Private constructor from StringStorageSPtr
    String(StringStorageSPtr ssRef) : ref(ssRef) {}
    
public:

    // Default constructor - null (matches C# uninitialized reference)
	String() {}

    // Check if string is null (unallocated)
    friend bool IsNull(const String& str) {
        return str.ref == nullptr;
    }

	String(const char* cstr) {
		if (!cstr) return;
		ref = FindOrCreate(cstr);
	}
	
	String(char c) {
		char buf[2] = {c, 0};
		ref = FindOrCreate(buf);
	}

    // Factory method - allocates empty string (matches C# "new String()")
    static String New() { return String(""); }

    // Factory method with argument
    static String New(const char *cstr) { return String(cstr); }

    // Copy constructor and assignment (defaulted for trivial copyability)
    String(const String& other) = default;
    String& operator=(const String& other) = default;
    
    // Assignment from C string
    String& operator=(const char* cstr) {
		if (!cstr || *cstr == '\0') {
			ref = nullptr;
		} else {
			ref = FindOrCreate(cstr);
		}
        return *this;
    }
    
    // Basic operations (inline wrappers)
    int lengthB() const { 
        const StringStorage* s = getStorageRaw(); 
        return ss_lengthB(s);
    }
    
    int lengthC() const { 
        const StringStorage* s = getStorageRaw(); 
        return ss_lengthC(s);
    }
    
    const char* c_str() const {
		if (!ref) return "";
		const StringStorage *ss = getStorageRaw();
		return ss ? ss->data : "";
    }

	// Implicit conversion to const char* for use with C APIs and stream operators
	operator const char*() const {
		return c_str();
	}

	const StringStorage* getStorageRaw() const {
		return (const StringStorage*)(ref.get());
	}
	
    // String concatenation
    String operator+(const String& other) const {
        const StringStorage* s1 = getStorageRaw();
        const StringStorage* s2 = other.getStorageRaw();
		if (!s1) return other;
		if (!s2) return *this;

        StringStorage* result_ss = ss_concat(s1, s2, malloc);
        return wrapStorage(result_ss, *this, other);
    }

    // String concatenation assignment
    String& operator+=(const String& other) {
        const StringStorage* s1 = getStorageRaw();
        const StringStorage* s2 = other.getStorageRaw();
		if (!s2) return *this;
		if (!s1) {
			*this = other;
			return *this;
		}

        StringStorage* result_ss = ss_concat(s1, s2, malloc);
        *this = wrapStorage(result_ss, *this, other);
        return *this;
    }

    // String concatenation assignment (single character)
    String& operator+=(char c) {
        *this = *this + String(c);
        return *this;
    }

    // Comparison
    bool operator==(const String& other) const {
        if (ref == other.ref) return true;
        
        const StringStorage* s1 = getStorageRaw();
        const StringStorage* s2 = other.getStorageRaw();
        if (!s1) return !s2;
        if (s1->lenB != s2->lenB) return false;
        
        return ss_equals(s1, s2);
    }
    
    bool operator!=(const String& other) const { return !(*this == other); }

    // Comparison with C strings
    bool operator==(const char* cstr) const {
        const StringStorage* s = getStorageRaw();
        if (!s && (!cstr || *cstr == '\0')) return true;  // Both empty
        if (!s) return false;  // This is empty, cstr is not
        if (!cstr || *cstr == '\0') return false;  // cstr is empty, this is not
        return strcmp(s->data, cstr) == 0;
    }

    bool operator!=(const char* cstr) const { return !(*this == cstr); }

    bool operator<(const String& other) const {
        return Compare(other) < 0;
    }
    
    bool operator<=(const String& other) const {
        return Compare(other) <= 0;
    }
    
    bool operator>(const String& other) const {
        return Compare(other) > 0;
    }
    
    bool operator>=(const String& other) const {
        return Compare(other) >= 0;
    }
    
    // C# String API - Properties
    int Length() const { return lengthC(); }
    bool Empty() const { return Length() == 0; }
    
    // C# String API - Character access
    char operator[](int index) const {
        const StringStorage* s = getStorageRaw();
        return ss_charAt(s, index);
    }
    
    // C# String API - Search methods
    int IndexOf(const String& value) const {
        const StringStorage* s = getStorageRaw();
        const StringStorage* needle = value.getStorageRaw();
        if (!needle || needle->lenB == 0) return 0;
        return ss_indexOf(s, needle);
    }
    
    int IndexOf(const String& value, int startIndex) const {
        const StringStorage* s = getStorageRaw();
        const StringStorage* needle = value.getStorageRaw();
        if (!needle || needle->lenB == 0) return startIndex;
        return ss_indexOfFrom(s, needle, startIndex);
    }
    
    int IndexOf(char value) const {
        const StringStorage* s = getStorageRaw();
        return ss_indexOfChar(s, value);
    }
    
    int IndexOf(char value, int startIndex) const {
        const StringStorage* s = getStorageRaw();
        return ss_indexOfCharFrom(s, value, startIndex);
    }
    
    int LastIndexOf(const String& value) const {
        const StringStorage* s = getStorageRaw();
        const StringStorage* needle = value.getStorageRaw();
        if (!needle || needle->lenB == 0) return true;
        return ss_lastIndexOf(s, needle);
    }
    
    int LastIndexOf(char value) const {
        const StringStorage* s = getStorageRaw();
        return ss_lastIndexOfChar(s, value);
    }
    
    bool Contains(const String& value) const {
        const StringStorage* s = getStorageRaw();
        const StringStorage* needle = value.getStorageRaw();
        if (!needle || needle->lenB == 0) return true;
        return ss_contains(s, needle);
    }
    
    bool StartsWith(const String& value) const {
        const StringStorage* s = getStorageRaw();
        const StringStorage* prefix = value.getStorageRaw();
        if (!prefix || prefix->lenB == 0) return true;
        return ss_startsWith(s, prefix);
    }
    
    bool EndsWith(const String& value) const {
        const StringStorage* s = getStorageRaw();
        const StringStorage* suffix = value.getStorageRaw();
        if (!suffix || suffix->lenB == 0) return true;
        return ss_endsWith(s, suffix);
    }
    
    // C# String API - Manipulation methods
    String Substring(int startIndex) const {
        const StringStorage* s = getStorageRaw();
        if (!s) return String();

        StringStorage* result = ss_substring(s, startIndex, malloc);
        return wrapStorage(result, *this);
    }

    String Substring(int startIndex, int length) const {
        const StringStorage* s = getStorageRaw();
        if (!s) return String();

        StringStorage* result_ss = ss_substringLen(s, startIndex, length, malloc);
        return wrapStorage(result_ss, *this);
    }
    
    String Left(int chars) const {
    	return Substring(0, chars);
    }
    
    String Right(int chars) const {
    	int len = Length();
    	if (chars > len) return *this;
    	return Substring(len - chars, chars);
    }
    
    String Insert(int startIndex, const String& value) const {
        const StringStorage* s = getStorageRaw();
        const StringStorage* v = value.getStorageRaw();
        if (!s || !v) return String();

        StringStorage* result_ss = ss_insert(s, startIndex, v, malloc);
		return wrapStorage(result_ss, *this);
    }

    String Remove(int startIndex) const {
        const StringStorage* s = getStorageRaw();
        if (!s) return String();

        StringStorage* result_ss = ss_remove(s, startIndex, malloc);
		return wrapStorage(result_ss, *this);
    }

    String Remove(int startIndex, int count) const {
        const StringStorage* s = getStorageRaw();
        if (!s) return String();

        StringStorage* result_ss = ss_removeLen(s, startIndex, count, malloc);
		return wrapStorage(result_ss, *this);
    }

    String Replace(const String& oldValue, const String& newValue) const {
        const StringStorage* s = getStorageRaw();
        const StringStorage* oldVal = oldValue.getStorageRaw();
        const StringStorage* newVal = newValue.getStorageRaw();

        StringStorage* result_ss = ss_replace(s, oldVal, newVal, malloc);
		return wrapStorage(result_ss, *this);
    }

    String Replace(char oldChar, char newChar) const {
        const StringStorage* s = getStorageRaw();
        if (!s) return String();

        StringStorage* result_ss = ss_replaceByte(s, oldChar, newChar, malloc);
		return wrapStorage(result_ss, *this);
    }
    
    // C# String API - Case conversion
    String ToLower() const {
        const StringStorage* s = getStorageRaw();
        if (!s) return String();

        StringStorage* result_ss = ss_toLower(s, malloc);
        return wrapStorage(result_ss, *this);
    }

    String ToUpper() const {
        const StringStorage* s = getStorageRaw();
        if (!s) return String();

        StringStorage* result_ss = ss_toUpper(s, malloc);
        return wrapStorage(result_ss, *this);
    }

    // C# String API - Trimming
    String Trim() const {
        const StringStorage* s = getStorageRaw();
        if (!s) return String();

        StringStorage* result_ss = ss_trim(s, malloc);
        return wrapStorage(result_ss, *this);
    }

    String TrimStart() const {
        const StringStorage* s = getStorageRaw();
        if (!s) return String();

        StringStorage* result_ss = ss_trimStart(s, malloc);
        return wrapStorage(result_ss, *this);
    }

    String TrimEnd() const {
        const StringStorage* s = getStorageRaw();
        if (!s) return String();

        StringStorage* result_ss = ss_trimEnd(s, malloc);
        return wrapStorage(result_ss, *this);
    }
    
    // C# String API - Splitting (caller must free returned array and its contents)
    std::vector<String> Split(char separator) const {
        const StringStorage* s = getStorageRaw();
        if (!s) return {};
        
        int count;
        StringStorage** parts = ss_split(s, separator, &count, malloc);
        if (!parts) return {};
        
        std::vector<String> result(count);
        for (int i = 0; i < count; i++) {
            result[i] = fromMallocStorage(parts[i]);
        }
        
        return result;
    }
    
    std::vector<String> Split(const String& separator) const {
        const StringStorage* s = getStorageRaw();
        const StringStorage* sep = separator.getStorageRaw();
        if (!s || !sep) return {};
        
        int count = 0;
        StringStorage** parts = ss_splitStr(s, sep, &count, malloc);
        if (!parts) return {};
        
        std::vector<String> result(count);
        for (int i = 0; i < count; i++) {
            result[i] = fromMallocStorage(parts[i]);
        }
        free(parts);
        
        return result;
    }
    
    // List-based Split methods - return List<String> instead of String*
    List<String> SplitToList(char separator, uint8_t listPool = 0) const {
        const StringStorage* s = getStorageRaw();
        List<String> result(listPool);
        if (!s) return result;
        
        int count = 0;
        StringStorage** parts = ss_split(s, separator, &count, malloc);
        if (!parts) return result;
        
        // Add all parts to the list
		// OFI: add some List API to let us directly set the capacity to `count`
        for (int i = 0; i < count; i++) {
            String part = fromMallocStorage(parts[i]);
            result.Add(part);
        }
        
        free(parts);  // Free the array, but not the contents (transferred to List)
        return result;
    }
    
    List<String> SplitToList(const String& separator, uint8_t listPool = 0) const {
        const StringStorage* s = getStorageRaw();
        const StringStorage* sep = separator.getStorageRaw();
        List<String> result(listPool);
        if (!s || !sep) return result;
        
        int count = 0;
        StringStorage** parts = ss_splitStr(s, sep, &count, malloc);
        if (!parts) return result;
        
        // Add all parts to the list
        for (int i = 0; i < count; i++) {
            String part = fromMallocStorage(parts[i]);
            result.Add(part);
        }
        
        free(parts);
        return result;
    }
    
    // C# String API - Static methods (as regular methods)
    static bool IsNullOrEmpty(const String& s) { return s.Length() == 0; }
    
    bool IsNullOrWhiteSpace() const {
        const StringStorage* s = getStorageRaw();
        return ss_isNullOrWhiteSpace(s);
    }
    
    // C# String API - Comparison (case-sensitive)
    bool Equals(const String& other) const { return *this == other; }
    
    int Compare(const String& other) const {
        const StringStorage* s1 = getStorageRaw();
        const StringStorage* s2 = other.getStorageRaw();
        if (!s1 && !s2) return 0;
        if (!s1) return -1;
        if (!s2) return 1;
        return ss_compare(s1, s2);
    }
    
    // C# String API - Static methods (Join)
    static String Join(const String& separator, const std::vector<String> values) {
    	int count = (int)values.size();
        if (values.empty() || count <= 0) return String();
        if (count == 1) return values[0];
        
        // Calculate total length needed
        int totalLength = 0;
        const StringStorage* sepStorage = separator.getStorageRaw();
        int sepLength = sepStorage ? ss_lengthB(sepStorage) : 0;
        
        for (int i = 0; i < count; i++) {
            const StringStorage* valueStorage = values[i].getStorageRaw();
            if (valueStorage) {
                totalLength += ss_lengthB(valueStorage);
            }
        }
        totalLength += sepLength * (count - 1); // separators between elements
        
        if (totalLength == 0) return String();
        
        // Build the result String
        char* result = (char*)malloc(totalLength + 1); // malloc valid here because we free it below
        if (!result) return String();
        
        int pos = 0;
        const char* sepCStr = ss_getCString(sepStorage);
        
        for (int i = 0; i < count; i++) {
            if (i > 0 && sepLength > 0) {
                strcpy(result + pos, sepCStr);  // ToDo: replace with memcpy
                pos += sepLength;
            }
            
            const StringStorage* valueStorage = values[i].getStorageRaw();
            if (valueStorage) {
                const char* valueCStr = ss_getCString(valueStorage);
                int valueLength = ss_lengthB(valueStorage);
                strcpy(result + pos, valueCStr);  // ToDo: replace with memcpy
                pos += valueLength;
            }
        }
        result[totalLength] = '\0';
        
        // Create String from result and clean up
        String joined(result);
        ::free(result);		// free valid here because it came from malloc, above
        return joined;
    }
    
    // Join overload that takes List<String>
    static String Join(const String& separator, const List<String>& values) {
        int count = values.Count();
        if (count <= 0) return String();
        if (count == 1) return values[0];
        
        // Calculate total length needed
        int totalLength = 0;
        const StringStorage* sepStorage = separator.getStorageRaw();
        int sepLength = sepStorage ? ss_lengthB(sepStorage) : 0;
        
        for (int i = 0; i < count; i++) {
            const StringStorage* valueStorage = values[i].getStorageRaw();
            if (valueStorage) {
                totalLength += ss_lengthB(valueStorage);
            }
        }
        totalLength += sepLength * (count - 1); // separators between elements
        
        if (totalLength == 0) return String();
        
        // Build the result String
        char* result = (char*)malloc(totalLength + 1);	// malloc valid here because we free it below
        if (!result) return String();
        
        int pos = 0;
        const char* sepCStr = sepStorage ? ss_getCString(sepStorage) : "";
        
        for (int i = 0; i < count; i++) {
            if (i > 0 && sepLength > 0) {
                strcpy(result + pos, sepCStr);
                pos += sepLength;
            }
            
            const StringStorage* valueStorage = values[i].getStorageRaw();
            if (valueStorage) {
                const char* valueCStr = ss_getCString(valueStorage);
                int valueLength = ss_lengthB(valueStorage);
                strcpy(result + pos, valueCStr);
                pos += valueLength;
            }
        }
        result[totalLength] = '\0';
        
        // Create String from result and clean up
        String joined(result);
        ::free(result);					// free valid here because it came from malloc, above
        return joined;
    }
};

// C string + String concatenation
inline String operator+(const char* lhs, const String& rhs) {
    if (!lhs) return rhs;
	if (String::IsNullOrEmpty(rhs)) return String(lhs);
    String temp(lhs);		// OFI: use ss_ methods to avoid creating a temp String
    return temp + rhs;
}

// Free (i.e. global) ToString methods for atomic types
inline String ToString(double d, const char *format=nullptr) {
	if (!format) format = "%g";
	char str[32];
	snprintf(str, 32, format, d);
	return String(str);
}

inline String ToString(int i) {
	char str[32];
	snprintf(str, 32, "%d", i);
	return String(str);
}

// String interpolation function - forward declarations for variadic template
namespace InterpImpl {
	// Helper to convert any argument to String
	inline String ArgToString(int val) { return ToString(val); }
	inline String ArgToString(unsigned int val) { return ToString((int)val); }
	inline String ArgToString(double val) { return ToString(val); }
	inline String ArgToString(const char* val) { return String(val ? val : ""); }
	inline String ArgToString(const String& val) { return val; }

	// Helper to format a double with a format specifier (e.g., "0.00" -> 2 decimal places)
	inline String FormatDouble(double val, const char* formatSpec);
}

// String interpolation - replaces {} placeholders with arguments
// Supports format specifiers like {0.00} for doubles
// Usage: Interp("Value: {}", 42) -> "Value: 42"
//        Interp("Pi: {0.00}", 3.14159) -> "Pi: 3.14"
template<typename... Args>
String Interp(const char* format, Args... args);

// Hash function for String (used by Dictionary<String, TValue>)
inline int Hash(const String& str) {
    const StringStorage* storage = str.getStorageRaw();
    return (int)(ss_hash(const_cast<StringStorage*>(storage)) & 0x7FFFFFFF);
}

