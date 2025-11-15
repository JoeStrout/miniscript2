// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: UnitTests.cs

#ifndef __UNITTESTS_H
#define __UNITTESTS_H

// This module gathers together all the unit tests for this prototype.
// Each test returns true on success, false on failure.

#include "core_includes.h"
#include "IOHelper.g.h"
#include "StringUtils.g.h"
#include "Disassembler.g.h"
#include "Assembler.g.h"  // We really should automate this.

namespace MiniScript {

	class UnitTests {
	
		public: static Boolean Assert(Boolean condition, String message);
		
		public: static Boolean AssertEqual(String actual, String expected);
			
		public: static Boolean AssertEqual(UInt32 actual, UInt32 expected);
			
		public: static Boolean AssertEqual(Int32 actual, Int32 expected);
			
		public: static Boolean AssertEqual(List<String> actual, List<String> expected);
			
		public: static Boolean TestStringUtils();
		
		public: static Boolean TestDisassembler();
		
		public: static Boolean TestAssembler();

		public: static Boolean TestValueMap();

		public: static Boolean RunAll();
	}; // end of class UnitTests

}

#endif // __UNITTESTS_H
