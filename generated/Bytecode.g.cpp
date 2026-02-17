// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: Bytecode.cs

#include "Bytecode.g.h"
#include "value.h"
#include "IOHelper.g.h"

namespace MiniScript {






Boolean BytecodeUtil::ValidateOpcodes = true;
EmitPattern BytecodeUtil::GetEmitPattern(Opcode opcode) {
	String mnemonic = ToMnemonic(opcode);

	// Check for specific patterns in the mnemonic suffix
	// Order matters: check more specific patterns first

	// ABC patterns: three operands, or two register operands (A and B as separate 8-bit fields)
	// _rA_rB_rC, _rA_rB_iC, _rA_iB_rC, _iA_rB_iC, _rA_rB_kC, _rA_rB (two registers)
	if (mnemonic.Contains("_rA_rB_rC") || mnemonic.Contains("_rA_rB_iC") ||
		mnemonic.Contains("_rA_iB_rC") || mnemonic.Contains("_iA_rB_iC") ||
		mnemonic.Contains("_rA_rB_kC") || mnemonic.EndsWith("_rA_rB")) {
		return EmitPattern::ABC;
	}

	// BC patterns: _iAB_rC, _iAB_iC
	if (mnemonic.Contains("_iAB_rC") || mnemonic.Contains("_iAB_iC")) {
		return EmitPattern::BC;
	}

	// AB patterns: _rA_iBC, _rA_kBC, _iA_iBC, _iA_kBC (one 8-bit field + one 16-bit field)
	if (mnemonic.Contains("_rA_iBC") || mnemonic.Contains("_rA_kBC") ||
		mnemonic.Contains("_iA_iBC") || mnemonic.Contains("_iA_kBC")) {
		return EmitPattern::AB;
	}

	// A patterns: _rA, _iA (but not followed by B or BC)
	if (mnemonic.EndsWith("_rA") || mnemonic.EndsWith("_iA")) {
		return EmitPattern::A;
	}

	// iABC pattern (24-bit immediate, like JUMP_iABC, ARG_iABC, ARGBLK_iABC)
	if (mnemonic.Contains("_iABC")) {
		return EmitPattern::ABC;  // Uses all 24 bits as one value
	}

	// No suffix - opcode only (NOOP, RETURN, etc.)
	return EmitPattern::None;
}
Boolean BytecodeUtil::CheckEmitPattern(Opcode opcode, EmitPattern expected) {
	if (!ValidateOpcodes) return Boolean(true);

	EmitPattern actual = GetEmitPattern(opcode);
	if (actual == expected) return Boolean(true);

	// Mismatch - report error
	String mnemonic = ToMnemonic(opcode);
	IOHelper::Print(Interp("ERROR: Opcode {} expects Emit{} but Emit{} was called", mnemonic, actual, expected));
	return Boolean(false);
}
Int32 BytecodeUtil::ABCs(UInt32 instruction) {
	UInt32 value = ABCu(instruction);
	// If bit 23 is set (sign bit), extend the sign to upper 8 bits
	if ((value & 0x800000) != 0) {
		return (Int32)(value | 0xFF000000);
	}
	return (Int32)value;
}
String BytecodeUtil::ToMnemonic(Opcode opcode) {
	switch (opcode) {
		case Opcode::NOOP:           return "NOOP";
		case Opcode::LOAD_rA_rB:     return "LOAD_rA_rB";
		case Opcode::LOAD_rA_iBC:    return "LOAD_rA_iBC";
		case Opcode::LOAD_rA_kBC:    return "LOAD_rA_kBC";
		case Opcode::LOADV_rA_rB_kC: return "LOADV_rA_rB_kC";
		case Opcode::LOADC_rA_rB_kC: return "LOADC_rA_rB_kC";
		case Opcode::FUNCREF_iA_iBC: return "FUNCREF_iA_iBC";
		case Opcode::ASSIGN_rA_rB_kC:return "ASSIGN_rA_rB_kC";
		case Opcode::NAME_rA_kBC:    return "NAME_rA_kBC";
		case Opcode::ADD_rA_rB_rC:   return "ADD_rA_rB_rC";
		case Opcode::SUB_rA_rB_rC:   return "SUB_rA_rB_rC";
		case Opcode::MULT_rA_rB_rC:  return "MULT_rA_rB_rC";
		case Opcode::DIV_rA_rB_rC:   return "DIV_rA_rB_rC";
		case Opcode::MOD_rA_rB_rC:   return "MOD_rA_rB_rC";
		case Opcode::AND_rA_rB_rC:   return "AND_rA_rB_rC";
		case Opcode::OR_rA_rB_rC:    return "OR_rA_rB_rC";
		case Opcode::NOT_rA_rB:      return "NOT_rA_rB";
		case Opcode::LIST_rA_iBC:    return "LIST_rA_iBC";
		case Opcode::MAP_rA_iBC:     return "MAP_rA_iBC";
		case Opcode::PUSH_rA_rB:     return "PUSH_rA_rB";
		case Opcode::INDEX_rA_rB_rC: return "INDEX_rA_rB_rC";
		case Opcode::IDXSET_rA_rB_rC:return "IDXSET_rA_rB_rC";
		case Opcode::LOCALS_rA:      return "LOCALS_rA";
		case Opcode::OUTER_rA:       return "OUTER_rA";
		case Opcode::GLOBALS_rA:     return "GLOBALS_rA";
		case Opcode::JUMP_iABC:      return "JUMP_iABC";
		case Opcode::LT_rA_rB_rC:    return "LT_rA_rB_rC";
		case Opcode::LT_rA_rB_iC:    return "LT_rA_rB_iC";
		case Opcode::LT_rA_iB_rC:    return "LT_rA_iB_rC";
		case Opcode::LE_rA_rB_rC:    return "LE_rA_rB_rC";
		case Opcode::LE_rA_rB_iC:    return "LE_rA_rB_iC";
		case Opcode::LE_rA_iB_rC:    return "LE_rA_iB_rC";
		case Opcode::EQ_rA_rB_rC:    return "EQ_rA_rB_rC";
		case Opcode::EQ_rA_rB_iC:    return "EQ_rA_rB_iC";
		case Opcode::NE_rA_rB_rC:    return "NE_rA_rB_rC";
		case Opcode::NE_rA_rB_iC:    return "NE_rA_rB_iC";
		case Opcode::BRTRUE_rA_iBC:  return "BRTRUE_rA_iBC";
		case Opcode::BRFALSE_rA_iBC: return "BRFALSE_rA_iBC";
		case Opcode::BRLT_rA_rB_iC:  return "BRLT_rA_rB_iC";
		case Opcode::BRLT_rA_iB_iC:  return "BRLT_rA_iB_iC";
		case Opcode::BRLT_iA_rB_iC:  return "BRLT_iA_rB_iC";
		case Opcode::BRLE_rA_rB_iC:  return "BRLE_rA_rB_iC";
		case Opcode::BRLE_rA_iB_iC:  return "BRLE_rA_iB_iC";
		case Opcode::BRLE_iA_rB_iC:  return "BRLE_iA_rB_iC";
		case Opcode::BREQ_rA_rB_iC:  return "BREQ_rA_rB_iC";
		case Opcode::BREQ_rA_iB_iC:  return "BREQ_rA_iB_iC";
		case Opcode::BRNE_rA_rB_iC:  return "BRNE_rA_rB_iC";
		case Opcode::BRNE_rA_iB_iC:  return "BRNE_rA_iB_iC";
		case Opcode::IFLT_rA_rB:     return "IFLT_rA_rB";
		case Opcode::IFLT_rA_iBC:    return "IFLT_rA_iBC";
		case Opcode::IFLT_iAB_rC:    return "IFLT_iAB_rC";
		case Opcode::IFLE_rA_rB:     return "IFLE_rA_rB";
		case Opcode::IFLE_rA_iBC:    return "IFLE_rA_iBC";
		case Opcode::IFLE_iAB_rC:    return "IFLE_iAB_rC";
		case Opcode::IFEQ_rA_rB:     return "IFEQ_rA_rB";
		case Opcode::IFEQ_rA_iBC:    return "IFEQ_rA_iBC";
		case Opcode::IFNE_rA_rB:     return "IFLT_rA_rB";
		case Opcode::IFNE_rA_iBC:    return "IFLT_rA_iBC";
		case Opcode::NEXT_rA_rB:     return "NEXT_rA_rB";
		case Opcode::ARGBLK_iABC:  return "ARGBLK_iABC";
		case Opcode::ARG_rA:         return "ARG_rA";
		case Opcode::ARG_iABC:       return "ARG_iABC";
		case Opcode::CALLF_iA_iBC:   return "CALLF_iA_iBC";
		case Opcode::CALLFN_iA_kBC:  return "CALLFN_iA_kBC";
		case Opcode::CALL_rA_rB_rC:  return "CALL_rA_rB_rC";
		case Opcode::RETURN:         return "RETURN";
		case Opcode::NEW_rA_rB:      return "NEW_rA_rB";
		case Opcode::ISA_rA_rB_rC:   return "ISA_rA_rB_rC";
		case Opcode::METHFIND_rA_rB_rC: return "METHFIND_rA_rB_rC";
		case Opcode::SETSELF_rA:     return "SETSELF_rA";
		case Opcode::CALLIFREF_rA:   return "CALLIFREF_rA";
		default:
			return "Unknown opcode";
	}
}
Opcode BytecodeUtil::FromMnemonic(String s) {
	if (s == "NOOP")            return Opcode::NOOP;
	if (s == "LOAD_rA_rB")      return Opcode::LOAD_rA_rB;
	if (s == "LOAD_rA_iBC")     return Opcode::LOAD_rA_iBC;
	if (s == "LOAD_rA_kBC")     return Opcode::LOAD_rA_kBC;
	if (s == "LOADV_rA_rB_kC")  return Opcode::LOADV_rA_rB_kC;
	if (s == "LOADC_rA_rB_kC")  return Opcode::LOADC_rA_rB_kC;
	if (s == "FUNCREF_iA_iBC")  return Opcode::FUNCREF_iA_iBC;
	if (s == "ASSIGN_rA_rB_kC") return Opcode::ASSIGN_rA_rB_kC;
	if (s == "NAME_rA_kBC")     return Opcode::NAME_rA_kBC;
	if (s == "ADD_rA_rB_rC")    return Opcode::ADD_rA_rB_rC;
	if (s == "SUB_rA_rB_rC")    return Opcode::SUB_rA_rB_rC;
	if (s == "MULT_rA_rB_rC")   return Opcode::MULT_rA_rB_rC;
	if (s == "DIV_rA_rB_rC")    return Opcode::DIV_rA_rB_rC;
	if (s == "MOD_rA_rB_rC")    return Opcode::MOD_rA_rB_rC;
	if (s == "AND_rA_rB_rC")    return Opcode::AND_rA_rB_rC;
	if (s == "OR_rA_rB_rC")     return Opcode::OR_rA_rB_rC;
	if (s == "NOT_rA_rB")       return Opcode::NOT_rA_rB;
	if (s == "LIST_rA_iBC")     return Opcode::LIST_rA_iBC;
	if (s == "MAP_rA_iBC")      return Opcode::MAP_rA_iBC;
	if (s == "PUSH_rA_rB")      return Opcode::PUSH_rA_rB;
	if (s == "INDEX_rA_rB_rC")  return Opcode::INDEX_rA_rB_rC;
	if (s == "IDXSET_rA_rB_rC") return Opcode::IDXSET_rA_rB_rC;
	if (s == "LOCALS_rA")       return Opcode::LOCALS_rA;
	if (s == "OUTER_rA")        return Opcode::OUTER_rA;
	if (s == "GLOBALS_rA")      return Opcode::GLOBALS_rA;
	if (s == "JUMP_iABC")       return Opcode::JUMP_iABC;
	if (s == "LT_rA_rB_rC")     return Opcode::LT_rA_rB_rC;
	if (s == "LT_rA_rB_iC")     return Opcode::LT_rA_rB_iC;
	if (s == "LT_rA_iB_rC")     return Opcode::LT_rA_iB_rC;
	if (s == "LE_rA_rB_rC")     return Opcode::LE_rA_rB_rC;
	if (s == "LE_rA_rB_iC")     return Opcode::LE_rA_rB_iC;
	if (s == "LE_rA_iB_rC")     return Opcode::LE_rA_iB_rC;
	if (s == "EQ_rA_rB_rC")     return Opcode::EQ_rA_rB_rC;
	if (s == "EQ_rA_rB_iC")     return Opcode::EQ_rA_rB_iC;
	if (s == "NE_rA_rB_rC")     return Opcode::NE_rA_rB_rC;
	if (s == "NE_rA_rB_iC")     return Opcode::NE_rA_rB_iC;
	if (s == "BRTRUE_rA_iBC")   return Opcode::BRTRUE_rA_iBC;
	if (s == "BRFALSE_rA_iBC")  return Opcode::BRFALSE_rA_iBC;
	if (s == "BRLT_rA_rB_iC")   return Opcode::BRLT_rA_rB_iC;
	if (s == "BRLT_rA_iB_iC")   return Opcode::BRLT_rA_iB_iC;
	if (s == "BRLT_iA_rB_iC")   return Opcode::BRLT_iA_rB_iC;
	if (s == "BRLE_rA_rB_iC")   return Opcode::BRLE_rA_rB_iC;
	if (s == "BRLE_rA_iB_iC")   return Opcode::BRLE_rA_iB_iC;
	if (s == "BRLE_iA_rB_iC")   return Opcode::BRLE_iA_rB_iC;
	if (s == "BREQ_rA_rB_iC")   return Opcode::BREQ_rA_rB_iC;
	if (s == "BREQ_rA_iB_iC")   return Opcode::BREQ_rA_iB_iC;
	if (s == "BRNE_rA_rB_iC")   return Opcode::BRNE_rA_rB_iC;
	if (s == "BRNE_rA_iB_iC")   return Opcode::BRNE_rA_iB_iC;
	if (s == "IFLT_rA_rB")      return Opcode::IFLT_rA_rB;
	if (s == "IFLT_rA_iBC")     return Opcode::IFLT_rA_iBC;
	if (s == "IFLT_iAB_rC")     return Opcode::IFLT_iAB_rC;
	if (s == "IFLE_rA_rB")      return Opcode::IFLE_rA_rB;
	if (s == "IFLE_rA_iBC")     return Opcode::IFLE_rA_iBC;
	if (s == "IFLE_iAB_rC")     return Opcode::IFLE_iAB_rC;
	if (s == "IFEQ_rA_rB")      return Opcode::IFEQ_rA_rB;
	if (s == "IFEQ_rA_iBC")     return Opcode::IFEQ_rA_iBC;
	if (s == "IFNE_rA_rB")      return Opcode::IFNE_rA_rB;
	if (s == "IFNE_rA_iBC")     return Opcode::IFNE_rA_iBC;
	if (s == "NEXT_rA_rB")      return Opcode::NEXT_rA_rB;
	if (s == "ARGBLK_iABC")     return Opcode::ARGBLK_iABC;
	if (s == "ARG_rA")          return Opcode::ARG_rA;
	if (s == "ARG_iABC")        return Opcode::ARG_iABC;
	if (s == "CALLF_iA_iBC")    return Opcode::CALLF_iA_iBC;
	if (s == "CALLFN_iA_kBC")   return Opcode::CALLFN_iA_kBC;
	if (s == "CALL_rA_rB_rC")   return Opcode::CALL_rA_rB_rC;
	if (s == "RETURN")          return Opcode::RETURN;
	if (s == "NEW_rA_rB")       return Opcode::NEW_rA_rB;
	if (s == "ISA_rA_rB_rC")    return Opcode::ISA_rA_rB_rC;
	if (s == "METHFIND_rA_rB_rC") return Opcode::METHFIND_rA_rB_rC;
	if (s == "SETSELF_rA")      return Opcode::SETSELF_rA;
	if (s == "CALLIFREF_rA")    return Opcode::CALLIFREF_rA;
	return Opcode::NOOP;
}


} // end of namespace MiniScript
