// CodeEmitter.cs - Base class and implementations for emitting bytecode or assembly
// Provides an abstraction layer for code generation output.

using System;
using System.Collections.Generic;
using static MiniScript.ValueHelpers;
// H: #include "Bytecode.g.h"
// H: #include "value.h"
// H: #include "FuncDef.g.h"

namespace MiniScript {

// Abstract base class for emitting code (bytecode or assembly text)
public abstract class CodeEmitterBase {
	// Emit instructions with varying operand patterns
	// Method names match BytecodeUtil.INS_* patterns
	public abstract void Emit(Opcode op, String comment);              // INS: opcode only
	public abstract void EmitA(Opcode op, Int32 a, String comment);    // INS_A: 8-bit A field
	public abstract void EmitAB(Opcode op, Int32 a, Int32 bc, String comment);   // INS_AB: 8-bit A + 16-bit BC
	public abstract void EmitBC(Opcode op, Int32 ab, Int32 c, String comment);   // INS_BC: 16-bit AB + 8-bit C
	public abstract void EmitABC(Opcode op, Int32 a, Int32 b, Int32 c, String comment);  // INS_ABC: 8-bit A + 8-bit B + 8-bit C

	// Add a constant to the constant pool, return its index
	public abstract Int32 AddConstant(Value value);

	// Label management for jumps
	public abstract Int32 CreateLabel();
	public abstract void PlaceLabel(Int32 labelId);
	public abstract void EmitJump(Opcode op, Int32 labelId, String comment);
	public abstract void EmitBranch(Opcode op, Int32 reg, Int32 labelId, String comment);

	// Track register usage
	public abstract void ReserveRegister(Int32 registerNumber);

	// Finalize and return the compiled function
	public abstract FuncDef Finalize(String name);
}

// Tracks a pending label reference that needs patching
public struct LabelRef {
	public Int32 CodeIndex;    // index in _code where the instruction is
	public Int32 LabelId;      // label being referenced
	public Opcode Op;          // opcode (for re-encoding)
	public Int32 A;            // A operand (for re-encoding)
	public Boolean IsABC;      // true for 24-bit offset (JUMP), false for 16-bit (BRFALSE/BRTRUE)
}


// Emits directly to bytecode (production use)
public class BytecodeEmitter : CodeEmitterBase {
	private List<UInt32> _code;
	private List<Value> _constants;
	private UInt16 _maxRegs;
	private Dictionary<Int32, Int32> _labelAddresses;  // labelId -> code address
	private List<LabelRef> _labelRefs;                 // pending label references
	private Int32 _nextLabelId;

	public BytecodeEmitter() {
		_code = new List<UInt32>();
		_constants = new List<Value>();
		_maxRegs = 0;
		_labelAddresses = new Dictionary<Int32, Int32>();
		_labelRefs = new List<LabelRef>();
		_nextLabelId = 0;
	}

	public override void Emit(Opcode op, String comment) {
		BytecodeUtil.CheckEmitPattern(op, EmitPattern.None);
		_code.Add(BytecodeUtil.INS(op));
	}

	public override void EmitA(Opcode op, Int32 a, String comment) {
		BytecodeUtil.CheckEmitPattern(op, EmitPattern.A);
		_code.Add(BytecodeUtil.INS_A(op, (Byte)a));
	}

	public override void EmitAB(Opcode op, Int32 a, Int32 bc, String comment) {
		BytecodeUtil.CheckEmitPattern(op, EmitPattern.AB);
		_code.Add(BytecodeUtil.INS_AB(op, (Byte)a, (Int16)bc));
	}

	public override void EmitBC(Opcode op, Int32 ab, Int32 c, String comment) {
		BytecodeUtil.CheckEmitPattern(op, EmitPattern.BC);
		_code.Add(BytecodeUtil.INS_BC(op, (Int16)ab, (Byte)c));
	}

	public override void EmitABC(Opcode op, Int32 a, Int32 b, Int32 c, String comment) {
		BytecodeUtil.CheckEmitPattern(op, EmitPattern.ABC);
		_code.Add(BytecodeUtil.INS_ABC(op, (Byte)a, (Byte)b, (Byte)c));
	}

