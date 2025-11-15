using System;
using System.Collections.Generic;
// CPP: #include "value.h"
// CPP: #include "value_string.h"
// CPP: #include "Bytecode.g.h"
// CPP: #include "FuncDef.g.h"
// CPP: #include "StringUtils.g.h"
// CPP: #include "MemPoolShim.g.h"
// CPP: #include "CS_value_util.h"
// CPP: #include <climits>

using static MiniScript.ValueHelpers;

namespace MiniScript {

	public class Assembler {

		// Multiple functions support
		public List<FuncDef> Functions; // all functions
		public FuncDef Current; // function we are currently building
		private List<String> _labelNames; // label names within current function
		private List<Int32> _labelAddresses; // corresponding instruction addresses within current function
		
		// Error handling state
		public Boolean HasError { get; private set; }
		public String ErrorMessage { get; private set; }
		public Int32 CurrentLineNumber { get; private set; }
		public String CurrentLine { get; private set; }

		public Assembler() {
			Functions = new List<FuncDef>();
			Current = new FuncDef();
			_labelNames = new List<String>();
			_labelAddresses = new List<Int32>();
			
			// Initialize error state
			HasError = false;
			ErrorMessage = "";
			CurrentLineNumber = 0;
			CurrentLine = "";
		}

		// Helper to find a function by name (returns -1 if not found)
		public Int32 FindFunctionIndex(String name) {
			for (Int32 i = 0; i < Functions.Count; i++) {
				if (Functions[i].Name == name) return i;
			}
			return -1;
		}

		// Helper to find a function by name.
		// C#: returns null if not found; C++: returns an empty FuncDef.
		public FuncDef FindFunction(String name) {
			Int32 index = FindFunctionIndex(name);
			if (index >= 0) return Functions[index];			
			return null; // CPP: FuncDef result; return result;
		}

		// Helper to check if a function exists
		public Boolean HasFunction(String name) {
			return FindFunctionIndex(name) >= 0;
		}

		public static List<String> GetTokens(String line) {
			// Clean the string, stripping off comment at '#',
			// and divide into tokens by whitespace and commas.  Example:
			//  "   LOAD r5, r6 # comment"  -->  ["LOAD", "r5", "r6"]
			//  "LOAD r1, \"Hello world\""  -->  ["LOAD", "r1", "\"Hello world\""]
			
			// Parse line character by character, collecting tokens
			List<String> tokens = new List<String>();
			Int32 tokenStart = -1;
			Boolean inQuotes = false;
			
			for (Int32 i = 0; i < line.Length; i++) {
				Char c = line[i];
				
				// Stop at comment (but not inside quotes)
				if (c == '#' && !inQuotes) break;
				
				// Handle quote characters
				if (c == '"') {
					if (tokenStart < 0) {
						// Start of quoted token
						tokenStart = i;
						inQuotes = true;
					} else if (inQuotes) {
						// End of quoted token
						tokens.Add(line.Substring(tokenStart, i - tokenStart + 1));
						tokenStart = -1;
						inQuotes = false;
					}
					continue;
				}
				
				// Check if this is a delimiter (whitespace or comma), but not inside quotes
				if ((c == ' ' || c == '\t' || c == ',') && !inQuotes) {
					// End current token if we have one
					if (tokenStart >= 0) {
						tokens.Add(line.Substring(tokenStart, i - tokenStart));
						tokenStart = -1;
					}
				} else {
					// Start new token if not already started and not already in quotes
					if (tokenStart < 0 && !inQuotes) {
						tokenStart = i;
					}
				}
			}
			
			// Add final token if any
			if (tokenStart >= 0) {
				if (inQuotes) {
					// Unclosed quote - include the rest of the line
					tokens.Add(line.Substring(tokenStart, line.Length - tokenStart));
				} else {
					tokens.Add(line.Substring(tokenStart, line.Length - tokenStart));
				}
			}
			return tokens;
		}
		
		private void SetCurrentLine(Int32 lineNumber, String line) {
			CurrentLineNumber = lineNumber;
			CurrentLine = line;
		}
		
		public void ClearError() {
			HasError = false;
			ErrorMessage = "";
			CurrentLineNumber = 0;
			CurrentLine = "";
		}
		
		private void Error(String errMsg) {
			if (HasError) return; // Don't overwrite first error
			
			HasError = true;
			ErrorMessage = errMsg;
			IOHelper.Print(StringUtils.Format("ERROR: {0}", errMsg));
			IOHelper.Print(StringUtils.Format("Line {0}: {1}", CurrentLineNumber, CurrentLine));
		}
		
		// Assemble a single source line, add to our current function,
		// and also return its value (mainly for unit testing).
		public UInt32 AddLine(String line) {
			return AddLine(line, 0);  // Default line number
		}
		
