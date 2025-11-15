using System;
using System.Collections.Generic;
// CPP: #include "value.h"

namespace MiniScript {
	// Opcodes.  Note that these must have sequential values, starting at 0.
	public enum Opcode : Byte {
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
	}

	public static class BytecodeUtil {
		// Instruction field extraction helpers
		public static Byte OP(UInt32 instruction) => (Byte)((instruction >> 24) & 0xFF);
		
		// 8-bit field extractors
		public static Byte Au(UInt32 instruction) => (Byte)((instruction >> 16) & 0xFF);
		public static Byte Bu(UInt32 instruction) => (Byte)((instruction >> 8) & 0xFF);
		public static Byte Cu(UInt32 instruction) => (Byte)(instruction & 0xFF);
		
		public static SByte As(UInt32 instruction) => (SByte)Au(instruction);
		public static SByte Bs(UInt32 instruction) => (SByte)Bu(instruction);
		public static SByte Cs(UInt32 instruction) => (SByte)Cu(instruction);
		
		// 16-bit field extractors
		public static UInt16 ABu(UInt32 instruction) => (UInt16)((instruction >> 8) & 0xFFFF);
		public static Int16 ABs(UInt32 instruction) => (Int16)ABu(instruction);
		public static UInt16 BCu(UInt32 instruction) => (UInt16)(instruction & 0xFFFF);
		public static Int16 BCs(UInt32 instruction) => (Int16)BCu(instruction);
		
		// 24-bit field extractors
		public static UInt32 ABCu(UInt32 instruction) => instruction & 0xFFFFFF;
		public static Int32 ABCs(UInt32 instruction) {
			UInt32 value = ABCu(instruction);
			// If bit 23 is set (sign bit), extend the sign to upper 8 bits
			if ((value & 0x800000) != 0) {
				return (Int32)(value | 0xFF000000);
			}
			return (Int32)value;
		}
		
		// Instruction encoding helpers
		public static UInt32 INS(Opcode op) => (UInt32)((Byte)op << 24);
		public static UInt32 INS_A(Opcode op, Byte a) => (UInt32)(((Byte)op << 24) | (a << 16));
		public static UInt32 INS_AB(Opcode op, Byte a, Int16 bc) => (UInt32)(((Byte)op << 24) | (a << 16) | ((UInt16)bc));
		public static UInt32 INS_BC(Opcode op, Int16 ab, Byte c) => (UInt32)(((Byte)op << 24) | ((UInt16)ab << 8) | c); // Note: ab is casted to (UInt16) instead of (Int16) in the encoding to avoid padding with 1's which overwrites the opcode. We could also use & instead.
		public static UInt32 INS_ABC(Opcode op, Byte a, Byte b, Byte c) => (UInt32)(((Byte)op << 24) | (a << 16) | (b << 8) | c);
		
