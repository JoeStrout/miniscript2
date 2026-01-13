// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: Disassembler.cs

#pragma once
#include "core_includes.h"
#include "Bytecode.g.h"

namespace MiniScript {

// FORWARD DECLARATIONS

struct CallInfo;
class CallInfoStorage;
struct VM;
class VMStorage;
struct VMVis;
class VMVisStorage;
struct Assembler;
class AssemblerStorage;
struct FuncDef;
class FuncDefStorage;
struct App;
class AppStorage;

// DECLARATIONS


class Disassembler {

	// Return the short pseudo-opcode for the given instruction
	// (e.g.: LOAD instead of LOAD_rA_iBC, etc.)
	public: static String AssemOp(Opcode opcode);
	
	public: static String ToString(UInt32 instruction);

	// Disassemble the given function.  If detailed=true, include extra
	// details for debugging, like line numbers and instruction hex code.
	public: static void Disassemble(FuncDef funcDef, List<String> output, Boolean detailed=true);

	// Disassemble the whole program (list of functions).  If detailed=true, include 
	// extra details for debugging, like line numbers and instruction hex code.
	public: static List<String> Disassemble(List<FuncDef> functions, Boolean detailed=true);

}; // end of struct Disassembler














// INLINE METHODS

} // end of namespace MiniScript