	public override Int32 AddConstant(Value value) {
		// Check if constant already exists (deduplication)
		for (Int32 i = 0; i < _constants.Count; i++) {
			if (value_equal(_constants[i], value)) return i;
		}
		_constants.Add(value);
		return _constants.Count - 1;
	}

	public override Int32 CreateLabel() {
		Int32 labelId = _nextLabelId;
		_nextLabelId = _nextLabelId + 1;
		return labelId;
	}

	public override void PlaceLabel(Int32 labelId) {
		_labelAddresses[labelId] = _code.Count;
	}

	public override void EmitJump(Opcode op, Int32 labelId, String comment) {
		// Emit placeholder instruction, record for later patching
		LabelRef labelRef;
		labelRef.CodeIndex = _code.Count;
		labelRef.LabelId = labelId;
		labelRef.Op = op;
		labelRef.A = 0;
		labelRef.IsABC = true;  // 24-bit offset for JUMP_iABC
		_labelRefs.Add(labelRef);
		_code.Add(BytecodeUtil.INS(op));  // placeholder
	}

	public override void EmitBranch(Opcode op, Int32 reg, Int32 labelId, String comment) {
		// Emit placeholder instruction for conditional branch, record for later patching
		LabelRef labelRef;
		labelRef.CodeIndex = _code.Count;
		labelRef.LabelId = labelId;
		labelRef.Op = op;
		labelRef.A = reg;
		labelRef.IsABC = false;  // 16-bit offset for BRFALSE_rA_iBC, BRTRUE_rA_iBC
		_labelRefs.Add(labelRef);
		_code.Add(BytecodeUtil.INS(op));  // placeholder
	}

	public override void ReserveRegister(Int32 registerNumber) {
		UInt16 impliedCount = (UInt16)(registerNumber + 1);
		if (_maxRegs < impliedCount) _maxRegs = impliedCount;
	}

	public override FuncDef Finalize(String name) {
		// Patch all label references
		for (Int32 i = 0; i < _labelRefs.Count; i++) {
			LabelRef labelRef = _labelRefs[i];
			if (!_labelAddresses.ContainsKey(labelRef.LabelId)) {
				// Error: undefined label
				continue;
			}
			Int32 targetAddr = _labelAddresses[labelRef.LabelId];
			Int32 currentAddr = labelRef.CodeIndex;
			Int32 offset = targetAddr - currentAddr - 1;  // relative offset

			// Re-encode with the correct offset based on instruction type
			if (labelRef.IsABC) {
				// 24-bit offset for JUMP_iABC
				// Encode offset in lower 24 bits (A, B, C fields combined)
				Byte a = (Byte)((offset >> 16) & 0xFF);
				Byte b = (Byte)((offset >> 8) & 0xFF);
				Byte c = (Byte)(offset & 0xFF);
				_code[labelRef.CodeIndex] = BytecodeUtil.INS_ABC(labelRef.Op, a, b, c);
			} else {
				// 8-bit register + 16-bit offset for BRFALSE_rA_iBC, BRTRUE_rA_iBC
				_code[labelRef.CodeIndex] = BytecodeUtil.INS_AB(labelRef.Op, (Byte)labelRef.A, (Int16)offset);
			}
		}

		FuncDef func = new FuncDef();
		func.Name = name;
		func.Code = _code;
		func.Constants = _constants;
		func.MaxRegs = _maxRegs;
		return func;
	}
}

// Emits assembly text (for debugging and testing)
public class AssemblyEmitter : CodeEmitterBase {
	private List<String> _lines;
	private List<Value> _constants;
	private UInt16 _maxRegs;
	private Dictionary<Int32, String> _labelNames;
	private Int32 _nextLabelId;

	public AssemblyEmitter() {
		_lines = new List<String>();
		_constants = new List<Value>();
		_maxRegs = 0;
		_labelNames = new Dictionary<Int32, String>();
		_nextLabelId = 0;
	}

