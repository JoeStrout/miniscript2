// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: IOHelper.cs

#pragma once
#include "core_includes.h"
#include "forward_decs.g.h"
// IOHelper
//	This is a simple wrapper for console output on each platform.

namespace MiniScript {

// DECLARATIONS

enum class TextStyle : Int32 {
	Normal,
	Subdued,
	Strong,
	Error
}; // end of enum TextStyle

class IOHelper {
	private: static TextStyle currentStyle;
	private: static bool ansiInitialized;

	private: static void EnsureAnsiEnabled();

	public: static String GetStyleTermCode(TextStyle style);

	public: static void SetStyle(TextStyle style);

	public: static void NoteStyleSet(TextStyle style);

	public: static void Print(String message, TextStyle style=TextStyle::Normal);
	
	// Print to standard error (for usage errors and the like).  Style codes are
	// written to stderr too, so they stay correct when stdout is redirected.
	public: static void PrintErr(String message);

	public: static void PrintNoCR(String message, TextStyle style=TextStyle::Normal);
	
	// Read one line from standard input.  Returns true and sets `result` to the
	// line (without its newline); returns false at end of file, leaving `result`
	// null.  Callers must test the return value rather than the string: EOF and
	// an empty line are different things, and on the C++ side an empty String is
	// easy to confuse with a null one.
	public: static Boolean TryInput(String prompt, String* result, TextStyle promptStyle=TextStyle::Normal, TextStyle inputStyle=TextStyle::Normal);

	// Convenience form of TryInput: returns the line read, or null at end of file.
	public: static String Input(String prompt, TextStyle promptStyle=TextStyle::Normal, TextStyle inputStyle=TextStyle::Normal);
	
	public: static List<String> ReadFile(String filePath);
	
}; // end of struct IOHelper

// INLINE METHODS

} // end of namespace MiniScript