		// Conversion to/from opcode mnemonics (names)
		public static String ToMnemonic(Opcode opcode) {
			switch (opcode) {
				case Opcode.NOOP:           return "NOOP";
				case Opcode.LOAD_rA_rB:     return "LOAD_rA_rB";
				case Opcode.LOAD_rA_iBC:    return "LOAD_rA_iBC";
				case Opcode.LOAD_rA_kBC:    return "LOAD_rA_kBC";
				case Opcode.LOADV_rA_rB_kC: return "LOADV_rA_rB_kC";
				case Opcode.LOADC_rA_rB_kC: return "LOADC_rA_rB_kC";
				case Opcode.FUNCREF_iA_iBC: return "FUNCREF_iA_iBC";
				case Opcode.ASSIGN_rA_rB_kC:return "ASSIGN_rA_rB_kC";
				case Opcode.NAME_rA_kBC:    return "NAME_rA_kBC";
				case Opcode.ADD_rA_rB_rC:   return "ADD_rA_rB_rC";
				case Opcode.SUB_rA_rB_rC:   return "SUB_rA_rB_rC";
				case Opcode.MULT_rA_rB_rC:  return "MULT_rA_rB_rC";
				case Opcode.DIV_rA_rB_rC:   return "DIV_rA_rB_rC";
				case Opcode.MOD_rA_rB_rC:   return "MOD_rA_rB_rC";
				case Opcode.LIST_rA_iBC:    return "LIST_rA_iBC";
				case Opcode.MAP_rA_iBC:     return "MAP_rA_iBC";
				case Opcode.PUSH_rA_rB:     return "PUSH_rA_rB";
				case Opcode.INDEX_rA_rB_rC: return "INDEX_rA_rB_rC";
				case Opcode.IDXSET_rA_rB_rC:return "IDXSET_rA_rB_rC";
				case Opcode.LOCALS_rA:      return "LOCALS_rA";
				case Opcode.OUTER_rA:       return "OUTER_rA";
				case Opcode.GLOBALS_rA:     return "GLOBALS_rA";
				case Opcode.JUMP_iABC:      return "JUMP_iABC";
				case Opcode.LT_rA_rB_rC:    return "LT_rA_rB_rC";
				case Opcode.LT_rA_rB_iC:    return "LT_rA_rB_iC";
				case Opcode.LT_rA_iB_rC:    return "LT_rA_iB_rC";
				case Opcode.LE_rA_rB_rC:    return "LE_rA_rB_rC";
				case Opcode.LE_rA_rB_iC:    return "LE_rA_rB_iC";
				case Opcode.LE_rA_iB_rC:    return "LE_rA_iB_rC";
				case Opcode.EQ_rA_rB_rC:    return "EQ_rA_rB_rC";
				case Opcode.EQ_rA_rB_iC:    return "EQ_rA_rB_iC";
				case Opcode.NE_rA_rB_rC:    return "NE_rA_rB_rC";
				case Opcode.NE_rA_rB_iC:    return "NE_rA_rB_iC";
				case Opcode.BRTRUE_rA_iBC:  return "BRTRUE_rA_iBC";
				case Opcode.BRFALSE_rA_iBC: return "BRFALSE_rA_iBC";
				case Opcode.BRLT_rA_rB_iC:  return "BRLT_rA_rB_iC";
				case Opcode.BRLT_rA_iB_iC:  return "BRLT_rA_iB_iC";
				case Opcode.BRLT_iA_rB_iC:  return "BRLT_iA_rB_iC";
				case Opcode.BRLE_rA_rB_iC:  return "BRLE_rA_rB_iC";
				case Opcode.BRLE_rA_iB_iC:  return "BRLE_rA_iB_iC";
				case Opcode.BRLE_iA_rB_iC:  return "BRLE_iA_rB_iC";
				case Opcode.BREQ_rA_rB_iC:  return "BREQ_rA_rB_iC";
				case Opcode.BREQ_rA_iB_iC:  return "BREQ_rA_iB_iC";
				case Opcode.BRNE_rA_rB_iC:  return "BRNE_rA_rB_iC";
				case Opcode.BRNE_rA_iB_iC:  return "BRNE_rA_iB_iC";
				case Opcode.IFLT_rA_rB:     return "IFLT_rA_rB";
				case Opcode.IFLT_rA_iBC:    return "IFLT_rA_iBC";
				case Opcode.IFLT_iAB_rC:    return "IFLT_iAB_rC";
				case Opcode.IFLE_rA_rB:     return "IFLE_rA_rB";
				case Opcode.IFLE_rA_iBC:    return "IFLE_rA_iBC";
				case Opcode.IFLE_iAB_rC:    return "IFLE_iAB_rC";
				case Opcode.IFEQ_rA_rB:     return "IFEQ_rA_rB";
				case Opcode.IFEQ_rA_iBC:    return "IFEQ_rA_iBC";
				case Opcode.IFNE_rA_rB:     return "IFLT_rA_rB";
				case Opcode.IFNE_rA_iBC:    return "IFLT_rA_iBC";
				case Opcode.ARGBLK_iABC:  return "ARGBLK_iABC";
				case Opcode.ARG_rA:         return "ARG_rA";
				case Opcode.ARG_iABC:       return "ARG_iABC";
				case Opcode.CALLF_iA_iBC:   return "CALLF_iA_iBC";
				case Opcode.CALLFN_iA_kBC:  return "CALLFN_iA_kBC";
				case Opcode.CALL_rA_rB_rC:  return "CALL_rA_rB_rC";
				case Opcode.RETURN:         return "RETURN";
				default:
					return "Unknown opcode";
			}
		}
		