		public UInt32 AddLine(String line, Int32 lineNumber) {
			SetCurrentLine(lineNumber, line);
			if (HasError) return 0;  // Don't process more lines if we already have an error
			// Break into tokens (stripping whitespace, commas, and comments)
			List<String> parts = GetTokens(line);
			
			// Check if first token is a label and remove it
			if (parts.Count > 0 && IsLabel(parts[0])) parts.RemoveAt(0);

			// If there is no instruction on this line, return 0
			if (parts.Count == 0) return 0;

			String mnemonic = parts[0];
			UInt32 instruction = 0;

			// Handle .param directive (not an instruction, but a function parameter definition)
			if (mnemonic == ".param") {
				// .param paramName
				// .param paramName=defaultValue
				if (parts.Count < 2) {
					Error("Syntax error: .param requires a parameter name");
					return 0;
				}

				String paramSpec = parts[1];
				String paramName;
				Value defaultValue = make_null();

				// Check if there's a default value (e.g., "b=1")
				Int32 equalsPos = -1;
				for (Int32 i = 0; i < paramSpec.Length; i++) {
					if (paramSpec[i] == '=') {
						equalsPos = i;
						break;
					}
				}

				if (equalsPos >= 0) {
					// Has default value
					paramName = paramSpec.Substring(0, equalsPos);
					String defaultStr = paramSpec.Substring(equalsPos + 1);
					defaultValue = ParseAsConstant(defaultStr);
				} else {
					// No default value (defaults to null)
					paramName = paramSpec;
				}

				// Add parameter to current function (store name as Value string)
				// ToDo: make simple, consistent conversion functions between String and Value, and use everywhere.
				Current.ParamNames.Add(make_string(paramName));
				Current.ParamDefaults.Add(defaultValue);

				return 0; // Directives don't produce instructions
			}

			if (mnemonic == "NOOP") {
				instruction = BytecodeUtil.INS(Opcode.NOOP);
				
			} else if (mnemonic == "LOAD") {
				if (parts.Count != 3) {
					Error("Syntax error: LOAD requires exactly 2 operands");
					return 0;
				}
				
				String destReg = parts[1];  // should be "r5" etc.
				String source = parts[2];   // "r6", "42", "3.14", "hello", or "k20" 
								
				Byte dest = ParseRegister(destReg);
				Current.ReserveRegister(dest);

				if (source[0] == 'r') {
					// LOAD r2, r5  -->  LOAD_rA_rB
					Byte srcReg = ParseRegister(source);
					instruction = BytecodeUtil.INS_ABC(Opcode.LOAD_rA_rB, dest, srcReg, 0);
				} else if (source[0] == 'k') {
					// LOAD r3, k20  -->  LOAD_rA_kBC (explicit constant reference)
					Int16 constIdx = ParseInt16(source.Substring(1));
					instruction = BytecodeUtil.INS_AB(Opcode.LOAD_rA_kBC, dest, constIdx);
				} else if (NeedsConstant(source)) {
					// String literals, floats, or large integers -> add to constants table
					Value constantValue = ParseAsConstant(source);
					Int32 constIdx = AddConstant(constantValue);
					instruction = BytecodeUtil.INS_AB(Opcode.LOAD_rA_kBC, dest, (Int16)constIdx);
				} else {
					// Small integer that fits in Int16 -> use immediate form
					Int16 immediate = ParseInt16(source);
					instruction = BytecodeUtil.INS_AB(Opcode.LOAD_rA_iBC, dest, immediate);
				}

			} else if (mnemonic == "LOADV") {
				// LOADV r1, r2, "varname"  -->  LOADV_rA_rB_kC
				// Load value from r2 into r1, but verify that r2 has name matching varname
				if (parts.Count != 4) {
					Error("Syntax error: LOADV requires exactly 3 operands");
					return 0;
				}
				Byte dest = ParseRegister(parts[1]);
				Current.ReserveRegister(dest);
				Byte src = ParseRegister(parts[2]);

				Value constantValue = ParseAsConstant(parts[3]);
				if (!is_string(constantValue)) Error("Variable name must be a string");
				Int32 constIdx = AddConstant(constantValue);
				if (constIdx > 255) Error("Constant index out of range for LOADV opcode");
				if (HasError) return 0;
				instruction = BytecodeUtil.INS_ABC(Opcode.LOADV_rA_rB_kC, dest, src, (Byte)constIdx);

			} else if (mnemonic == "LOADC") {
				// LOADC r1, r2, "varname"  -->  LOADC_rA_rB_kC
				// Load value from r2 into r1, but verify name matches varname and call if funcref
				if (parts.Count != 4) {
					Error("Syntax error: LOADC requires exactly 3 operands");
					return 0;
				}
				Byte dest = ParseRegister(parts[1]);
				Current.ReserveRegister(dest);
				Byte src = ParseRegister(parts[2]);

				Value constantValue = ParseAsConstant(parts[3]);
				if (!is_string(constantValue)) Error("Variable name must be a string");
				Int32 constIdx = AddConstant(constantValue);
				if (constIdx > 255) Error("Constant index out of range for LOADC opcode");
				if (HasError) return 0;
				instruction = BytecodeUtil.INS_ABC(Opcode.LOADC_rA_rB_kC, dest, src, (Byte)constIdx);

			} else if (mnemonic == "FUNCREF") {
				// FUNCREF r1, 5  -->  FUNCREF_iA_iBC
				// Store make_funcref(funcIndex) into register A
				if (parts.Count != 3) {
					Error("Syntax error: FUNCREF requires exactly 2 operands");
					return 0;
				}
				Byte dest = ParseRegister(parts[1]);
				Current.ReserveRegister(dest);
				Int16 funcIdx = (Int16)FindFunctionIndex(parts[2]);
				instruction = BytecodeUtil.INS_AB(Opcode.FUNCREF_iA_iBC, dest, funcIdx);

			} else if (mnemonic == "ASSIGN") {
				// ASSIGN r1, r2, k3  -->  ASSIGN_rA_rB_kC
				// Copy value from r2 to r1, and assign variable name from constants[3]
				if (parts.Count != 4) {
					Error("Syntax error: ASSIGN requires exactly 3 operands");
					return 0;
				}
				Byte dest = ParseRegister(parts[1]);
				Current.ReserveRegister(dest);
				Byte src = ParseRegister(parts[2]);

				Value constantValue = ParseAsConstant(parts[3]);
				if (!is_string(constantValue)) Error("Variable name must be a string");
				Int32 constIdx = AddConstant(constantValue);
				if (constIdx > 255) Error("Constant index out of range for ASSIGN opcode");
				if (HasError) return 0;
				instruction = BytecodeUtil.INS_ABC(Opcode.ASSIGN_rA_rB_kC, dest, src, (Byte)constIdx);
				Current.ReserveRegister(dest);

			} else if (mnemonic == "NAME") {
				// NAME r1, "varname"  -->  NAME_rA_kBC
				// Set variable name for r1 without changing its value
				if (parts.Count != 3) {
					Error("Syntax error: NAME requires exactly 2 operands");
					return 0;
				}
				Byte dest = ParseRegister(parts[1]);
				if (HasError) return 0;

				Value constantValue = ParseAsConstant(parts[2]);
				if (!is_string(constantValue)) Error("Variable name must be a string");
				Int32 constIdx = AddConstant(constantValue);
				if (constIdx > 65535) Error("Constant index out of range for NAME opcode");
				if (HasError) return 0;
				instruction = BytecodeUtil.INS_AB(Opcode.NAME_rA_kBC, dest, (Int16)constIdx);
				Current.ReserveRegister(dest);

			} else if (mnemonic == "ADD") {
				if (parts.Count != 4) {
					Error("Syntax error: ADD requires exactly 3 operands");
					return 0;
				}
				Byte dest = ParseRegister(parts[1]);
				Current.ReserveRegister(dest);
				if (HasError) return 0;
				Byte src1 = ParseRegister(parts[2]);
				if (HasError) return 0;
				Byte src2 = ParseRegister(parts[3]);
				if (HasError) return 0;
				instruction = BytecodeUtil.INS_ABC(Opcode.ADD_rA_rB_rC, dest, src1, src2);
				
			} else if (mnemonic == "SUB") {
				if (parts.Count != 4) { Error("Syntax error"); return 0; }
				Byte dest = ParseRegister(parts[1]);
				Current.ReserveRegister(dest);
				Byte src1 = ParseRegister(parts[2]);
				Byte src2 = ParseRegister(parts[3]);
				instruction = BytecodeUtil.INS_ABC(Opcode.SUB_rA_rB_rC, dest, src1, src2);
				
			} else if (mnemonic == "MULT") {
				if (parts.Count != 4) { Error("Syntax error"); return 0; }
				Byte dest = ParseRegister(parts[1]);
				Current.ReserveRegister(dest);
				Byte src1 = ParseRegister(parts[2]);
				Byte src2 = ParseRegister(parts[3]);
				instruction = BytecodeUtil.INS_ABC(Opcode.MULT_rA_rB_rC, dest, src1, src2);
			
			} else if (mnemonic == "DIV") {
				if (parts.Count != 4) { Error("Syntax error"); return 0; }
				Byte dest = ParseRegister(parts[1]);
				Current.ReserveRegister(dest);
				Byte src1 = ParseRegister(parts[2]);
				Byte src2 = ParseRegister(parts[3]);
				instruction = BytecodeUtil.INS_ABC(Opcode.DIV_rA_rB_rC, dest, src1, src2);

			} else if (mnemonic == "MOD") {
				if (parts.Count != 4) { Error("Syntax error"); return 0; }
				Byte dest = ParseRegister(parts[1]);
				Current.ReserveRegister(dest);
				Byte src1 = ParseRegister(parts[2]);
				Byte src2 = ParseRegister(parts[3]);
				instruction = BytecodeUtil.INS_ABC(Opcode.MOD_rA_rB_rC, dest, src1, src2);
			
			} else if (mnemonic == "LIST") {
				if (parts.Count != 3) { Error("Syntax error"); return 0; }
				Byte dest = ParseRegister(parts[1]);
				Current.ReserveRegister(dest);
				Int16 capacity = ParseInt16(parts[2]);
				instruction = BytecodeUtil.INS_AB(Opcode.LIST_rA_iBC, dest, capacity);

			} else if (mnemonic == "MAP") {
				if (parts.Count != 3) { Error("Syntax error"); return 0; }
				Byte dest = ParseRegister(parts[1]);
				Current.ReserveRegister(dest);
				Int16 capacity = ParseInt16(parts[2]);
				instruction = BytecodeUtil.INS_AB(Opcode.MAP_rA_iBC, dest, capacity);

			} else if (mnemonic == "PUSH") {
				if (parts.Count != 3) { Error("Syntax error"); return 0; }
				Byte listReg = ParseRegister(parts[1]);
				Byte valueReg = ParseRegister(parts[2]);
				instruction = BytecodeUtil.INS_ABC(Opcode.PUSH_rA_rB, listReg, valueReg, 0);
			
			} else if (mnemonic == "INDEX") {
				if (parts.Count != 4) { Error("Syntax error"); return 0; }
				Byte dest = ParseRegister(parts[1]);
				Current.ReserveRegister(dest);
				Byte listReg = ParseRegister(parts[2]);
				Byte indexReg = ParseRegister(parts[3]);
				instruction = BytecodeUtil.INS_ABC(Opcode.INDEX_rA_rB_rC, dest, listReg, indexReg);
			
			} else if (mnemonic == "IDXSET") {
				if (parts.Count != 4) { Error("Syntax error"); return 0; }
				Byte listReg = ParseRegister(parts[1]);
				Byte indexReg = ParseRegister(parts[2]);
				Byte valueReg = ParseRegister(parts[3]);
				instruction = BytecodeUtil.INS_ABC(Opcode.IDXSET_rA_rB_rC, listReg, indexReg, valueReg);

			} else if (mnemonic == "LOCALS") {
				if (parts.Count != 2) { Error("Syntax error: LOCALS requires exactly 1 operand"); return 0; }
				Byte reg = ParseRegister(parts[1]);
				Current.ReserveRegister(reg);
				instruction = BytecodeUtil.INS_A(Opcode.LOCALS_rA, reg);

			} else if (mnemonic == "OUTER") {
				if (parts.Count != 2) { Error("Syntax error: OUTER requires exactly 1 operand"); return 0; }
				Byte reg = ParseRegister(parts[1]);
				Current.ReserveRegister(reg);
				instruction = BytecodeUtil.INS_A(Opcode.OUTER_rA, reg);

			} else if (mnemonic == "GLOBALS") {
				if (parts.Count != 2) { Error("Syntax error: GLOBALS requires exactly 1 operand"); return 0; }
				Byte reg = ParseRegister(parts[1]);
				Current.ReserveRegister(reg);
				instruction = BytecodeUtil.INS_A(Opcode.GLOBALS_rA, reg);

			} else if (mnemonic == "JUMP") {
				if (parts.Count != 2) { Error("Syntax error"); return 0; }
				String target = parts[1];
				Int32 offset;
				
				// Check if target is a label or a number
				Int32 labelAddr = FindLabelAddress(target);
				if (labelAddr >= 0) {
					// It's a label - calculate relative offset from next instruction
					offset = labelAddr - (Current.Code.Count + 1);
				} else {
					// It's a number (up to 24 bits allowed)
					offset = ParseInt24(target);
				}
				instruction = BytecodeUtil.INS(Opcode.JUMP_iABC) | (UInt32)(offset & 0xFFFFFF);

			} else if (mnemonic == "LT") {
				if (parts.Count != 4) { Error("Syntax error"); return 0; }
				
				if (parts[3][0] == 'r') {
					if (parts[2][0] == 'r') {
						// LT r5, r3, r2  -->  LT_rA_rB_rC
						Byte reg1 = ParseRegister(parts[1]);
						Current.ReserveRegister(reg1);
						Byte reg2 = ParseRegister(parts[2]);
						Byte reg3 = ParseRegister(parts[3]);
						instruction = BytecodeUtil.INS_ABC(Opcode.LT_rA_rB_rC, reg1, reg2, reg3);
					}
					else{
						// LT r5, 15, r2  -->  LT_rA_iB_rC
						Byte reg1 = ParseRegister(parts[1]);
						Current.ReserveRegister(reg1);
						Byte immediate = ParseByte(parts[2]);
						Byte reg3 = ParseRegister(parts[3]);
						instruction = BytecodeUtil.INS_ABC(Opcode.LT_rA_iB_rC, reg1, immediate, reg3);
					}
				} else {
					// LT r5, r3, 15  -->  LT_rA_rB_iC
					Byte reg1 = ParseRegister(parts[1]);
					Current.ReserveRegister(reg1);
					Byte reg2 = ParseRegister(parts[2]);
					Byte immediate = ParseByte(parts[3]);
					instruction = BytecodeUtil.INS_ABC(Opcode.LT_rA_rB_iC, reg1, reg2, immediate);
				}

			} else if (mnemonic == "LE") {
				if (parts.Count != 4) { Error("Syntax error"); return 0; }
				
				if (parts[3][0] == 'r') {
					if (parts[2][0] == 'r') {
						// LT r5, r3, r2  -->  LT_rA_rB_rC
						Byte reg1 = ParseRegister(parts[1]);
						Current.ReserveRegister(reg1);
						Byte reg2 = ParseRegister(parts[2]);
						Byte reg3 = ParseRegister(parts[3]);
						instruction = BytecodeUtil.INS_ABC(Opcode.LE_rA_rB_rC, reg1, reg2, reg3);
					}
					else{
						// LT r5, 15, r2  -->  LT_rA_iB_rC
						Byte reg1 = ParseRegister(parts[1]);
						Current.ReserveRegister(reg1);
						Byte immediate = ParseByte(parts[2]);
						Byte reg3 = ParseRegister(parts[3]);
						instruction = BytecodeUtil.INS_ABC(Opcode.LE_rA_iB_rC, reg1, immediate, reg3);
					}
				} else {
					// LT r5, r3, 15  -->  LT_rA_rB_iC
					Byte reg1 = ParseRegister(parts[1]);
					Current.ReserveRegister(reg1);
					Byte reg2 = ParseRegister(parts[2]);
					Byte immediate = ParseByte(parts[3]);
					instruction = BytecodeUtil.INS_ABC(Opcode.LE_rA_rB_iC, reg1, reg2, immediate);
				}

			} else if (mnemonic == "EQ") {
				if (parts.Count != 4) { Error("Syntax error"); return 0; }

				Byte reg1 = ParseRegister(parts[1]);
				Current.ReserveRegister(reg1);
				Byte reg2 = ParseRegister(parts[2]);

				if (parts[3][0] == 'r') {
					// EQ r5, r3, r2  -->  EQ_rA_rB_rC
						Byte reg3 = ParseRegister(parts[3]);
						instruction = BytecodeUtil.INS_ABC(Opcode.EQ_rA_rB_rC, reg1, reg2, reg3);
				} else {
					// EQ r5, r3, 15  -->  EQ_rA_rB_iC
					Byte immediate = ParseByte(parts[3]);
					instruction = BytecodeUtil.INS_ABC(Opcode.EQ_rA_rB_iC, reg1, reg2, immediate);
				}

			} else if (mnemonic == "NE") {
				if (parts.Count != 4) { Error("Syntax error"); return 0; }

				Byte reg1 = ParseRegister(parts[1]);
				Current.ReserveRegister(reg1);
				Byte reg2 = ParseRegister(parts[2]);

				if (parts[3][0] == 'r') {
					// NE r5, r3, r2  -->  NE_rA_rB_rC
						Byte reg3 = ParseRegister(parts[3]);
						instruction = BytecodeUtil.INS_ABC(Opcode.NE_rA_rB_rC, reg1, reg2, reg3);
				} else {
					// NE r5, r3, 15  -->  NE_rA_rB_iC
					Byte immediate = ParseByte(parts[3]);
					instruction = BytecodeUtil.INS_ABC(Opcode.NE_rA_rB_iC, reg1, reg2, immediate);
				}
			
			} else if (mnemonic == "BRTRUE") {
				if (parts.Count != 3) { Error("Syntax error"); return 0; }

				Byte reg1 = ParseRegister(parts[1]);
				String target = parts[2];
				Int32 offset;
				
				// Check if target is a label or a number
				Int32 labelAddr = FindLabelAddress(target);
				if (labelAddr >= 0) {
					// It's a label - calculate relative offset from next instruction
					offset = labelAddr - (Current.Code.Count + 1);
				} else {
					// It's a number
					offset = ParseInt32(target);
				}

				// ToDo: handle other cases similar to this, where we parse the number
				// as a bigger Int32 but then check the range, so that we can display
				// a better error message.
				if (offset < Int16.MinValue || offset > Int16.MaxValue) {
					Error("Range error (Cannot fit branch offset into Int16)"); return 0;
				}

				instruction = BytecodeUtil.INS_AB(Opcode.BRTRUE_rA_iBC, reg1, (Int16)offset);

			} else if (mnemonic == "BRFALSE") {
				if (parts.Count != 3) { Error("Syntax error"); return 0; }

				Byte reg1 = ParseRegister(parts[1]);
				String target = parts[2];
				Int32 offset;
				
				// Check if target is a label or a number
				Int32 labelAddr = FindLabelAddress(target);
				if (labelAddr >= 0) {
					// It's a label - calculate relative offset from next instruction
					offset = labelAddr - (Current.Code.Count + 1);
				} else {
					// It's a number
					offset = ParseInt32(target);
				}

				// ToDo: handle other cases similar to this, where we parse the number
				// as a bigger Int32 but then check the range, so that we can display
				// a better error message.
				if (offset < Int16.MinValue || offset > Int16.MaxValue) {
					Error("Range error (Cannot fit branch offset into Int16)"); return 0;
				}

				instruction = BytecodeUtil.INS_AB(Opcode.BRFALSE_rA_iBC, reg1, (Int16)offset);

			} else if (mnemonic == "BRLT") {
				if (parts.Count != 4) { Error("Syntax error"); return 0; }

				String target = parts[3];
				Int32 offset;
				
				// Check if target is a label or a number
				Int32 labelAddr = FindLabelAddress(target);
				if (labelAddr >= 0) {
					// It's a label - calculate relative offset from next instruction
					offset = labelAddr - (Current.Code.Count + 1);
				} else {
					// It's a number
					offset = ParseInt32(target);
				}

				// ToDo: handle other cases similar to this, where we parse the number
				// as a bigger Int32 but then check the range, so that we can display
				// a better error message.
				if (offset < SByte.MinValue || offset > SByte.MaxValue) {
					Error("Range error (Cannot fit branch offset into SByte)"); return 0;
				}

				if (parts[2][0] == 'r') {
					if (parts[1][0] == 'r'){
						// BRLT r5, r3, someOffset
						Byte reg1 = ParseRegister(parts[1]);
						Byte reg2 = ParseRegister(parts[2]);
						instruction = BytecodeUtil.INS_ABC(Opcode.BRLT_rA_rB_iC, reg1, reg2, (Byte)offset);
					} else {
						// BRLT 5, r5, someOffset
						Byte immediate = (Byte)ParseInt16(parts[1]);
						Byte reg2 = ParseRegister(parts[2]);
						instruction = BytecodeUtil.INS_ABC(Opcode.BRLT_iA_rB_iC, immediate, reg2, (Byte)offset);		
					}
				} else {
					// BRLT r5, 5, someOffset
					Byte reg1 = ParseRegister(parts[1]);
					Byte immediate = (Byte)ParseInt16(parts[2]);
					instruction = BytecodeUtil.INS_ABC(Opcode.BRLT_rA_iB_iC, reg1, immediate, (Byte)offset);
				}
			} else if (mnemonic == "BRLE") {
				if (parts.Count != 4) { Error("Syntax error"); return 0; }

				String target = parts[3];
				Int32 offset;
				
				// Check if target is a label or a number
				Int32 labelAddr = FindLabelAddress(target);
				if (labelAddr >= 0) {
					// It's a label - calculate relative offset from next instruction
					offset = labelAddr - (Current.Code.Count + 1);
				} else {
					// It's a number
					offset = ParseInt32(target);
				}

				// ToDo: handle other cases similar to this, where we parse the number
				// as a bigger Int32 but then check the range, so that we can display
				// a better error message.
				if (offset < SByte.MinValue || offset > SByte.MaxValue) {
					Error("Range error (Cannot fit branch offset into SByte)"); return 0;
				}

				if (parts[2][0] == 'r') {
					if (parts[1][0] == 'r'){
						// BRLE r5, r3, someOffset
						Byte reg1 = ParseRegister(parts[1]);
						Byte reg2 = ParseRegister(parts[2]);
						instruction = BytecodeUtil.INS_ABC(Opcode.BRLE_rA_rB_iC, reg1, reg2, (Byte)offset);
					} else {
						// BRLE 5, r5, someOffset
						Byte immediate = (Byte)ParseInt16(parts[1]);
						Byte reg2 = ParseRegister(parts[2]);
						instruction = BytecodeUtil.INS_ABC(Opcode.BRLE_iA_rB_iC, immediate, reg2, (Byte)offset);		
					}
				} else {
					// BRLE r5, 5, someOffset
					Byte reg1 = ParseRegister(parts[1]);
					Byte immediate = (Byte)ParseInt16(parts[2]);
					instruction = BytecodeUtil.INS_ABC(Opcode.BRLE_rA_iB_iC, reg1, immediate, (Byte)offset);
				}			

			} else if (mnemonic == "BREQ") {
				if (parts.Count != 4) { Error("Syntax error"); return 0; }

				String target = parts[3];
				Int32 offset;
				
				// Check if target is a label or a number
				Int32 labelAddr = FindLabelAddress(target);
				if (labelAddr >= 0) {
					// It's a label - calculate relative offset from next instruction
					offset = labelAddr - (Current.Code.Count + 1);
				} else {
					// It's a number
					offset = ParseInt32(target);
				}

				// ToDo: handle other cases similar to this, where we parse the number
				// as a bigger Int32 but then check the range, so that we can display
				// a better error message.
				if (offset < SByte.MinValue || offset > SByte.MaxValue) {
					Error("Range error (Cannot fit branch offset into SByte)"); return 0;
				}

				Byte reg1 = ParseRegister(parts[1]);
				if (parts[2][0] == 'r'){
					// BREQ r5, r3, someOffset
					Byte reg2 = ParseRegister(parts[2]);
					instruction = BytecodeUtil.INS_ABC(Opcode.BREQ_rA_rB_iC, reg1, reg2, (Byte)offset);
				} else {
					// BREQ r5, 5, someOffset
					Byte immediate = (Byte)ParseInt16(parts[2]);
					instruction = BytecodeUtil.INS_ABC(Opcode.BREQ_rA_iB_iC, reg1, immediate, (Byte)offset);
				}
		
			} else if (mnemonic == "BRNE") {
				if (parts.Count != 4) { Error("Syntax error"); return 0; }

				String target = parts[3];
				Int32 offset;
				
				// Check if target is a label or a number
				Int32 labelAddr = FindLabelAddress(target);
				if (labelAddr >= 0) {
					// It's a label - calculate relative offset from next instruction
					offset = labelAddr - (Current.Code.Count + 1);
				} else {
					// It's a number
					offset = ParseInt32(target);
				}

				// ToDo: handle other cases similar to this, where we parse the number
				// as a bigger Int32 but then check the range, so that we can display
				// a better error message.
				if (offset < SByte.MinValue || offset > SByte.MaxValue) {
					Error("Range error (Cannot fit branch offset into SByte)"); return 0;
				}

				Byte reg1 = ParseRegister(parts[1]);
				if (parts[2][0] == 'r') {
					// BRNE r5, r3, someOffset
					Byte reg2 = ParseRegister(parts[2]);
					instruction = BytecodeUtil.INS_ABC(Opcode.BRNE_rA_rB_iC, reg1, reg2, (Byte)offset);
				} else {
					// BRNE r5, 5, someOffset
					Byte immediate = (Byte)ParseInt16(parts[2]);
					instruction = BytecodeUtil.INS_ABC(Opcode.BRNE_rA_iB_iC, reg1, immediate, (Byte)offset);
				}

			} else if (mnemonic == "IFLT") {
				if (parts.Count != 3) { Error("Syntax error"); return 0; }
				
				if (parts[2][0] == 'r') {
					if (parts[1][0] == 'r') {
						// IFLT r5, r3  -->  IFLT_rA_rB
						Byte reg1 = ParseRegister(parts[1]);
						Byte reg2 = ParseRegister(parts[2]);
						instruction = BytecodeUtil.INS_ABC(Opcode.IFLT_rA_rB, reg1, reg2, 0);
					}
					else{
						// IFLT 1337, r3  -->  IFLT_iAB_rc
						Int16 immediate = ParseInt16(parts[1]);
						Byte reg2 = ParseRegister(parts[2]);
						instruction = BytecodeUtil.INS_BC(Opcode.IFLT_iAB_rC, immediate, reg2);
					}
				} else {
					// IFLT r5, 42  -->  IFLT_rA_iBC
					Byte reg1 = ParseRegister(parts[1]);
					Int16 immediate = ParseInt16(parts[2]);
					instruction = BytecodeUtil.INS_AB(Opcode.IFLT_rA_iBC, reg1, immediate);
				}
			
			} else if (mnemonic == "IFLE") {
				if (parts.Count != 3) { Error("Syntax error"); return 0; }
				
				if (parts[2][0] == 'r') {
					if (parts[1][0] == 'r') {
						// IFLE r5, r3  -->  IFLE_rA_rB
						Byte reg1 = ParseRegister(parts[1]);
						Byte reg2 = ParseRegister(parts[2]);
						instruction = BytecodeUtil.INS_ABC(Opcode.IFLE_rA_rB, reg1, reg2, 0);
					} else {
						// IFLE 1337, r3  -->  IFLE_iAB_rc
						Int16 immediate = ParseInt16(parts[1]);
						Byte reg2 = ParseRegister(parts[2]);
						instruction = BytecodeUtil.INS_BC(Opcode.IFLE_iAB_rC, immediate, reg2);
					}
				} else {
					// IFLE r5, 42  -->  IFLE_rA_iBC
					Byte reg1 = ParseRegister(parts[1]);
					Int16 immediate = ParseInt16(parts[2]);
					instruction = BytecodeUtil.INS_AB(Opcode.IFLE_rA_iBC, reg1, immediate);
				}

			} else if (mnemonic == "IFEQ") {
				if (parts.Count != 3) { Error("Syntax error"); return 0; }

				Byte reg1 = ParseRegister(parts[1]);

				if (parts[2][0] == 'r') {
					// IFEQ r5, r3  -->  IFEQ_rA_rB
						Byte reg2 = ParseRegister(parts[2]);
						instruction = BytecodeUtil.INS_ABC(Opcode.IFEQ_rA_rB, reg1, reg2, 0);
				} else {
					// IFEQ r5, 42  -->  IFEQ_rA_iBC
					Int16 immediate = ParseInt16(parts[2]);
					instruction = BytecodeUtil.INS_AB(Opcode.IFEQ_rA_iBC, reg1, immediate);
				}

			} else if (mnemonic == "IFNE") {
				if (parts.Count != 3) { Error("Syntax error"); return 0; }

				Byte reg1 = ParseRegister(parts[1]);

				if (parts[2][0] == 'r') {
					// IFNE r5, r3  -->  IFNE_rA_rB
						Byte reg2 = ParseRegister(parts[2]);
						instruction = BytecodeUtil.INS_ABC(Opcode.IFNE_rA_rB, reg1, reg2, 0);
				} else {
					// IFNE r5, 42  -->  IFNE_rA_iBC
					Int16 immediate = ParseInt16(parts[2]);
					instruction = BytecodeUtil.INS_AB(Opcode.IFNE_rA_iBC, reg1, immediate);
				}

			} else if (mnemonic == "ARGBLK") {
				if (parts.Count != 2) { Error("Syntax error: ARGBLK requires exactly 1 operand"); return 0; }
				Int32 argCount = ParseInt32(parts[1]);
				if (argCount < 0 || argCount > 0xFFFFFF) {
					Error("ARGBLK argument count out of range");
					return 0;
				}
				instruction = BytecodeUtil.INS(Opcode.ARGBLK_iABC) | (UInt32)(argCount & 0xFFFFFF);

			} else if (mnemonic == "ARG") {
				if (parts.Count != 2) { Error("Syntax error: ARG requires exactly 1 operand"); return 0; }
				String arg = parts[1];

				if (arg[0] == 'r') {
					// ARG r5  -->  ARG_rA
					Byte reg = ParseRegister(arg);
					instruction = BytecodeUtil.INS_A(Opcode.ARG_rA, reg);
				} else {
					// ARG 42  -->  ARG_iABC (immediate value, up to 24 bits)
					Int32 immediate = ParseInt32(arg);
					if (immediate < -16777215 || immediate > 16777215) {
						Error("ARG immediate value out of range for 24-bit signed integer");
						return 0;
					}
					instruction = BytecodeUtil.INS(Opcode.ARG_iABC) | (UInt32)(immediate & 0xFFFFFF);
				}

			} else if (mnemonic == "CALLF") {
				if (parts.Count != 3) { Error("Syntax error"); return 0; }
				Byte reserveRegs = (Byte)ParseInt16(parts[1]);	// ToDo: check range before typecast
				Int16 funcIdx = (Int16)FindFunctionIndex(parts[2]);
				if (funcIdx < 0) {
					Error(StringUtils.Format("Unknown function: '{0}'", parts[2]));
					return 0;
				}
				instruction = BytecodeUtil.INS_AB(Opcode.CALLF_iA_iBC, reserveRegs, funcIdx);

			} else if (mnemonic == "CALLFN") {
				if (parts.Count != 3) { Error("Syntax error"); return 0; }
				Byte reserveRegs = (Byte)ParseInt16(parts[1]);	// ToDo: check range before typecast
				Value constantValue = ParseAsConstant(parts[2]);
				if (!is_string(constantValue)) {
					Error(StringUtils.Format("Function name must be a string"));
					return 0;
				}
				Int32 constIdx = AddConstant(constantValue);
				instruction = BytecodeUtil.INS_AB(Opcode.CALLFN_iA_kBC, reserveRegs, (Int16)constIdx);

			} else if (mnemonic == "CALL") {
				if (parts.Count != 4) { Error("Syntax error: CALL requires exactly 3 operands"); return 0; }
				Byte destReg = ParseRegister(parts[1]);
				Current.ReserveRegister(destReg);
				Byte stackReg = ParseRegister(parts[2]);
				Byte funcRefReg = ParseRegister(parts[3]);
				instruction = BytecodeUtil.INS_ABC(Opcode.CALL_rA_rB_rC, destReg, stackReg, funcRefReg);

			} else if (mnemonic == "RETURN") {
				instruction = BytecodeUtil.INS(Opcode.RETURN);
			
			} else {
				Error(StringUtils.Format("Unknown opcode: '{0}'", mnemonic));
				return 0;
			}
			
			// Add instruction to current function (only if no error occurred)
			if (!HasError) Current.Code.Add(instruction);
			
			return instruction;
		}
		
