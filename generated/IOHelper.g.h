// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: IOHelper.cs

#ifndef __IOHELPER_H
#define __IOHELPER_H

// IOHelper
//	This is a simple wrapper for console output on each platform.

#include "core_includes.h"
#include <iostream>
#include <stdio.h>
#include <fstream>
#include <string>
#include <algorithm>

class IOHelper {

	public: static void Print(String message);
	
	public: static String Input(String prompt);
	
	public: static List<String> ReadFile(String filePath);
	
}; // end of class IOHelper

#endif // __IOHELPER_H
