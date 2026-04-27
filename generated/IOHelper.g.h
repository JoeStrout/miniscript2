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

	public: static String GetStyleTermCode(TextStyle style);
		
	public: static void SetStyle(TextStyle style);

	public: static void NoteStyleSet(TextStyle style);

	public: static void Print(String message, TextStyle style=TextStyle::Normal);
	
	public: static void PrintNoCR(String message, TextStyle style=TextStyle::Normal);
	
	public: static String Input(String prompt, TextStyle promptStyle=TextStyle::Normal, TextStyle inputStyle=TextStyle::Normal);
	
	public: static List<String> ReadFile(String filePath);
	
}; // end of struct IOHelper

// INLINE METHODS

} // end of namespace MiniScript
