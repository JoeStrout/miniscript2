// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: IOHelper.cs

#pragma once
#include "core_includes.h"
#include "forward_decs.g.h"
// IOHelper
//	This is a simple wrapper for console output on each platform.

namespace MiniScript {

// DECLARATIONS

class IOHelper {

	public: static void Print(String message);
	
	public: static String Input(String prompt);
	
	public: static List<String> ReadFile(String filePath);
	
}; // end of struct IOHelper

// INLINE METHODS

} // end of namespace MiniScript
