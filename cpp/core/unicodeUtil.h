// Functions for working with Unicode text in UTF-8 format.

#ifndef UNICODEUTIL_H
#define UNICODEUTIL_H

#include <stdint.h>
#include <stdbool.h>

// This file is part of Layer 0 (foundation utilities)
#define CORE_LAYER_0

#ifdef __cplusplus
extern "C" {
#endif


// IsUTF8IntraChar
//
// Return whether this is an intra-character byte (in UTF-8 encoding),
// i.e., it's not the first or only byte of a character, but some subsequent
// byte of a multi-byte character.
//
// Gets: charByte -- byte value to consider
// Returns: true iff charByte is not the first byte of a character
static inline bool IsUTF8IntraChar(const unsigned char charByte)
{
    // It's an intra-char character if its high 2 bits are set to 10.
    return 0x80 == (charByte & 0xC0);
}

// AdvanceUTF8
//
// Advance the character pointer within a UTF-8 buffer until it hits
// a limit defined by another character pointer, or has advanced the
// given number of characters.
//
// Gets: c -- address of a pointer to advance
//       maxc -- pointer beyond which *c won't be moved
//       count -- how many characters to advance
// Returns: <nothing>
void AdvanceUTF8(unsigned char **c, const unsigned char *maxc, int count);

// BackupUTF8
//
// Back up the character pointer within a UTF-8 buffer until it hits
// a limit defined by another character pointer, or has backed up the
// given number of characters.
//
// Gets: c -- address of a pointer to back up
//       minc -- pointer beyond which *c won't be moved
//       count -- how many characters to back up
// Returns: <nothing>
void BackupUTF8(unsigned char **c, const unsigned char *minc, int count);

// UTF8Encode
//
// Encode the given Unicode code point in UTF-8 form, followed by a
// null terminator. This requires up to 5 bytes.
//
// Gets: uniChar -- Unicode code point (between 0 and 0x1FFFFF, inclusive)
//       outBuf -- pointer to buffer at least 5 bytes long
// Returns: how many bytes were used (not counting the null), i.e., character length in bytes
int UTF8Encode(unsigned long uniChar, unsigned char *outBuf);

// UTF8Decode
//
// Decode the first character of the given UTF-8 string back into its
// Unicode code point. This is the inverse of UTF8Encode.
//
// Gets: inBuf -- pointer to buffer containing at least one UTF-8 character
// Returns: Unicode code point (between 0 and 0x1FFFFF, inclusive)
unsigned long UTF8Decode(unsigned char *inBuf);

// UTF8DecodeAndAdvance
//
// Decode the first character of the given UTF-8 string back into its
// Unicode code point, and advance the given pointer to the next character.
// This is like calling UTF8Decode followed by AdvanceUTF8, but is more
// efficient.
//
// Gets: inBuf -- address of pointer to buffer containing at least one UTF-8 character
// Returns: Unicode code point (between 0 and 0x1FFFFF, inclusive)
unsigned long UTF8DecodeAndAdvance(unsigned char **inBuf);

// UTF8CharacterCount
//
// Count the number of Unicode characters in a UTF-8 string.
//
// Gets: utf8String -- pointer to UTF-8 string
//       byteCount -- number of bytes in the string
// Returns: number of Unicode characters
int UTF8CharacterCount(const unsigned char *utf8String, int byteCount);

// UTF8ByteIndexToCharIndex
//
// Convert a byte index within a UTF-8 string to a character index.
//
// Gets: utf8String -- pointer to UTF-8 string
//       byteIndex -- byte index within the string
//       maxBytes -- maximum number of bytes in the string
// Returns: character index, or -1 if byteIndex is invalid
int UTF8ByteIndexToCharIndex(const unsigned char *utf8String, int byteIndex, int maxBytes);

// UTF8CharIndexToByteIndex
//
// Convert a character index within a UTF-8 string to a byte index.
//
// Gets: utf8String -- pointer to UTF-8 string
//       charIndex -- character index within the string
//       maxBytes -- maximum number of bytes in the string
// Returns: byte index, or -1 if charIndex is invalid
int UTF8CharIndexToByteIndex(const unsigned char *utf8String, int charIndex, int maxBytes);

// Various case-conversion utilities

unsigned long UnicodeCharToUpper(unsigned long low);
unsigned long UnicodeCharToLower(unsigned long upper);

// Validation function to check if case tables are properly sorted for binary search
bool UnicodeValidateCaseTables(void);
bool UTF8ToUpper(unsigned char *utf8String, unsigned long byteCount,
				 unsigned char **outBuf, unsigned long *outByteCount);
bool UTF8ToLower(unsigned char *utf8String, unsigned long byteCount,
				 unsigned char **outBuf, unsigned long *outByteCount);
bool UTF8Capitalize(unsigned char *utf8String, unsigned long byteCount,
				 unsigned char **outBuf, unsigned long *outByteCount);
bool UTF8IsCaseless(unsigned char *utf8String, unsigned long byteCount);


// UnicodeCharCompare
//
//	Compare the two Unicode code points as for sorting.  True sorting is
//	extremely difficult as the preferred sort order varies by language
//	and region.  So, for now, we just sort lower Unicode code points
//	before higher ones.
//
// Gets: leftChar -- left character to compare
//		 rightChar -- right chracter to compare
// Returns: -1 if leftChar < rightChar, 0 if equal, 1 if leftChar > rightChar
static inline long UnicodeCharCompare(unsigned long leftChar, unsigned long rightChar)
{
	if (leftChar == rightChar) return 0;
	if (leftChar < rightChar) return -1;
	if (leftChar > rightChar) return 1;
	return 0;
}

// String comparisons
long UTF8StringCompare(unsigned char *leftBuf, unsigned long leftByteCount,
					   unsigned char *rightBuf, unsigned long rightByteCount);

// UnicodeCharIsWhitespace
//
// Return whether the given Unicode character should be considered whitespace.
//
// Gets: uniChar -- Unicode code point (between 0 and 0x1FFFFF, inclusive)
// Returns: true if it's whitespace, false if not
static inline bool UnicodeCharIsWhitespace(unsigned long uniChar)
{
    // From the list of whitespace on the Unicode webpage, found
	// at: <http://www.unicode.org/Public/UNIDATA/PropList.txt>
	//
	// 0009..000D    ; White_Space # Cc   [5] <control-0009>..<control-000D>
	// 0020          ; White_Space # Zs       SPACE
	// 0085          ; White_Space # Cc       <control-0085>
	// 00A0          ; White_Space # Zs       NO-BREAK SPACE
	// 1680          ; White_Space # Zs       OGHAM SPACE MARK
	// 180E          ; White_Space # Zs       MONGOLIAN VOWEL SEPARATOR
	// 2000..200A    ; White_Space # Zs  [11] EN QUAD..HAIR SPACE
	// 2028          ; White_Space # Zl       LINE SEPARATOR
	// 2029          ; White_Space # Zp       PARAGRAPH SEPARATOR
	// 202F          ; White_Space # Zs       NARROW NO-BREAK SPACE
	// 205F          ; White_Space # Zs       MEDIUM MATHEMATICAL SPACE
	// 3000          ; White_Space # Zs       IDEOGRAPHIC SPACE
    return ((uniChar >= 0x9 && uniChar <= 0xD) ||
        (uniChar >= 0x2000 && uniChar <= 0x200A) ||
        uniChar == 0x20 || uniChar == 0x85 ||
        uniChar == 0xA0 || uniChar == 0x1680 ||
        uniChar == 0x180E || uniChar == 0x2028 ||
        uniChar == 0x2029 || uniChar == 0x202F ||
        uniChar == 0x205F || uniChar == 0x3000);
}

// FNV-1a hash function
uint32_t fnv1a_hash(const char* data, int len);

#ifdef __cplusplus
}
#endif

#endif // UNICODEUTIL_H