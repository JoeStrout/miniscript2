// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: Assembler.cs

#ifndef __ASSEMBLER_H
#define __ASSEMBLER_H

#include "core_includes.h"
#include "value.h"
#include "value_string.h"
#include "Bytecode.g.h"
#include "FuncDef.g.h"
#include "StringUtils.g.h"
#include "MemPoolShim.g.h"
#include "CS_value_util.h"
#include <climits>


namespace MiniScript {

	class Assembler {

		// Multiple functions support
		public: List<FuncDef> Functions; // all functions
		public: FuncDef Current; // function we are currently building
		private: List<String> _labelNames; // label names within current function
		private: List<Int32> _labelAddresses; // corresponding instruction addresses within current function
		
		// Error handling state
		public: Boolean HasError;
		public: String ErrorMessage;
		public: Int32 CurrentLineNumber;
		public: String CurrentLine;

		public: Assembler();

		// Helper to find a function by name (returns -1 if not found)
		public: Int32 FindFunctionIndex(String name);

		// Helper to find a function by name.
		// C#: returns null if not found; C++: returns an empty FuncDef.
		public: FuncDef FindFunction(String name);

		// Helper to check if a function exists
		public: Boolean HasFunction(String name);

		public: static List<String> GetTokens(String line);
		
		private: void SetCurrentLine(Int32 lineNumber, String line);
		
		public: void ClearError();
		
		private: void Error(String errMsg);
		
		// Assemble a single source line, add to our current function,
		// and also return its value (mainly for unit testing).
		public: UInt32 AddLine(String line);
		
		public: UInt32 AddLine(String line, Int32 lineNumber);
		
		// Helper to parse register like "r5" -> 5
		private: Byte ParseRegister(String reg);

		// Helper to parse a Byte number (handles negative numbers)
		private: Byte ParseByte(String num);
		
		// Helper to parse an Int16 number (handles negative numbers)
		private: Int16 ParseInt16(String num);

		// Helper to parse a 24-bit int number (handles negative numbers)
		private: Int32 ParseInt24(String num);

		// Helper to parse a 32-bit int number (handles negative numbers)
		private: Int32 ParseInt32(String num);

		// Helper to determine if a token is a label (ends with colon)
		private: static Boolean IsLabel(String token);

		// Helper to determine if a token is a function label (starts with @ and ends with colon)
		private: static Boolean IsFunctionLabel(String token);
		
		// Extract just the label part, e.g. "someLabel:" --> "someLabel"
		private: static String ParseLabel(String token);

		// Add a new empty function to our function list.
		// Return true on success, false if failed (because function already exists).
		private: bool AddFunction(String functionName);

		// Helper to find the address of a label
		private: Int32 FindLabelAddress(String labelName);

		// Helper to add a constant to the constants table and return its index
		private: Int32 AddConstant(Value value);

		// Helper to check if a token is a string literal (surrounded by quotes)
		private: static Boolean IsStringLiteral(String token);

		// Helper to check if a token needs to be stored as a constant
		// (string literals, floating point numbers, or integers too large for Int16)
		private: static Boolean NeedsConstant(String token);

		// Helper to create a Value from a token
		private: Value ParseAsConstant(String token);

		// Helper to parse a double from a string (basic implementation)
		private: Double ParseDouble(String str);

		// Multi-function assembly with support for @function: labels
		public: void Assemble(List<String> sourceLines);

		// Assemble a single function from sourceLines, starting at startLine,
		// and proceeding until we hit another function label or run out of lines.
		// Return the line number after this function ends.
		private: Int32 AssembleFunction(List<String> sourceLines, Int32 startLine);


	}; // end of class Assembler
}

#endif // __ASSEMBLER_H
