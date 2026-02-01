// CodeEmitter.cs - Interface and implementations for emitting bytecode or assembly
// Provides an abstraction layer for code generation output.

using System;
using System.Collections.Generic;
using static MiniScript.ValueHelpers;
// H: #include "Bytecode.g.h"
// H: #include "value.h"
// H: #include "FuncDef.g.h"

namespace MiniScript {

// Interface for emitting code (bytecode or assembly text)
public interface ICodeEmitter {
	// Emit instructions with varying operand patterns
	// Method names match BytecodeUtil.INS_* patterns
	void Emit(Opcode op, String comment);              // INS: opcode only
	void EmitA(Opcode op, Int32 a, String comment);    // INS_A: 8-bit A field
	void EmitAB(Opcode op, Int32 a, Int32 bc, String comment);   // INS_AB: 8-bit A + 16-bit BC
	void EmitBC(Opcode op, Int32 ab, Int32 c, String comment);   // INS_BC: 16-bit AB + 8-bit C
	void EmitABC(Opcode op, Int32 a, Int32 b, Int32 c, String comment);  // INS_ABC: 8-bit A + 8-bit B + 8-bit C

	// Add a constant to the constant pool, return its index
	Int32 AddConstant(Value value);

	// Label management for jumps
	Int32 CreateLabel();
	void PlaceLabel(Int32 labelId);
	void EmitJump(Opcode op, Int32 labelId, String comment);

	// Track register usage
	void ReserveRegister(Int32 registerNumber);

	// Finalize and return the compiled function
	FuncDef Finalize(String name);
}

// Tracks a pending label reference that needs patching
public struct LabelRef {
	public Int32 CodeIndex;    // index in _code where the instruction is
	public Int32 LabelId;      // label being referenced
	public Opcode Op;          // opcode (for re-encoding)
	public Int32 A;            // A operand (for re-encoding)
}


// Emits directly to bytecode (production use)
public class BytecodeEmitter : ICodeEmitter {
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

	public void Emit(Opcode op, String comment) {
		BytecodeUtil.CheckEmitPattern(op, EmitPattern.None);
		_code.Add(BytecodeUtil.INS(op));
	}

	public void EmitA(Opcode op, Int32 a, String comment) {
		BytecodeUtil.CheckEmitPattern(op, EmitPattern.A);
		_code.Add(BytecodeUtil.INS_A(op, (Byte)a));
	}

	public void EmitAB(Opcode op, Int32 a, Int32 bc, String comment) {
		BytecodeUtil.CheckEmitPattern(op, EmitPattern.AB);
		_code.Add(BytecodeUtil.INS_AB(op, (Byte)a, (Int16)bc));
	}

	public void EmitBC(Opcode op, Int32 ab, Int32 c, String comment) {
		BytecodeUtil.CheckEmitPattern(op, EmitPattern.BC);
		_code.Add(BytecodeUtil.INS_BC(op, (Int16)ab, (Byte)c));
	}

	public void EmitABC(Opcode op, Int32 a, Int32 b, Int32 c, String comment) {
		BytecodeUtil.CheckEmitPattern(op, EmitPattern.ABC);
		_code.Add(BytecodeUtil.INS_ABC(op, (Byte)a, (Byte)b, (Byte)c));
	}

	public Int32 AddConstant(Value value) {
		// Check if constant already exists (deduplication)
		for (Int32 i = 0; i < _constants.Count; i++) {
			if (value_equal(_constants[i], value)) return i;
		}
		_constants.Add(value);
		return _constants.Count - 1;
	}

	public Int32 CreateLabel() {
		Int32 labelId = _nextLabelId;
		_nextLabelId = _nextLabelId + 1;
		return labelId;
	}

	public void PlaceLabel(Int32 labelId) {
		_labelAddresses[labelId] = _code.Count;
	}

	public void EmitJump(Opcode op, Int32 labelId, String comment) {
		// Emit placeholder instruction, record for later patching
		LabelRef labelRef;
		labelRef.CodeIndex = _code.Count;
		labelRef.LabelId = labelId;
		labelRef.Op = op;
		labelRef.A = 0;
		_labelRefs.Add(labelRef);
		_code.Add(BytecodeUtil.INS(op));  // placeholder
	}

	public void ReserveRegister(Int32 registerNumber) {
		UInt16 impliedCount = (UInt16)(registerNumber + 1);
		if (_maxRegs < impliedCount) _maxRegs = impliedCount;
	}

	public FuncDef Finalize(String name) {
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

			// Re-encode with the correct offset
			_code[labelRef.CodeIndex] = BytecodeUtil.INS_AB(labelRef.Op, (Byte)labelRef.A, (Int16)offset);
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
public class AssemblyEmitter : ICodeEmitter {
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

	public void Emit(Opcode op, String comment) {
		BytecodeUtil.CheckEmitPattern(op, EmitPattern.None);
		String line = $"  {BytecodeUtil.ToMnemonic(op)}";
		if (comment != null) line += $"  ; {comment}";
		_lines.Add(line);
	}

	public void EmitA(Opcode op, Int32 a, String comment) {
		BytecodeUtil.CheckEmitPattern(op, EmitPattern.A);
		String line = $"  {BytecodeUtil.ToMnemonic(op)} r{a}";
		if (comment != null) line += $"  ; {comment}";
		_lines.Add(line);
	}

	public void EmitAB(Opcode op, Int32 a, Int32 bc, String comment) {
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

	public void EmitBC(Opcode op, Int32 ab, Int32 c, String comment) {
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

	public void EmitABC(Opcode op, Int32 a, Int32 b, Int32 c, String comment) {
		BytecodeUtil.CheckEmitPattern(op, EmitPattern.ABC);
		String mnemonic = BytecodeUtil.ToMnemonic(op);
		String line = $"  {mnemonic} r{a}, r{b}, r{c}";
		if (comment != null) line += $"  ; {comment}";
		_lines.Add(line);
	}

	public Int32 AddConstant(Value value) {
		// Check if constant already exists (deduplication)
		for (Int32 i = 0; i < _constants.Count; i++) {
			if (Value.Equal(_constants[i], value)) return i;
		}
		_constants.Add(value);
		return _constants.Count - 1;
	}

	public Int32 CreateLabel() {
		Int32 labelId = _nextLabelId;
		_nextLabelId = _nextLabelId + 1;
		_labelNames[labelId] = $"L{labelId}";
		return labelId;
	}

	public void PlaceLabel(Int32 labelId) {
		_lines.Add($"{_labelNames[labelId]}:");
	}

	public void EmitJump(Opcode op, Int32 labelId, String comment) {
		String line = $"  {BytecodeUtil.ToMnemonic(op)} {_labelNames[labelId]}";
		if (comment != null) line += $"  ; {comment}";
		_lines.Add(line);
	}

	public void ReserveRegister(Int32 registerNumber) {
		UInt16 impliedCount = (UInt16)(registerNumber + 1);
		if (_maxRegs < impliedCount) _maxRegs = impliedCount;
	}

	public FuncDef Finalize(String name) {
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
