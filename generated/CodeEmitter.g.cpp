// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: CodeEmitter.cs

#include "CodeEmitter.g.h"

namespace MiniScript {


Int32 CodeEmitterBaseStorage::AddConstant(Value value) {
	List<Value> constants = PendingFunc.Constants();
	for (Int32 i = 0; i < constants.Count(); i++) {
		if (value_equal(constants[i], value)) return i;
	}
	constants.Add(value);
	return constants.Count() - 1;
}
void CodeEmitterBaseStorage::ReserveRegister(Int32 registerNumber) {
	PendingFunc.ReserveRegister(registerNumber);
}
FuncDef CodeEmitterBaseStorage::Finalize(String name) {
	FuncDef result = PendingFunc;
	result.set_Name(name);
	return result;
}




BytecodeEmitterStorage::BytecodeEmitterStorage() {
	PendingFunc =  FuncDef::New();
	_labelAddresses =  Dictionary<Int32, Int32>::New();
	_labelRefs =  List<LabelRef>::New();
	_nextLabelId = 0;
}
void BytecodeEmitterStorage::Emit(Opcode op, String comment) {
	BytecodeUtil::CheckEmitPattern(op, EmitPattern::None);
	PendingFunc.Code().Add(BytecodeUtil::INS(op));
}
void BytecodeEmitterStorage::EmitA(Opcode op, Int32 a, String comment) {
	BytecodeUtil::CheckEmitPattern(op, EmitPattern::A);
	PendingFunc.Code().Add(BytecodeUtil::INS_A(op, (Byte)a));
}
void BytecodeEmitterStorage::EmitAB(Opcode op, Int32 a, Int32 bc, String comment) {
	BytecodeUtil::CheckEmitPattern(op, EmitPattern::AB);
	PendingFunc.Code().Add(BytecodeUtil::INS_AB(op, (Byte)a, (Int16)bc));
}
void BytecodeEmitterStorage::EmitBC(Opcode op, Int32 ab, Int32 c, String comment) {
	BytecodeUtil::CheckEmitPattern(op, EmitPattern::BC);
	PendingFunc.Code().Add(BytecodeUtil::INS_BC(op, (Int16)ab, (Byte)c));
}
void BytecodeEmitterStorage::EmitABC(Opcode op, Int32 a, Int32 b, Int32 c, String comment) {
	BytecodeUtil::CheckEmitPattern(op, EmitPattern::ABC);
	PendingFunc.Code().Add(BytecodeUtil::INS_ABC(op, (Byte)a, (Byte)b, (Byte)c));
}
Int32 BytecodeEmitterStorage::CreateLabel() {
	Int32 labelId = _nextLabelId;
	_nextLabelId = _nextLabelId + 1;
	return labelId;
}
void BytecodeEmitterStorage::PlaceLabel(Int32 labelId) {
	_labelAddresses[labelId] = PendingFunc.Code().Count();
}
void BytecodeEmitterStorage::EmitJump(Opcode op, Int32 labelId, String comment) {
	// Emit placeholder instruction, record for later patching
	LabelRef labelRef;
	labelRef.CodeIndex = PendingFunc.Code().Count();
	labelRef.LabelId = labelId;
	labelRef.Op = op;
	labelRef.A = 0;
	labelRef.IsABC = Boolean(true);  // 24-bit offset for JUMP_iABC
	_labelRefs.Add(labelRef);
	PendingFunc.Code().Add(BytecodeUtil::INS(op));  // placeholder
}
void BytecodeEmitterStorage::EmitBranch(Opcode op, Int32 reg, Int32 labelId, String comment) {
	// Emit placeholder instruction for conditional branch, record for later patching
	LabelRef labelRef;
	labelRef.CodeIndex = PendingFunc.Code().Count();
	labelRef.LabelId = labelId;
	labelRef.Op = op;
	labelRef.A = reg;
	labelRef.IsABC = Boolean(false);  // 16-bit offset for BRFALSE_rA_iBC, BRTRUE_rA_iBC
	_labelRefs.Add(labelRef);
	PendingFunc.Code().Add(BytecodeUtil::INS(op));  // placeholder
}
FuncDef BytecodeEmitterStorage::Finalize(String name) {
	// Patch all label references
	List<UInt32> code = PendingFunc.Code();
	for (Int32 i = 0; i < _labelRefs.Count(); i++) {
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
			code[labelRef.CodeIndex] = BytecodeUtil::INS_ABC(labelRef.Op, a, b, c);
		} else {
			// 8-bit register + 16-bit offset for BRFALSE_rA_iBC, BRTRUE_rA_iBC
			code[labelRef.CodeIndex] = BytecodeUtil::INS_AB(labelRef.Op, (Byte)labelRef.A, (Int16)offset);
		}
	}

