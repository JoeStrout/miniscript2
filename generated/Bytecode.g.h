// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: Bytecode.cs

#pragma once
#include "core_includes.h"

namespace MiniScript {

// FORWARD DECLARATIONS


// DECLARATIONS


// Opcodes.  Note that these must have sequential values, starting at 0.
enum class Opcode : Byte {
	NOOP = 0,
	LOAD_rA_rB,
	LOAD_rA_iBC,
	LOAD_rA_kBC,
	LOADV_rA_rB_kC,
	LOADC_rA_rB_kC,
	FUNCREF_iA_iBC,
	ASSIGN_rA_rB_kC,
	NAME_rA_kBC,
	ADD_rA_rB_rC,
	SUB_rA_rB_rC,
	MULT_rA_rB_rC,	// ToDo: rename this MUL, to match ADD SUB DIV and MOD
	DIV_rA_rB_rC,
	MOD_rA_rB_rC,
	LIST_rA_iBC,
	MAP_rA_iBC,
	PUSH_rA_rB,
	INDEX_rA_rB_rC,
	IDXSET_rA_rB_rC,
	LOCALS_rA,
	OUTER_rA,
	GLOBALS_rA,
	JUMP_iABC,
	LT_rA_rB_rC,
	LT_rA_rB_iC,
	LT_rA_iB_rC,
	LE_rA_rB_rC,
	LE_rA_rB_iC,
	LE_rA_iB_rC,
	EQ_rA_rB_rC,
	EQ_rA_rB_iC,
	NE_rA_rB_rC,
	NE_rA_rB_iC,
	BRTRUE_rA_iBC,
	BRFALSE_rA_iBC,
	BRLT_rA_rB_iC,
	BRLT_rA_iB_iC,
	BRLT_iA_rB_iC,
	BRLE_rA_rB_iC,
	BRLE_rA_iB_iC,
	BRLE_iA_rB_iC,
	BREQ_rA_rB_iC,
	BREQ_rA_iB_iC,
	BRNE_rA_rB_iC,
	BRNE_rA_iB_iC,
	IFLT_rA_rB,
	IFLT_rA_iBC,
	IFLT_iAB_rC,
	IFLE_rA_rB,
	IFLE_rA_iBC,
	IFLE_iAB_rC,
	IFEQ_rA_rB,
	IFEQ_rA_iBC,
	IFNE_rA_rB,
	IFNE_rA_iBC,
	ARGBLK_iABC,
	ARG_rA,
	ARG_iABC,
	CALLF_iA_iBC,
	CALLFN_iA_kBC,
	CALL_rA_rB_rC,
	RETURN,
	OP__COUNT  // Not an opcode, but rather how many opcodes we have.
}; // end of enum Opcode


class BytecodeUtil {
	// Instruction field extraction helpers
	public: static Byte OP(UInt32 instruction) { return (Byte)((instruction >> 24) & 0xFF); }
	
	// 8-bit field extractors
	public: static Byte Au(UInt32 instruction) { return (Byte)((instruction >> 16) & 0xFF); }
	public: static Byte Bu(UInt32 instruction) { return (Byte)((instruction >> 8) & 0xFF); }
	public: static Byte Cu(UInt32 instruction) { return (Byte)(instruction & 0xFF); }
	
	public: static SByte As(UInt32 instruction) { return (SByte)Au(instruction); }
	public: static SByte Bs(UInt32 instruction) { return (SByte)Bu(instruction); }
	public: static SByte Cs(UInt32 instruction) { return (SByte)Cu(instruction); }
	
	// 16-bit field extractors
	public: static UInt16 ABu(UInt32 instruction) { return (UInt16)((instruction >> 8) & 0xFFFF); }
	public: static Int16 ABs(UInt32 instruction) { return (Int16)ABu(instruction); }
	public: static UInt16 BCu(UInt32 instruction) { return (UInt16)(instruction & 0xFFFF); }
	public: static Int16 BCs(UInt32 instruction) { return (Int16)BCu(instruction); }
	
	// 24-bit field extractors
	public: static UInt32 ABCu(UInt32 instruction) { return instruction & 0xFFFFFF; }
	public: static Int32 ABCs(UInt32 instruction);
	
	// Instruction encoding helpers
	public: static UInt32 INS(Opcode op) { return (UInt32)((Byte)op << 24); }
	public: static UInt32 INS_A(Opcode op, Byte a) { return (UInt32)(((Byte)op << 24) | (a << 16)); }
	public: static UInt32 INS_AB(Opcode op, Byte a, Int16 bc) { return (UInt32)(((Byte)op << 24) | (a << 16) | ((UInt16)bc)); }
	public: static UInt32 INS_BC(Opcode op, Int16 ab, Byte c) { return (UInt32)(((Byte)op << 24) | ((UInt16)ab << 8) | c); } // Note: ab is casted to (UInt16) instead of (Int16) in the encoding to avoid padding with 1's which overwrites the opcode. We could also use & instead.
	public: static UInt32 INS_ABC(Opcode op, Byte a, Byte b, Byte c) { return (UInt32)(((Byte)op << 24) | (a << 16) | (b << 8) | c); }
	
	// Conversion to/from opcode mnemonics (names)
	public: static String ToMnemonic(Opcode opcode);
	
	public: static Opcode FromMnemonic(String s);
}; // end of struct BytecodeUtil


// INLINE METHODS



} // end of namespace MiniScript

