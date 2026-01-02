// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: IOHelper.cs

#pragma once
#include "core_includes.h"
// IOHelper
//	This is a simple wrapper for console output on each platform.


namespace MiniScript {

// FORWARD DECLARATIONS

struct CallInfo;
class CallInfoStorage;
struct AcceptException;
class AcceptExceptionStorage;
struct VMVis;
class VMVisStorage;
struct Assembler;
class AssemblerStorage;
struct FuncDef;
class FuncDefStorage;
struct Lexer;
class LexerStorage;

// DECLARATIONS












class IOHelper {
	public: static void Print(String message);
	public: static String Input(String prompt);
	public: static List<String> ReadFile(String filePath);
}; // end of struct IOHelper




// INLINE METHODS














} // end of namespace MiniScript
