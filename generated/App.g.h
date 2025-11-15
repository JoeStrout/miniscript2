// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: App.cs

#ifndef __APP_H
#define __APP_H

#include "core_includes.h"

#include "UnitTests.g.h"
#include "VM.g.h"
#include "gc.h"
#include "gc_debug_output.h"
#include "value_string.h"
#include "dispatch_macros.h"
#include "VMVis.g.h"
#include "StringPool.h"
#include "MemPoolShim.g.h"
using namespace MiniScript;

class App {
	public: static bool debugMode;
	public: static bool visMode;
	
	public: static void Main(String args[], int argCount);
}; // end of class App


#endif // __APP_H
