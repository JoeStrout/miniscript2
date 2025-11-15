// This header defines macros used to encapsulate two different approaches
// to opcode dispatch in the C++ version of the VM:
//
//	1. Computed-goto (using a list of labels, one per opcode).
//	2. An ordinary `switch` statement (just like the C# version).

// This file is part of Layer 0 (foundation utilities)
#define CORE_LAYER_0

// Feature detection (override with -DVM_USE_COMPUTED_GOTO=0/1)
#ifndef VM_USE_COMPUTED_GOTO
/*
 * Auto-detect: enable computed-goto for any GCC-like compiler (GCC or Clang)
 * when not in strict ANSI mode. Clang defines __GNUC__ for compatibility, so
 * this covers Apple Clang as well. If you compile with -std=c11 (strict), this
 * will resolve to 0 unless you pass -DVM_USE_COMPUTED_GOTO=1 or use -std=gnu11.
 */
#  if (defined(__GNUC__) || defined(__clang__)) && !defined(__STRICT_ANSI__)
#    define VM_USE_COMPUTED_GOTO 1
#  else
#    define VM_USE_COMPUTED_GOTO 0
#  endif
#endif

// X-macro defining all opcodes - must match the C# Opcode enum exactly
#define VM_OPCODES(X) \
	X(NOOP) \
	X(LOAD_rA_rB) \
	X(LOAD_rA_iBC) \
	X(LOAD_rA_kBC) \
	X(LOADV_rA_rB_kC) \
	X(LOADC_rA_rB_kC) \
	X(FUNCREF_iA_iBC) \
	X(ASSIGN_rA_rB_kC) \
	X(NAME_rA_kBC) \
	X(ADD_rA_rB_rC) \
	X(SUB_rA_rB_rC) \
	X(MULT_rA_rB_rC) \
	X(DIV_rA_rB_rC) \
	X(MOD_rA_rB_rC) \
	X(LIST_rA_iBC) \
	X(MAP_rA_iBC) \
	X(PUSH_rA_rB) \
	X(INDEX_rA_rB_rC) \
	X(IDXSET_rA_rB_rC) \
	X(LOCALS_rA) \
	X(OUTER_rA) \
	X(GLOBALS_rA) \
	X(JUMP_iABC) \
	X(LT_rA_rB_rC) \
	X(LT_rA_rB_iC) \
	X(LT_rA_iB_rC) \
	X(LE_rA_rB_rC) \
	X(LE_rA_rB_iC) \
	X(LE_rA_iB_rC) \
	X(EQ_rA_rB_rC) \
	X(EQ_rA_rB_iC) \
	X(NE_rA_rB_rC) \
	X(NE_rA_rB_iC) \
	X(BRTRUE_rA_iBC) \
	X(BRFALSE_rA_iBC) \
	X(BRLT_rA_rB_iC) \
	X(BRLT_rA_iB_iC) \
	X(BRLT_iA_rB_iC) \
	X(BRLE_rA_rB_iC) \
	X(BRLE_rA_iB_iC) \
	X(BRLE_iA_rB_iC) \
	X(BREQ_rA_rB_iC) \
	X(BREQ_rA_iB_iC) \
	X(BRNE_rA_rB_iC) \
	X(BRNE_rA_iB_iC) \
	X(IFLT_rA_rB) \
	X(IFLT_rA_iBC) \
	X(IFLT_iAB_rC) \
	X(IFLE_rA_rB) \
	X(IFLE_rA_iBC) \
	X(IFLE_iAB_rC) \
	X(IFEQ_rA_rB) \
	X(IFEQ_rA_iBC) \
	X(IFNE_rA_rB) \
	X(IFNE_rA_iBC) \
	X(ARGBLK_iABC) \
	X(ARG_rA) \
	X(ARG_iABC) \
	X(CALLF_iA_iBC) \
	X(CALLFN_iA_kBC) \
	X(CALL_rA_rB_rC) \
	X(RETURN)


#if VM_USE_COMPUTED_GOTO
	// Computed-goto: build a label table inside the function.
	// Important: include commas between entries!
	#define VM_LABEL_ADDR(OP) &&L_##OP
	#define VM_LABEL_LIST(OP) VM_LABEL_ADDR(OP),

	#define VM_DISPATCH_TOP() vm_dispatch_top:
	#define VM_DISPATCH_BEGIN() \
		if (!IsRunning) goto vm_dispatch_bottom; \
		goto *vm_labels[(int)opcode];

	#define VM_CASE(OP)     L_##OP:
	#define VM_NEXT()       goto vm_dispatch_top
	#define VM_DISPATCH_END()
	#define VM_DISPATCH_BOTTOM() vm_dispatch_bottom:
#else
	#define VM_DISPATCH_TOP() /* unused */
	#define VM_DISPATCH_BEGIN() \
		switch (opcode) {
	#define VM_CASE(OP)        case Opcode::OP:
	#define VM_NEXT()          break;
	#define VM_DISPATCH_END()  default: IOHelper::Print("unknown opcode"); return make_null(); }
	#define VM_DISPATCH_BOTTOM()
#endif

// Layer 0 foundation utility - safe to include from any layer
// No layer violation checks needed (Layer 0 has no dependencies)

