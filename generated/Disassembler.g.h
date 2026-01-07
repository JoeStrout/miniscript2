// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: Disassembler.cs

#pragma once
#include "core_includes.h"

namespace MiniScript {

// FORWARD DECLARATIONS

struct CallInfo;
class CallInfoStorage;
struct VMVis;
class VMVisStorage;
struct Assembler;
class AssemblerStorage;
struct FuncDef;
class FuncDefStorage;
struct App;
class AppStorage;
struct Lexer;
class LexerStorage;

// DECLARATIONS


class Disassembler {
	
		// Return the short pseudo-opcode for the given instruction
		// (e.g.: LOAD instead of LOAD_rA_iBC, etc.)
	public: static String AssemOp(Opcode opcode);

}; // end of struct Disassembler














// INLINE METHODS