	return this->CodeEmitterBaseStorage::Finalize(name); 
}


AssemblyEmitterStorage::AssemblyEmitterStorage() {
	PendingFunc =  FuncDef::New();
	_lines =  List<String>::New();
	_labelNames =  Dictionary<Int32, String>::New();
	_nextLabelId = 0;
}
void AssemblyEmitterStorage::Emit(Opcode op, String comment) {
	BytecodeUtil::CheckEmitPattern(op, EmitPattern::None);
	String line = Interp("  {}", BytecodeUtil::ToMnemonic(op));
	if (!IsNull(comment)) line += Interp("  ; {}", comment);
	_lines.Add(line);
}
void AssemblyEmitterStorage::EmitA(Opcode op, Int32 a, String comment) {
	BytecodeUtil::CheckEmitPattern(op, EmitPattern::A);
	String line = Interp("  {} r{}", BytecodeUtil::ToMnemonic(op), a);
	if (!IsNull(comment)) line += Interp("  ; {}", comment);
	_lines.Add(line);
}
void AssemblyEmitterStorage::EmitAB(Opcode op, Int32 a, Int32 bc, String comment) {
	BytecodeUtil::CheckEmitPattern(op, EmitPattern::AB);
	String mnemonic = BytecodeUtil::ToMnemonic(op);
	String line;
	if (mnemonic.Contains("_kBC")) {
		line = Interp("  {} r{}, k{}", mnemonic, a, bc);
	} else if (mnemonic.Contains("_rB")) {
		line = Interp("  {} r{}, r{}", mnemonic, a, bc);
	} else {
		line = Interp("  {} r{}, {}", mnemonic, a, bc);
	}
	if (!IsNull(comment)) line += Interp("  ; {}", comment);
	_lines.Add(line);
}
void AssemblyEmitterStorage::EmitBC(Opcode op, Int32 ab, Int32 c, String comment) {
	BytecodeUtil::CheckEmitPattern(op, EmitPattern::BC);
	String mnemonic = BytecodeUtil::ToMnemonic(op);
	String line;
	if (mnemonic.Contains("_rC")) {
		line = Interp("  {} {}, r{}", mnemonic, ab, c);
	} else {
		line = Interp("  {} {}, {}", mnemonic, ab, c);
	}
	if (!IsNull(comment)) line += Interp("  ; {}", comment);
	_lines.Add(line);
}
void AssemblyEmitterStorage::EmitABC(Opcode op, Int32 a, Int32 b, Int32 c, String comment) {
	BytecodeUtil::CheckEmitPattern(op, EmitPattern::ABC);
	String mnemonic = BytecodeUtil::ToMnemonic(op);
	String line = Interp("  {} r{}, r{}, r{}", mnemonic, a, b, c);
	if (!IsNull(comment)) line += Interp("  ; {}", comment);
	_lines.Add(line);
}
Int32 AssemblyEmitterStorage::CreateLabel() {
	Int32 labelId = _nextLabelId;
	_nextLabelId = _nextLabelId + 1;
	_labelNames[labelId] = Interp("L{}", labelId);
	return labelId;
}
void AssemblyEmitterStorage::PlaceLabel(Int32 labelId) {
	_lines.Add(Interp("{}:", _labelNames[labelId]));
}
void AssemblyEmitterStorage::EmitJump(Opcode op, Int32 labelId, String comment) {
	String line = Interp("  {} {}", BytecodeUtil::ToMnemonic(op), _labelNames[labelId]);
	if (!IsNull(comment)) line += Interp("  ; {}", comment);
	_lines.Add(line);
}
void AssemblyEmitterStorage::EmitBranch(Opcode op, Int32 reg, Int32 labelId, String comment) {
	String line = Interp("  {} r{}, {}", BytecodeUtil::ToMnemonic(op), reg, _labelNames[labelId]);
	if (!IsNull(comment)) line += Interp("  ; {}", comment);
	_lines.Add(line);
}
List<String> AssemblyEmitterStorage::GetLines() {
	return _lines;
}
String AssemblyEmitterStorage::GetAssembly() {
	String result = "";
	for (Int32 i = 0; i < _lines.Count(); i++) {
		result = result + _lines[i] + "\n";
	}
	return result;
}


} // end of namespace MiniScript