		// Helper to parse register like "r5" -> 5
		private Byte ParseRegister(String reg) {
			if (reg.Length < 2 || reg[0] != 'r') {
				Error(StringUtils.Format("Invalid register format: '{0}' (expected format: r0, r1, etc.)", reg));
				return 0;
			}
			return (Byte)ParseInt16(reg.Substring(1));
		}

		// Helper to parse a Byte number (handles negative numbers)
		private Byte ParseByte(String num) {
			// Simple number parsing - could be enhanced
			Int64 result = 0;
			Boolean negative = false;
			Int32 start = 0;
			
			if (num.Length > 0 && num[0] == '-') {
				negative = true;
				start = 1;
			}
			
			for (Int32 i = start; i < num.Length; i++) {
				if (num[i] >= '0' && num[i] <= '9') {
					result = result * 10 + (num[i] - '0');
				} else {
					Error(StringUtils.Format("Invalid number format: '{0}' (unexpected character '{1}')", num, num[i]));
					return 0;
				}
			}
			
			if (negative) result = -result;
			if (result < Byte.MinValue || result > Byte.MaxValue) {
				Error(StringUtils.Format("Number '{0}' is out of range for Int16 ({1} to {2})", num, Byte.MinValue, Byte.MaxValue));
				return 0;
			}
			return (Byte)result;
		}
		