		public static Opcode FromMnemonic(String s) {
			if (s == "NOOP")            return Opcode.NOOP;
			if (s == "LOAD_rA_rB")      return Opcode.LOAD_rA_rB;
			if (s == "LOAD_rA_iBC")     return Opcode.LOAD_rA_iBC;
			if (s == "LOAD_rA_kBC")     return Opcode.LOAD_rA_kBC;
			if (s == "LOADV_rA_rB_kC")  return Opcode.LOADV_rA_rB_kC;
			if (s == "LOADC_rA_rB_kC")  return Opcode.LOADC_rA_rB_kC;
			if (s == "FUNCREF_iA_iBC")  return Opcode.FUNCREF_iA_iBC;
			if (s == "ASSIGN_rA_rB_kC") return Opcode.ASSIGN_rA_rB_kC;
			if (s == "NAME_rA_kBC")     return Opcode.NAME_rA_kBC;
			if (s == "ADD_rA_rB_rC")    return Opcode.ADD_rA_rB_rC;
			if (s == "SUB_rA_rB_rC")    return Opcode.SUB_rA_rB_rC;
			if (s == "MULT_rA_rB_rC")   return Opcode.MULT_rA_rB_rC;
			if (s == "DIV_rA_rB_rC")    return Opcode.DIV_rA_rB_rC;
			if (s == "MOD_rA_rB_rC")    return Opcode.MOD_rA_rB_rC;
			if (s == "LIST_rA_iBC")     return Opcode.LIST_rA_iBC;
			if (s == "MAP_rA_iBC")      return Opcode.MAP_rA_iBC;
			if (s == "PUSH_rA_rB")      return Opcode.PUSH_rA_rB;
			if (s == "INDEX_rA_rB_rC")  return Opcode.INDEX_rA_rB_rC;
			if (s == "IDXSET_rA_rB_rC") return Opcode.IDXSET_rA_rB_rC;
			if (s == "LOCALS_rA")       return Opcode.LOCALS_rA;
			if (s == "OUTER_rA")        return Opcode.OUTER_rA;
			if (s == "GLOBALS_rA")      return Opcode.GLOBALS_rA;
			if (s == "JUMP_iABC")       return Opcode.JUMP_iABC;
			if (s == "LT_rA_rB_rC")     return Opcode.LT_rA_rB_rC;
			if (s == "LT_rA_rB_iC")     return Opcode.LT_rA_rB_iC;
			if (s == "LT_rA_iB_rC")     return Opcode.LT_rA_iB_rC;
			if (s == "LE_rA_rB_rC")     return Opcode.LE_rA_rB_rC;
			if (s == "LE_rA_rB_iC")     return Opcode.LE_rA_rB_iC;
			if (s == "LE_rA_iB_rC")     return Opcode.LE_rA_iB_rC;
			if (s == "EQ_rA_rB_rC")     return Opcode.EQ_rA_rB_rC;
			if (s == "EQ_rA_rB_iC")     return Opcode.EQ_rA_rB_iC;
			if (s == "NE_rA_rB_rC")     return Opcode.NE_rA_rB_rC;
			if (s == "NE_rA_rB_iC")     return Opcode.NE_rA_rB_iC;
			if (s == "BRTRUE_rA_iBC")   return Opcode.BRTRUE_rA_iBC;
			if (s == "BRFALSE_rA_iBC")  return Opcode.BRFALSE_rA_iBC;
			if (s == "BRLT_rA_rB_iC")   return Opcode.BRLT_rA_rB_iC;
			if (s == "BRLT_rA_iB_iC")   return Opcode.BRLT_rA_iB_iC;
			if (s == "BRLT_iA_rB_iC")   return Opcode.BRLT_iA_rB_iC;
			if (s == "BRLE_rA_rB_iC")   return Opcode.BRLE_rA_rB_iC;
			if (s == "BRLE_rA_iB_iC")   return Opcode.BRLE_rA_iB_iC;
			if (s == "BRLE_iA_rB_iC")   return Opcode.BRLE_iA_rB_iC;
			if (s == "BREQ_rA_rB_iC")   return Opcode.BREQ_rA_rB_iC;
			if (s == "BREQ_rA_iB_iC")   return Opcode.BREQ_rA_iB_iC;
			if (s == "BRNE_rA_rB_iC")   return Opcode.BRNE_rA_rB_iC;
			if (s == "BRNE_rA_iB_iC")   return Opcode.BRNE_rA_iB_iC;
			if (s == "IFLT_rA_rB")      return Opcode.IFLT_rA_rB;
			if (s == "IFLT_rA_iBC")     return Opcode.IFLT_rA_iBC;
			if (s == "IFLT_iAB_rC")     return Opcode.IFLT_iAB_rC;
			if (s == "IFLE_rA_rB")      return Opcode.IFLE_rA_rB;
			if (s == "IFLE_rA_iBC")     return Opcode.IFLE_rA_iBC;
			if (s == "IFLE_iAB_rC")     return Opcode.IFLE_iAB_rC;
			if (s == "IFEQ_rA_rB")      return Opcode.IFEQ_rA_rB;
			if (s == "IFEQ_rA_iBC")     return Opcode.IFEQ_rA_iBC;
			if (s == "IFNE_rA_rB")      return Opcode.IFNE_rA_rB;
			if (s == "IFNE_rA_iBC")     return Opcode.IFNE_rA_iBC;
			if (s == "ARGBLK_iABC")     return Opcode.ARGBLK_iABC;
			if (s == "ARG_rA")          return Opcode.ARG_rA;
			if (s == "ARG_iABC")        return Opcode.ARG_iABC;
			if (s == "CALLF_iA_iBC")    return Opcode.CALLF_iA_iBC;
			if (s == "CALLFN_iA_kBC")   return Opcode.CALLFN_iA_kBC;
			if (s == "CALL_rA_rB_rC")   return Opcode.CALL_rA_rB_rC;
			if (s == "RETURN")          return Opcode.RETURN;
			return Opcode.NOOP;
		}
	}

}