	public override void Emit(Opcode op, String comment) {
		BytecodeUtil.CheckEmitPattern(op, EmitPattern.None);
		String line = $"  {BytecodeUtil.ToMnemonic(op)}";
		if (comment != null) line += $"  ; {comment}";
		_lines.Add(line);
	}

	public override void EmitA(Opcode op, Int32 a, String comment) {
		BytecodeUtil.CheckEmitPattern(op, EmitPattern.A);
		String line = $"  {BytecodeUtil.ToMnemonic(op)} r{a}";
		if (comment != null) line += $"  ; {comment}";
		_lines.Add(line);
	}

	public override void EmitAB(Opcode op, Int32 a, Int32 bc, String comment) {
		BytecodeUtil.CheckEmitPattern(op, EmitPattern.AB);
		String mnemonic = BytecodeUtil.ToMnemonic(op);
		String line;
		if (mnemonic.Contains("_kBC")) {
			line = $"  {mnemonic} r{a}, k{bc}";
		} else if (mnemonic.Contains("_rB")) {
			line = $"  {mnemonic} r{a}, r{bc}";
		} else {
			line = $"  {mnemonic} r{a}, {bc}";
		}
		if (comment != null) line += $"  ; {comment}";
		_lines.Add(line);
	}

	public override void EmitBC(Opcode op, Int32 ab, Int32 c, String comment) {
		BytecodeUtil.CheckEmitPattern(op, EmitPattern.BC);
		String mnemonic = BytecodeUtil.ToMnemonic(op);
		String line;
		if (mnemonic.Contains("_rC")) {
			line = $"  {mnemonic} {ab}, r{c}";
		} else {
			line = $"  {mnemonic} {ab}, {c}";
		}
		if (comment != null) line += $"  ; {comment}";
		_lines.Add(line);
	}

	public override void EmitABC(Opcode op, Int32 a, Int32 b, Int32 c, String comment) {
		BytecodeUtil.CheckEmitPattern(op, EmitPattern.ABC);
		String mnemonic = BytecodeUtil.ToMnemonic(op);
		String line = $"  {mnemonic} r{a}, r{b}, r{c}";
		if (comment != null) line += $"  ; {comment}";
		_lines.Add(line);
	}

	public override Int32 AddConstant(Value value) {
		// Check if constant already exists (deduplication)
		for (Int32 i = 0; i < _constants.Count; i++) {
			if (value_equal(_constants[i], value)) return i;
		}
		_constants.Add(value);
		return _constants.Count - 1;
	}

	public override Int32 CreateLabel() {
		Int32 labelId = _nextLabelId;
		_nextLabelId = _nextLabelId + 1;
		_labelNames[labelId] = $"L{labelId}";
		return labelId;
	}

	public override void PlaceLabel(Int32 labelId) {
		_lines.Add($"{_labelNames[labelId]}:");
	}

	public override void EmitJump(Opcode op, Int32 labelId, String comment) {
		String line = $"  {BytecodeUtil.ToMnemonic(op)} {_labelNames[labelId]}";
		if (comment != null) line += $"  ; {comment}";
		_lines.Add(line);
	}

	public override void EmitBranch(Opcode op, Int32 reg, Int32 labelId, String comment) {
		String line = $"  {BytecodeUtil.ToMnemonic(op)} r{reg}, {_labelNames[labelId]}";
		if (comment != null) line += $"  ; {comment}";
		_lines.Add(line);
	}

	public override void ReserveRegister(Int32 registerNumber) {
		UInt16 impliedCount = (UInt16)(registerNumber + 1);
		if (_maxRegs < impliedCount) _maxRegs = impliedCount;
	}

	public override FuncDef Finalize(String name) {
		// For assembly emitter, we don't actually produce a FuncDef
		// This is primarily for debugging output
		FuncDef func = new FuncDef();
		func.Name = name;
		func.Constants = _constants;
		func.MaxRegs = _maxRegs;
		return func;
	}

	// Get the generated assembly text
	public List<String> GetLines() {
		return _lines;
	}

	// Get the assembly as a single string
	public String GetAssembly() {
		String result = "";
		for (Int32 i = 0; i < _lines.Count; i++) {
			result = result + _lines[i] + "\n";
		}
		return result;
	}
}

}