		// Helper to parse an Int16 number (handles negative numbers)
		private Int16 ParseInt16(String num) {
			// Simple number parsing - could be enhanced
			Int64 result = 0;
			Boolean negative = false;
			Int32 start = 0;
			
			if (num.Length > 0 && num[0] == '-') {
				negative = true;
				start = 1;
			}
			
			for (Int32 i = start; i < num.Length; i++) {
				if (num[i] >= '0' && num[i] <= '9') {
					result = result * 10 + (num[i] - '0');
				} else {
					Error(StringUtils.Format("Invalid number format: '{0}' (unexpected character '{1}')", num, num[i]));
					return 0;
				}
			}
			
			if (negative) result = -result;
			if (result < Int16.MinValue || result > Int16.MaxValue) {
				Error(StringUtils.Format("Number '{0}' is out of range for Int16 ({1} to {2})", num, Int16.MinValue, Int16.MaxValue));
				return 0;
			}
			return (Int16)result;
		}

		// Helper to parse a 24-bit int number (handles negative numbers)
		private Int32 ParseInt24(String num) {
			// Simple number parsing - could be enhanced
			Int64 result = 0;
			Boolean negative = false;
			Int32 start = 0;
			
			if (num.Length > 0 && num[0] == '-') {
				negative = true;
				start = 1;
			}
			
			for (Int32 i = start; i < num.Length; i++) {
				if (num[i] >= '0' && num[i] <= '9') {
					result = result * 10 + (num[i] - '0');
				} else {
					Error(StringUtils.Format("Invalid number format: '{0}' (unexpected character '{1}')", num, num[i]));
					return 0;
				}
			}
			
			if (negative) result = -result;
			if (result < -16777215 || result > 16777215) {
				Error(StringUtils.Format("Number '{0}' is out of range for 24-bit signed integer (-16777215 to 16777215)", num));
				return 0;
			}
			return (Int32)result;
		}

