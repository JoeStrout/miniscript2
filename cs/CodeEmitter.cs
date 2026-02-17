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
	// The function definition being built
	public FuncDef PendingFunc;

	// Emit instructions with varying operand patterns
	// Method names match BytecodeUtil.INS_* patterns
	public abstract void Emit(Opcode op, String comment);              // INS: opcode only
	public abstract void EmitA(Opcode op, Int32 a, String comment);    // INS_A: 8-bit A field
	public abstract void EmitAB(Opcode op, Int32 a, Int32 bc, String comment);   // INS_AB: 8-bit A + 16-bit BC
	public abstract void EmitBC(Opcode op, Int32 ab, Int32 c, String comment);   // INS_BC: 16-bit AB + 8-bit C
	public abstract void EmitABC(Opcode op, Int32 a, Int32 b, Int32 c, String comment);  // INS_ABC: 8-bit A + 8-bit B + 8-bit C

	// Add a constant to the constant pool, return its index
	public Int32 AddConstant(Value value) {
		List<Value> constants = PendingFunc.Constants;
		for (Int32 i = 0; i < constants.Count; i++) {
			if (value_equal(constants[i], value)) return i;
		}
		constants.Add(value);
		return constants.Count - 1;
	}

	// Label management for jumps
	public abstract Int32 CreateLabel();
	public abstract void PlaceLabel(Int32 labelId);
	public abstract void EmitJump(Opcode op, Int32 labelId, String comment);
	public abstract void EmitBranch(Opcode op, Int32 reg, Int32 labelId, String comment);

	// Track register usage
	public void ReserveRegister(Int32 registerNumber) {
		PendingFunc.ReserveRegister(registerNumber);
	}

	// Finalize: set name, do any patching, return the PendingFunc
	public virtual FuncDef Finalize(String name) {
		FuncDef result = PendingFunc;
		result.Name = name;
		return result;
	}
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
	private Dictionary<Int32, Int32> _labelAddresses;  // labelId -> code address
	private List<LabelRef> _labelRefs;                 // pending label references
	private Int32 _nextLabelId;

	public BytecodeEmitter() {
		PendingFunc = new FuncDef();
		_labelAddresses = new Dictionary<Int32, Int32>();
		_labelRefs = new List<LabelRef>();
		_nextLabelId = 0;
	}

	public override void Emit(Opcode op, String comment) {
		BytecodeUtil.CheckEmitPattern(op, EmitPattern.None);
		PendingFunc.Code.Add(BytecodeUtil.INS(op));
	}

	public override void EmitA(Opcode op, Int32 a, String comment) {
		BytecodeUtil.CheckEmitPattern(op, EmitPattern.A);
		PendingFunc.Code.Add(BytecodeUtil.INS_A(op, (Byte)a));
	}

	public override void EmitAB(Opcode op, Int32 a, Int32 bc, String comment) {
		BytecodeUtil.CheckEmitPattern(op, EmitPattern.AB);
		PendingFunc.Code.Add(BytecodeUtil.INS_AB(op, (Byte)a, (Int16)bc));
	}

	public override void EmitBC(Opcode op, Int32 ab, Int32 c, String comment) {
		BytecodeUtil.CheckEmitPattern(op, EmitPattern.BC);
		PendingFunc.Code.Add(BytecodeUtil.INS_BC(op, (Int16)ab, (Byte)c));
	}

	public override void EmitABC(Opcode op, Int32 a, Int32 b, Int32 c, String comment) {
		BytecodeUtil.CheckEmitPattern(op, EmitPattern.ABC);
		PendingFunc.Code.Add(BytecodeUtil.INS_ABC(op, (Byte)a, (Byte)b, (Byte)c));
	}

	public override Int32 CreateLabel() {
		Int32 labelId = _nextLabelId;
		_nextLabelId = _nextLabelId + 1;
		return labelId;
	}

	public override void PlaceLabel(Int32 labelId) {
		_labelAddresses[labelId] = PendingFunc.Code.Count;
	}

	public override void EmitJump(Opcode op, Int32 labelId, String comment) {
		// Emit placeholder instruction, record for later patching
		LabelRef labelRef;
		labelRef.CodeIndex = PendingFunc.Code.Count;
		labelRef.LabelId = labelId;
		labelRef.Op = op;
		labelRef.A = 0;
		labelRef.IsABC = true;  // 24-bit offset for JUMP_iABC
		_labelRefs.Add(labelRef);
		PendingFunc.Code.Add(BytecodeUtil.INS(op));  // placeholder
	}

	public override void EmitBranch(Opcode op, Int32 reg, Int32 labelId, String comment) {
		// Emit placeholder instruction for conditional branch, record for later patching
		LabelRef labelRef;
		labelRef.CodeIndex = PendingFunc.Code.Count;
		labelRef.LabelId = labelId;
		labelRef.Op = op;
		labelRef.A = reg;
		labelRef.IsABC = false;  // 16-bit offset for BRFALSE_rA_iBC, BRTRUE_rA_iBC
		_labelRefs.Add(labelRef);
		PendingFunc.Code.Add(BytecodeUtil.INS(op));  // placeholder
	}

	public override FuncDef Finalize(String name) {
		// Patch all label references
		List<UInt32> code = PendingFunc.Code;
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
				code[labelRef.CodeIndex] = BytecodeUtil.INS_ABC(labelRef.Op, a, b, c);
			} else {
				// 8-bit register + 16-bit offset for BRFALSE_rA_iBC, BRTRUE_rA_iBC
				code[labelRef.CodeIndex] = BytecodeUtil.INS_AB(labelRef.Op, (Byte)labelRef.A, (Int16)offset);
			}
		}

		return base.Finalize(name);  // CPP: return this->CodeEmitterBaseStorage::Finalize(name); 
	}
}

// Emits assembly text (for debugging and testing)
public class AssemblyEmitter : CodeEmitterBase {
	private List<String> _lines;
	private Dictionary<Int32, String> _labelNames;
	private Int32 _nextLabelId;

	public AssemblyEmitter() {
		PendingFunc = new FuncDef();
		_lines = new List<String>();
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