		// Helper to parse a 32-bit int number (handles negative numbers)
		private Int32 ParseInt32(String num) {
			// Simple number parsing - could be enhanced
			Int64 result = 0;
			Boolean negative = false;
			Int32 start = 0;
			
			if (num.Length > 0 && num[0] == '-') {
				negative = true;
				start = 1;
			}
			
			for (Int32 i = start; i < num.Length; i++) {
				if (num[i] >= '0' && num[i] <= '9') {
					result = result * 10 + (num[i] - '0');
				} else {
					Error(StringUtils.Format("Invalid number format: '{0}' (unexpected character '{1}')", num, num[i]));
					return 0;
				}
			}
			
			if (negative) result = -result;
			if (result < Int32.MinValue || result > Int32.MaxValue) {
				Error(StringUtils.Format("Number '{0}' is out of range for Int32 ({1} to {2})", num, Int32.MinValue, Int32.MaxValue));
				return 0;
			}
			return (Int32)result;
		}

		// Helper to determine if a token is a label (ends with colon)
		private static Boolean IsLabel(String token) {
			return token.Length > 1 && token[token.Length - 1] == ':';
		}

		// Helper to determine if a token is a function label (starts with @ and ends with colon)
		private static Boolean IsFunctionLabel(String token) {
			return token.Length > 2 && token[0] == '@' && token[token.Length - 1] == ':';
		}
		
		// Extract just the label part, e.g. "someLabel:" --> "someLabel"
		private static String ParseLabel(String token) {
			return token.Substring(0, token.Length-1);
		}

		// Add a new empty function to our function list.
		// Return true on success, false if failed (because function already exists).
		private bool AddFunction(String functionName) {
			if (HasFunction(functionName)) {
				IOHelper.Print(StringUtils.Format("ERROR: Function {0} is defined multiple times", functionName));
				return false;
			}
			
			FuncDef newFunc = new FuncDef();
			newFunc.Name = functionName;
			Functions.Add(newFunc);
			return true;
		}

		// Helper to find the address of a label
		private Int32 FindLabelAddress(String labelName) {
			for (Int32 i = 0; i < _labelNames.Count; i++) {
				if (_labelNames[i] == labelName) return _labelAddresses[i];
			}
			return -1; // not found
		}

		// Helper to add a constant to the constants table and return its index
		private Int32 AddConstant(Value value) {
			// First look for an existing content that is the same value
			for (Int32 i = 0; i < Current.Constants.Count; i++) {
				if (value_identical(Current.Constants[i], value)) return i;
			}
			
			// Failing that, add it to the table
			Current.Constants.Add(value);
			return Current.Constants.Count - 1;
		}

		// Helper to check if a token is a string literal (surrounded by quotes)
		private static Boolean IsStringLiteral(String token) {
			return token.Length >= 2 && token[0] == '"' && token[token.Length - 1] == '"'; // CPP: return token.lengthB() >= 2 && token[0] == '"' && token[token.lengthB() - 1] == '"'; // C++ indexes into the bytes, so we need to use lengthB() or change indexing to be character-based.
		}

		// Helper to check if a token needs to be stored as a constant
		// (string literals, floating point numbers, or integers too large for Int16)
		private static Boolean NeedsConstant(String token) {
			if (IsStringLiteral(token)) return true;
			
			// Check if it contains a decimal point (floating point number)
			if (token.Contains(".")) return true;
			
			// Check if it's an integer too large for Int16
			Int32 value = 0;
			Boolean negative = false;
			Int32 start = 0;
			
			if (token.Length > 0 && token[0] == '-') {
				negative = true;
				start = 1;
			}
			
			// Parse the integer
			for (Int32 i = start; i < token.Length; i++) {
				if (token[i] < '0' || token[i] > '9') return false; // Not a number
				value = value * 10 + (token[i] - '0');
			}
			
			Int32 finalValue = negative ? -value : value;
			return finalValue < Int16.MinValue || finalValue > Int16.MaxValue;
		}

		// Helper to create a Value from a token
		private Value ParseAsConstant(String token) {
			if (IsStringLiteral(token)) {
				// Remove quotes and create string value
				String content = token.Substring(1, token.Length - 2);
				return make_string(content);
			}
			
			// Check if it contains a decimal point (floating point number).
			if (token.Contains(".")) {
				// Simple double parsing (basic implementation)
				Double doubleValue = ParseDouble(token);
				return make_double(doubleValue);
			}
			
			// Parse as integer
			Int32 intValue = ParseInt32(token);
			return make_int(intValue);
		}

		// Helper to parse a double from a string (basic implementation)
		private Double ParseDouble(String str) {
			// Find the decimal point
			Int32 dotPos = -1;
			for (Int32 i = 0; i < str.Length; i++) {
				if (str[i] == '.') {
					dotPos = i;
					break;
				}
			}
			
			if (dotPos == -1) {
				// No decimal point, parse as integer
				return (Double)ParseInt16(str);
			}
			
			// Parse integer part
			String intPart = str.Substring(0, dotPos);
			Double result = (Double)ParseInt16(intPart);
			
			// Parse fractional part
			String fracPart = str.Substring(dotPos + 1);
			if (fracPart.Length > 0) {
				Double fracValue = (Double)ParseInt16(fracPart);
				Double divisor = 1.0;
				for (Int32 i = 0; i < fracPart.Length; i++) {
					divisor *= 10.0;
				}
				result += fracValue / divisor;
			}
			
			// Handle negative numbers
			if (str.Length > 0 && str[0] == '-') {
				result = -result;
			}
			
			return result;
		}

		// Multi-function assembly with support for @function: labels
		public void Assemble(List<String> sourceLines) {
			// Clear any previous state
			Functions.Clear();
			Current = new FuncDef();
			_labelNames.Clear();
			_labelAddresses.Clear();

			// Use a temporary string pool for assembly
			Byte savedPool = MemPoolShim.GetDefaultStringPool();
			Byte tempPool = MemPoolShim.GetUnusedPool();
			if (tempPool != 0) MemPoolShim.SetDefaultStringPool(tempPool);
			
			// Skim very quickly through our source lines, collecting
			// function labels (enabling forward calls).
			bool sawMain = false;
			Int32 lineNum = 0;
			for (lineNum = 0; lineNum < sourceLines.Count; lineNum++) {
				if (HasError) return; // Bail out if error occurred
				List<String> tokens = GetTokens(sourceLines[lineNum]);
				if (tokens.Count < 1 || !IsFunctionLabel(tokens[0])) continue;
				String funcName = ParseLabel(tokens[0]);
				if (!AddFunction(funcName)) return;
				if (tokens[0] == "@main:") sawMain = true;
			}
			if (!sawMain) AddFunction("@main");
				
			// Now proceed through the input lines, assembling one function at a time.
			lineNum = 0;
			while (lineNum < sourceLines.Count && !HasError) {			
				List<String> tokens = GetTokens(sourceLines[lineNum]);
				if (tokens.Count == 0) { // empty line or comment only
					lineNum++;
					continue;
				}
				
				// Our first non-empty line will either be "@main:" or an instruction
				// (to go into the implicit @main function).  After that, we will
				// always have a function name (@someFunc) here.
				if (IsFunctionLabel(tokens[0])) {
					// Starting a new function.
					Current.Name = ParseLabel(tokens[0]);
				} else {
					// No function name -- implicit @main.
					Current.Name = "@main";
				}
				
				// Assemble one function, starting at lineNum+1, and proceeding
				// until the next function or end-of-input.  The result will be
				// the line number where we should continue with the next function.	
				lineNum = AssembleFunction(sourceLines, lineNum);

				// Bail out if error occurred during function assembly
				if (HasError) break;

				// Then, store the just-assembled Current function in our function list.
				Int32 slot = FindFunctionIndex(Current.Name);
				Functions[slot] = Current;

				Current = new FuncDef();
			}

			// Before clearing the temporary pool, ensure all function names are interned 
			// in the pool that was active when we started (i.e. a non-temporary pool)
			if (tempPool != 0) {
				MemPoolShim.SetDefaultStringPool(savedPool);
				for (Int32 i = 0; i < Functions.Count; i++) {
					// Re-intern function name in pool 0 (this creates a new String in pool 0)
					Functions[i].Name = MemPoolShim.InternString(Functions[i].Name);
				}
				MemPoolShim.ClearStringPool(tempPool);  // Now safe to clear temp pool
			}
		}

		// Assemble a single function from sourceLines, starting at startLine,
		// and proceeding until we hit another function label or run out of lines.
		// Return the line number after this function ends.
		private Int32 AssembleFunction(List<String> sourceLines, Int32 startLine) {
			// Prepare label names/addresses, just for this function.
			// (So it's OK to reuse the same label in multiple functions!)
			_labelNames.Clear();
			_labelAddresses.Clear();

			// First pass: collect label positions within this function;
			// and also find the end line for the second pass.
			Int32 instructionAddress = 0;
			Int32 endLine = sourceLines.Count;
			for (Int32 i = startLine; i < endLine && !HasError; i++) {
				List<String> tokens = GetTokens(sourceLines[i]);
				if (tokens.Count == 0) continue;

				// Skip initial function label line
				if (i == startLine && IsFunctionLabel(tokens[0])) {
					startLine++;	// (and start assembling on the next line)
					continue;
				}

				// Any other function label ends the function
				if (IsFunctionLabel(tokens[0])) {
					endLine = i;
					break;
				}

				// Check if first token is a regular label
				if (IsLabel(tokens[0])) {
					String labelName = ParseLabel(tokens[0]);
					_labelNames.Add(labelName);
					_labelAddresses.Add(instructionAddress);
				}

				// Check if there's an instruction on this line
				if (!IsLabel(tokens[0]) || tokens[0] == "NOOP" || tokens.Count > 1) {
					instructionAddress++;
				}
			}

			// Second pass: assemble instructions with label resolution
			Current.Code.Clear(); // Clear any previous assembly
			Current.Constants.Clear();
			for (Int32 i = startLine; i < endLine && !HasError; i++) {
				CurrentLineNumber = i + 1; // Set line number for error reporting (1-based)
				CurrentLine = sourceLines[i]; // Set current line for error reporting
				AddLine(sourceLines[i]);
			}
			return endLine;
		}


	}
}
