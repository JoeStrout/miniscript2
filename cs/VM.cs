using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
// CPP: #include "value.h"
// CPP: #include "value_list.h"
// CPP: #include "value_string.h"
// CPP: #include "Bytecode.g.h"
// CPP: #include "FuncDef.g.h"
// CPP: #include "IOHelper.g.h"
// CPP: #include "Disassembler.g.h"
// CPP: #include "StringUtils.g.h"
// CPP: #include "dispatch_macros.h"

using static MiniScript.ValueHelpers;

namespace MiniScript {

	using CallInfoRef = CallInfo;
	using FuncDefRef = FuncDef;
	
	// Call stack frame (return info)
	public class CallInfo {
		public Int32 ReturnPC;        // where to continue in caller (PC index)
		public Int32 ReturnBase;      // caller's base pointer (stack index)
		public Int32 ReturnFuncIndex; // caller's function index in functions list
		public Int32 CopyResultToReg; // register number to copy result to, or -1
		public Value LocalVarMap;     // VarMap representing locals, if any
		public Value OuterVarMap;     // VarMap representing outer variables (closure context)

		public CallInfo(Int32 returnPC, Int32 returnBase, Int32 returnFuncIndex, Int32 copyToReg=-1) {
			ReturnPC = returnPC;
			ReturnBase = returnBase;
			ReturnFuncIndex = returnFuncIndex;
			CopyResultToReg = copyToReg;
			LocalVarMap = make_null();
			OuterVarMap = val_null;
		}

		public CallInfo(Int32 returnPC, Int32 returnBase, Int32 returnFuncIndex, Int32 copyToReg, Value outerVars) {
			ReturnPC = returnPC;
			ReturnBase = returnBase;
			ReturnFuncIndex = returnFuncIndex;
			CopyResultToReg = copyToReg;
			LocalVarMap = make_null();
			OuterVarMap = outerVars;
		}

		//*** BEGIN CS_ONLY ***
		public Value GetLocalVarMap(List<Value> registers, List<Value> names, int baseIdx, int regCount) {
			if (is_null(LocalVarMap)) {
				// Create a new VarMap with references to VM's stack and names arrays
				if (regCount == 0) {
					// We have no local vars at all!  Make an ordinary map.
					LocalVarMap = make_map(4);	// This is safe, right?
				} else {
					LocalVarMap = make_varmap(registers, names, baseIdx, regCount);
				}
			}
			return LocalVarMap;
		}
		//*** END CS_ONLY ***
		/*** BEGIN H_ONLY ***
		inline Value GetLocalVarMap(List<Value>& registers, List<Value>& names, int baseIdx, int regCount) {
			if (is_null(LocalVarMap)) {
				// Create a new VarMap with references to VM's stack and names arrays
				if (regCount == 0) {
					// We have no local vars at all!  Make an ordinary map.
					LocalVarMap = make_map(4);	// This is safe, right?
				} else {
					LocalVarMap = make_varmap(&registers[0], &names[0], baseIdx, regCount);
				}
			}
			return LocalVarMap;
		}
		*** END H_ONLY ***/
	}

	// VM state
	public class VM {
		public Boolean DebugMode = false;
		private List<Value> stack;
		private List<Value> names;		// Variable names parallel to stack (null if unnamed)

		private List<CallInfo> callStack;
		private Int32 callStackTop;	   // Index of next free call stack slot

		private List<FuncDef> functions; // functions addressed by CALLF

		// Execution state (persistent across RunSteps calls)
		public Int32 PC { get; private set; }
		private Int32 _currentFuncIndex = -1;
		public FuncDef CurrentFunction { get; private set; }
		public Boolean IsRunning { get; private set; }
		public Int32 BaseIndex { get; private set; }
		public String RuntimeError { get; private set; }

		public Int32 StackSize() {
			return stack.Count;
		}
		public Int32 CallStackDepth() {
			return callStackTop;
		}

		public Value GetStackValue(Int32 index) {
			if (index < 0 || index >= stack.Count) return make_null();
			return stack[index];
		}

		public Value GetStackName(Int32 index) {
			if (index < 0 || index >= names.Count) return make_null();
			return names[index];
		}

		public CallInfo GetCallStackFrame(Int32 index) {
			if (index < 0 || index >= callStackTop) return new CallInfo(0, 0, -1);
			return callStack[index];
		}

		public String GetFunctionName(Int32 funcIndex) {
			if (funcIndex < 0 || funcIndex >= functions.Count) return "???";
			return functions[funcIndex].Name;
		}

		public VM() {
			InitVM(1024, 256);
		}
		
		public VM(Int32 stackSlots, Int32 callSlots) {
			InitVM(stackSlots, callSlots);
		}

		private void InitVM(Int32 stackSlots, Int32 callSlots) {
			stack = new List<Value>();
			names = new List<Value>();
			callStack = new List<CallInfo>();
			functions = new List<FuncDef>();
			callStackTop = 0;
			RuntimeError = "";

			// Initialize stack with null values
			for (Int32 i = 0; i < stackSlots; i++) {
				stack.Add(make_null());
				names.Add(make_null());		// No variable name initially
			}
			
			
			// Pre-allocate call stack capacity
			for (Int32 i = 0; i < callSlots; i++) {
				callStack.Add(new CallInfo(0, 0, -1)); // -1 = invalid function index
			}
		}

		public void RegisterFunction(FuncDef funcDef) {
			functions.Add(funcDef);
		}

		public void Reset(List<FuncDef> allFunctions) {
			// Store all functions for CALLF instructions, and find @main
			FuncDef mainFunc = null; // CPP: FuncDef mainFunc;
			functions.Clear();
			for (Int32 i = 0; i < allFunctions.Count; i++) {
				functions.Add(allFunctions[i]);
				if (functions[i].Name == "@main") mainFunc = functions[i];
			}

			if (!mainFunc) {
				IOHelper.Print("ERROR: No @main function found in VM.Reset");
				return;
			}

			// Basic validation - simplified for C++ transpilation
			if (mainFunc.Code.Count == 0) {
				IOHelper.Print("Entry function has no code");
				return;
			}

			// Find the entry function index
			_currentFuncIndex = -1;
			for (Int32 i = 0; i < functions.Count; i++) {
				if (functions[i].Name == mainFunc.Name) {
					_currentFuncIndex = i;
					break;
				}
			}

			// Initialize execution state
			BaseIndex = 0;			  // entry executes at stack base
			PC = 0;				 // start at entry code
			CurrentFunction = mainFunc;
			IsRunning = true;
			callStackTop = 0;
			RuntimeError = "";

			EnsureFrame(BaseIndex, CurrentFunction.MaxRegs);

			if (DebugMode) {
				IOHelper.Print(StringUtils.Format("VM Reset: Executing {0} out of {1} functions", mainFunc.Name, functions.Count));
			}
		}

		public void RaiseRuntimeError(String message) {
			RuntimeError = message;
			IsRunning = false;
		}
		
		public bool ReportRuntimeError() {
			if (String.IsNullOrEmpty(RuntimeError)) return false;
			IOHelper.Print(StringUtils.Format("Runtime error: {0} [{1} line {2}]",
			  RuntimeError, CurrentFunction.Name, PC - 1));
			return true;
		}

		// Helper for argument processing (FUNCTION_CALLS.md steps 1-3):
		// Process ARG instructions, validate argument count, and set up parameter registers.
		// Returns the PC after the CALL instruction, or -1 on error.
		private Int32 ProcessArguments(Int32 argCount, Int32 startPC, Int32 callerBase, Int32 calleeBase, FuncDef callee, ref List<UInt32> code) {
			Int32 paramCount = callee.ParamNames.Count;

			// Step 1: Validate argument count
			if (argCount > paramCount) {
				RaiseRuntimeError(StringUtils.Format("Too many arguments: got {0}, expected {1}",
				                  argCount, paramCount));
				return -1;
			}

			// Step 2-3: Process ARG instructions, copying values to parameter registers
			// Note: Parameters start at r1 (r0 is reserved for return value)
			Int32 currentPC = startPC;
			for (Int32 i = 0; i < argCount; i++) {
				UInt32 argInstruction = code[currentPC];
				Opcode argOp = (Opcode)BytecodeUtil.OP(argInstruction);

				Value argValue = make_null();
				if (argOp == Opcode.ARG_rA) {
					// Argument from register
					Byte srcReg = BytecodeUtil.Au(argInstruction);
					argValue = stack[callerBase + srcReg];
				} else if (argOp == Opcode.ARG_iABC) {
					// Argument immediate value
					Int32 immediate = BytecodeUtil.ABCs(argInstruction);
					argValue = make_int(immediate);
				} else {
					RaiseRuntimeError("Expected ARG opcode in ARGBLK");
					return -1;
				}

				// Copy argument value to callee's parameter register and assign name
				// Parameters start at r1, so offset by 1
				stack[calleeBase + 1 + i] = argValue;
				names[calleeBase + 1 + i] = callee.ParamNames[i];

				currentPC++;
			}

			return currentPC + 1; // Return PC after the CALL instruction
		}

		// Helper for call setup (FUNCTION_CALLS.md steps 4-6):
		// Initialize remaining parameters with defaults and clear callee's registers.
		// Note: Parameters start at r1 (r0 is reserved for return value)
		private void SetupCallFrame(Int32 argCount, Int32 calleeBase, FuncDef callee) {
			Int32 paramCount = callee.ParamNames.Count;

			// Step 4: Set up remaining parameters with default values
			// Parameters start at r1, so offset by 1
			for (Int32 i = argCount; i < paramCount; i++) {
				stack[calleeBase + 1 + i] = callee.ParamDefaults[i];
				names[calleeBase + 1 + i] = callee.ParamNames[i];
			}

			// Step 5: Clear remaining registers (r0, and any beyond parameters)
			stack[calleeBase] = make_null();
			names[calleeBase] = make_null();
			for (Int32 i = paramCount + 1; i < callee.MaxRegs; i++) {
				stack[calleeBase + i] = make_null();
				names[calleeBase + i] = make_null();
			}

			// Step 6 is handled by the caller (pushing CallInfo, switching frame, etc.)
		}

		public Value Execute(FuncDef entry) {
			return Execute(entry, 0);
		}

		public Value Execute(FuncDef entry, UInt32 maxCycles) {
			// Legacy method - convert to Reset + Run pattern
			List<FuncDef> singleFunc = new List<FuncDef>();
			singleFunc.Add(entry);
			Reset(singleFunc);
			return Run(maxCycles);
		}

		public Value Run(UInt32 maxCycles=0) {
			if (!IsRunning || !CurrentFunction) {
				return make_null();
			}

			// Copy instance variables to locals for performance
			Int32 pc = PC;
			Int32 baseIndex = BaseIndex;
			Int32 currentFuncIndex = _currentFuncIndex;

			FuncDefRef curFunc = CurrentFunction; // should be: FuncDef& curFunc = CurrentFunction;
			Int32 codeCount = curFunc.Code.Count;
			var curCode = curFunc.Code; // CPP: UInt32* curCode = &curFunc.Code[0];
			var curConstants = curFunc.Constants; // CPP: Value* curConstants = &curFunc.Constants[0];

			UInt32 cyclesLeft = maxCycles;
			if (maxCycles == 0) cyclesLeft--;  // wraps to MAX_UINT32

/*** BEGIN CPP_ONLY ***
			Value* stackPtr = &stack[0];
#if VM_USE_COMPUTED_GOTO
			static void* const vm_labels[(int)Opcode::OP__COUNT] = { VM_OPCODES(VM_LABEL_LIST) };
			if (DebugMode) IOHelper::Print("(Running with computed-goto dispatch)");
#else
			if (DebugMode) IOHelper::Print("(Running with switch-based dispatch)");
#endif
*** END CPP_ONLY ***/

			while (IsRunning) {
				// CPP: VM_DISPATCH_TOP();
				if (cyclesLeft == 0) {
					// Update instance variables before returning
					PC = pc;
					BaseIndex = baseIndex;
					_currentFuncIndex = currentFuncIndex;
					CurrentFunction = curFunc;
					return make_null();
				}
				cyclesLeft--;

				if (pc >= codeCount) {
					IOHelper.Print("VM: PC out of bounds");
					IsRunning = false;
					// Update instance variables before returning
					PC = pc;
					BaseIndex = baseIndex;
					_currentFuncIndex = currentFuncIndex;
					CurrentFunction = curFunc;
					return make_null();
				}

				UInt32 instruction = curCode[pc++];
				Span<Value> localStack = CollectionsMarshal.AsSpan(stack).Slice(baseIndex); // CPP: Value* localStack = stackPtr + baseIndex;

				if (DebugMode) {
					// Debug output disabled for C++ transpilation
					IOHelper.Print(StringUtils.Format("{0} {1}: {2}     r0:{3}, r1:{4}, r2:{5}",
						curFunc.Name,
						StringUtils.ZeroPad(pc-1, 4),
						Disassembler.ToString(instruction),
						localStack[0], localStack[1], localStack[2]));
				}

				Opcode opcode = (Opcode)BytecodeUtil.OP(instruction);
				
				switch (opcode) { // CPP: VM_DISPATCH_BEGIN();
				
					case Opcode.NOOP: {
						// No operation
						break;
					}

					case Opcode.LOAD_rA_rB: {
						// R[A] = R[B] (equivalent to MOVE)
						Byte a = BytecodeUtil.Au(instruction);
						Byte b = BytecodeUtil.Bu(instruction);
						localStack[a] = localStack[b];
						break;
					}

					case Opcode.LOAD_rA_iBC: {
						// R[A] = BC (signed 16-bit immediate as integer)
						Byte a = BytecodeUtil.Au(instruction);
						short bc = BytecodeUtil.BCs(instruction);
						localStack[a] = make_int(bc);
						break;
					}

					case Opcode.LOAD_rA_kBC: {
						// R[A] = constants[BC]
						Byte a = BytecodeUtil.Au(instruction);
						UInt16 constIdx = BytecodeUtil.BCu(instruction);
						localStack[a] = curConstants[constIdx];
						break;
					}

					case Opcode.LOADV_rA_rB_kC: {
						// R[A] = R[B], but verify that register B has name matching constants[C]
						Byte a = BytecodeUtil.Au(instruction);
						Byte b = BytecodeUtil.Bu(instruction);
						Byte c = BytecodeUtil.Cu(instruction);

						// Check if the source register has the expected name
						Value expectedName = curConstants[c];
						Value actualName = names[baseIndex + b];
						if (value_identical(expectedName, actualName)) {
							localStack[a] = localStack[b];
						} else {
							// Variable not found in current scope, look in outer context
							localStack[a] = LookupVariable(expectedName);
						}
						break;
					}

					case Opcode.LOADC_rA_rB_kC: {
						// R[A] = R[B], but verify that register B has name matching constants[C]
						// and call the function if the value is a function reference
						Byte a = BytecodeUtil.Au(instruction);
						Byte b = BytecodeUtil.Bu(instruction);
						Byte c = BytecodeUtil.Cu(instruction);

						// Check if the source register has the expected name
						Value expectedName = curConstants[c];
						Value actualName = names[baseIndex + b];
						Value val;
						if (value_identical(expectedName, actualName)) {
							val = localStack[b];
						} else {
							// Variable not found in current scope, look in outer context
							val = LookupVariable(expectedName);
						}

						if (!is_funcref(val)) {
							// Simple case: value is not a funcref, so just copy it
							localStack[a] = val;
						} else {
							// Harder case: value is a funcref, which we must invoke,
							// and then copy the result into localStack[a] upon return.
							Int32 funcIndex = funcref_index(val);
							if (funcIndex < 0 || funcIndex >= functions.Count) {
								IOHelper.Print("LOADC to invalid func");
								return make_null();
							}

							FuncDef callee = functions[funcIndex];
							Value outerVars = funcref_outer_vars(val);

							// Push return info with closure context
							if (callStackTop >= callStack.Count) {
								IOHelper.Print("Call stack overflow");
								return make_null();
							}
							callStack[callStackTop] = new CallInfo(pc, baseIndex, currentFuncIndex, a, outerVars);
							callStackTop++;

							// Switch to callee frame: base slides to argument window
							baseIndex += curFunc.MaxRegs;
							for (Int32 i = 0; i < callee.MaxRegs; i++) { // clear registers (ugh)
								stack[baseIndex + i] = make_null();
								names[baseIndex + i] = make_null();
							}
							pc = 0; // Start at beginning of callee code
							curFunc = callee; // Switch to callee function
							codeCount = curFunc.Code.Count;
							curCode = curFunc.Code; // CPP: curCode = &curFunc.Code[0];
							curConstants = curFunc.Constants; // CPP: curConstants = &curFunc.Constants[0];
							currentFuncIndex = funcIndex; // Switch to callee function index

							EnsureFrame(baseIndex, callee.MaxRegs);
						}
						break;
					}

					case Opcode.FUNCREF_iA_iBC: {
						// R[A] := make_funcref(BC) with closure context
						Byte a = BytecodeUtil.Au(instruction);
						Int16 funcIndex = BytecodeUtil.BCs(instruction);

						// Create function reference with our locals as the closure context
						CallInfoRef frame = callStack[callStackTop];
						Value locals = frame.GetLocalVarMap(stack, names, baseIndex, curFunc.MaxRegs);
						localStack[a] = make_funcref(funcIndex, locals);
						break;
					}

					case Opcode.ASSIGN_rA_rB_kC: {
						// R[A] = R[B] and names[baseIndex + A] = constants[C]
						Byte a = BytecodeUtil.Au(instruction);
						Byte b = BytecodeUtil.Bu(instruction);
						Byte c = BytecodeUtil.Cu(instruction);
						localStack[a] = localStack[b];
						names[baseIndex + a] = curConstants[c];	// OFI: keep localNames?
						break;
					}

					case Opcode.NAME_rA_kBC: {
						// names[baseIndex + A] = constants[BC] (without changing R[A])
						Byte a = BytecodeUtil.Au(instruction);
						UInt16 constIdx = BytecodeUtil.BCu(instruction);
						names[baseIndex + a] = curConstants[constIdx];	// OFI: keep localNames?
						break;
					}

					case Opcode.ADD_rA_rB_rC: {
						// R[A] = R[B] + R[C]
						Byte a = BytecodeUtil.Au(instruction);
						Byte b = BytecodeUtil.Bu(instruction);
						Byte c = BytecodeUtil.Cu(instruction);
						localStack[a] = value_add(localStack[b], localStack[c]);
						break;
					}

					case Opcode.SUB_rA_rB_rC: {
						// R[A] = R[B] - R[C]
						Byte a = BytecodeUtil.Au(instruction);
						Byte b = BytecodeUtil.Bu(instruction);
						Byte c = BytecodeUtil.Cu(instruction);
						localStack[a] = value_sub(localStack[b], localStack[c]);
						break;
					}

					case Opcode.MULT_rA_rB_rC: {
						// R[A] = R[B] * R[C]
						Byte a = BytecodeUtil.Au(instruction);
						Byte b = BytecodeUtil.Bu(instruction);
						Byte c = BytecodeUtil.Cu(instruction);
						localStack[a] = value_mult(localStack[b], localStack[c]);
						break;
					}

					case Opcode.DIV_rA_rB_rC: {
						// R[A] = R[B] * R[C]
						Byte a = BytecodeUtil.Au(instruction);
						Byte b = BytecodeUtil.Bu(instruction);
						Byte c = BytecodeUtil.Cu(instruction);
						localStack[a] = value_div(localStack[b], localStack[c]);
						break;
					}

                    case Opcode.MOD_rA_rB_rC: {
						// R[A] = R[B] % R[C]
						Byte a = BytecodeUtil.Au(instruction);
						Byte b = BytecodeUtil.Bu(instruction);
						Byte c = BytecodeUtil.Cu(instruction);
						localStack[a] = value_mod(localStack[b], localStack[c]);
						break;
					}

					case Opcode.LIST_rA_iBC: {
						// R[A] = make_list(BC)
						Byte a = BytecodeUtil.Au(instruction);
						Int16 capacity = BytecodeUtil.BCs(instruction);
						localStack[a] = make_list(capacity);
						break;
					}

					case Opcode.MAP_rA_iBC: {
						// R[A] = make_map(BC)
						Byte a = BytecodeUtil.Au(instruction);
						Int16 capacity = BytecodeUtil.BCs(instruction);
						localStack[a] = make_map(capacity);
						break;
					}

					case Opcode.PUSH_rA_rB: {
						// list_push(R[A], R[B])
						Byte a = BytecodeUtil.Au(instruction);
						Byte b = BytecodeUtil.Bu(instruction);
						list_push(localStack[a], localStack[b]);
						break;
					}

					case Opcode.INDEX_rA_rB_rC: {
						// R[A] = R[B][R[C]] (supports both lists and maps)
						Byte a = BytecodeUtil.Au(instruction);
						Byte b = BytecodeUtil.Bu(instruction);
						Byte c = BytecodeUtil.Cu(instruction);
						Value container = localStack[b];
						Value index = localStack[c];

						if (is_list(container)) {
							// ToDo: add a list_try_get and use it here, like we do with map below
							localStack[a] = list_get(container, as_int(index));
						} else if (is_map(container)) {
							Value result;
							if (!map_try_get(container, index, out result)) {
								RaiseRuntimeError(StringUtils.Format("Key Not Found: '{0}' not found in map", index));
							}
							localStack[a] = result;
						} else {
							RaiseRuntimeError(StringUtils.Format("Can't index into {0}", container));
							localStack[a] = make_null();
						}
						break;
					}

					case Opcode.IDXSET_rA_rB_rC: {
						// R[A][R[B]] = R[C] (supports both lists and maps)
						Byte a = BytecodeUtil.Au(instruction);
						Byte b = BytecodeUtil.Bu(instruction);
						Byte c = BytecodeUtil.Cu(instruction);
						Value container = localStack[a];
						Value index = localStack[b];
						Value value = localStack[c];

						if (is_list(container)) {
							list_set(container, as_int(index), value);
						} else if (is_map(container)) {
							map_set(container, index, value);
						} else {
							RaiseRuntimeError(StringUtils.Format("Can't set indexed value in {0}", container));
						}
						break;
					}

					case Opcode.LOCALS_rA: {
						// Create VarMap for local variables and store in R[A]
						Byte a = BytecodeUtil.Au(instruction);

						CallInfoRef frame = callStack[callStackTop];
						localStack[a] = frame.GetLocalVarMap(stack, names, baseIndex, curFunc.MaxRegs);
						names[baseIndex+a] = make_null();
						break;
					}

					case Opcode.OUTER_rA: {
						// Create VarMap for outer variables and store in R[A]
						// TODO: Implement outer variable map access
						Byte a = BytecodeUtil.Au(instruction);
						CallInfoRef frame = callStack[callStackTop-1];
						localStack[a] = frame.OuterVarMap;
						names[baseIndex+a] = make_null();
						break;
					}

					case Opcode.GLOBALS_rA: {
						// Create VarMap for global variables and store in R[A]
						// TODO: Implement global variable map access
						Byte a = BytecodeUtil.Au(instruction);
						Int32 globalRegCount = functions[callStack[0].ReturnFuncIndex].MaxRegs;
						localStack[a] = callStack[0].GetLocalVarMap(stack, names, 0, globalRegCount);
						names[baseIndex+a] = make_null();
						break;
					}

					case Opcode.JUMP_iABC: {
						// Jump by signed 24-bit ABC offset from current PC
						Int32 offset = BytecodeUtil.ABCs(instruction);
						pc += offset;
						break;
					}

					case Opcode.LT_rA_rB_rC: {
						// if R[A] = R[B] < R[C]
						Byte a = BytecodeUtil.Au(instruction);
						Byte b = BytecodeUtil.Bu(instruction);
						Byte c = BytecodeUtil.Cu(instruction);

						localStack[a] = make_int(value_lt(localStack[b], localStack[c]));
						break;
					}

					case Opcode.LT_rA_rB_iC: {
						// if R[A] = R[B] < C (immediate)
						Byte a = BytecodeUtil.Au(instruction);
						Byte b = BytecodeUtil.Bu(instruction);
						SByte c = BytecodeUtil.Cs(instruction);
						
						localStack[a] = make_int(value_lt(localStack[b], make_int(c)));
						break;
					}

					case Opcode.LT_rA_iB_rC: {
						// if R[A] = B (immediate) < R[C]
						Byte a = BytecodeUtil.Au(instruction);
						SByte b = BytecodeUtil.Bs(instruction);
						Byte c = BytecodeUtil.Cu(instruction);
						
						localStack[a] = make_int(value_lt(make_int(b), localStack[c]));
						break;
					}

					case Opcode.LE_rA_rB_rC: {
						// if R[A] = R[B] <= R[C]
						Byte a = BytecodeUtil.Au(instruction);
						Byte b = BytecodeUtil.Bu(instruction);
						Byte c = BytecodeUtil.Cu(instruction);

						localStack[a] = make_int(value_le(localStack[b], localStack[c]));
						break;
					}

					case Opcode.LE_rA_rB_iC: {
						// if R[A] = R[B] <= C (immediate)
						Byte a = BytecodeUtil.Au(instruction);
						Byte b = BytecodeUtil.Bu(instruction);
						SByte c = BytecodeUtil.Cs(instruction);
						
						localStack[a] = make_int(value_le(localStack[b], make_int(c)));
						break;
					}

					case Opcode.LE_rA_iB_rC: {
						// if R[A] = B (immediate) <= R[C]
						Byte a = BytecodeUtil.Au(instruction);
						SByte b = BytecodeUtil.Bs(instruction);
						Byte c = BytecodeUtil.Cu(instruction);
						
						localStack[a] = make_int(value_le(make_int(b), localStack[c]));
						break;
					}

					case Opcode.EQ_rA_rB_rC: {
						// if R[A] = R[B] == R[C]
						Byte a = BytecodeUtil.Au(instruction);
						Byte b = BytecodeUtil.Bu(instruction);
						Byte c = BytecodeUtil.Cu(instruction);

						localStack[a] = make_int(value_equal(localStack[b], localStack[c]));
						break;
					}

					case Opcode.EQ_rA_rB_iC: {
						// if R[A] = R[B] == C (immediate)
						Byte a = BytecodeUtil.Au(instruction);
						Byte b = BytecodeUtil.Bu(instruction);
						SByte c = BytecodeUtil.Cs(instruction);
						
						localStack[a] = make_int(value_equal(localStack[b], make_int(c)));
						break;
					}

					case Opcode.NE_rA_rB_rC: {
						// if R[A] = R[B] != R[C]
						Byte a = BytecodeUtil.Au(instruction);
						Byte b = BytecodeUtil.Bu(instruction);
						Byte c = BytecodeUtil.Cu(instruction);

						localStack[a] = make_int(!value_equal(localStack[b], localStack[c]));
						break;
					}

					case Opcode.NE_rA_rB_iC: {
						// if R[A] = R[B] != C (immediate)
						Byte a = BytecodeUtil.Au(instruction);
						Byte b = BytecodeUtil.Bu(instruction);
						SByte c = BytecodeUtil.Cs(instruction);
						
						localStack[a] = make_int(!value_equal(localStack[b], make_int(c)));
						break;
					}

					case Opcode.BRTRUE_rA_iBC: {
						Byte a = BytecodeUtil.Au(instruction);
						Int32 offset = BytecodeUtil.BCs(instruction);
						if (is_truthy(localStack[a])){
							pc += offset;
						}
						break;
					}

					case Opcode.BRFALSE_rA_iBC: {
						Byte a = BytecodeUtil.Au(instruction);
						Int32 offset = BytecodeUtil.BCs(instruction);
						if (!is_truthy(localStack[a])){
							pc += offset;
						}
						break;
					}

					case Opcode.BRLT_rA_rB_iC: {
						// if R[A] < R[B] then jump offset C.
						Byte a = BytecodeUtil.Au(instruction);
						Byte b = BytecodeUtil.Bu(instruction);
						SByte offset = BytecodeUtil.Cs(instruction);
						if (value_lt(localStack[a], localStack[b])){
							pc += offset;
						}
						break;
					}

					case Opcode.BRLT_rA_iB_iC: {
						// if R[A] < B (immediate) then jump offset C.
						Byte a = BytecodeUtil.Au(instruction);
						SByte b = BytecodeUtil.Bs(instruction);
						SByte offset = BytecodeUtil.Cs(instruction);
						if (value_lt(localStack[a], make_int(b))){
							pc += offset;
						}
						break;
					}

					case Opcode.BRLT_iA_rB_iC: {
						// if A (immediate) < R[B] then jump offset C.
						SByte a = BytecodeUtil.As(instruction);
						Byte b = BytecodeUtil.Bu(instruction);
						SByte offset = BytecodeUtil.Cs(instruction);
						if (value_lt(make_int(a), localStack[b])){
							pc += offset;
						}
						break;
					}

					case Opcode.BRLE_rA_rB_iC: {
						// if R[A] <= R[B] then jump offset C.
						Byte a = BytecodeUtil.Au(instruction);
						Byte b = BytecodeUtil.Bu(instruction);
						SByte offset = BytecodeUtil.Cs(instruction);
						if (value_le(localStack[a], localStack[b])){
							pc += offset;
						}
						break;
					}

					case Opcode.BRLE_rA_iB_iC: {
						// if R[A] <= B (immediate) then jump offset C.
						Byte a = BytecodeUtil.Au(instruction);
						SByte b = BytecodeUtil.Bs(instruction);
						SByte offset = BytecodeUtil.Cs(instruction);
						if (value_le(localStack[a], make_int(b))){
							pc += offset;
						}
						break;
					}

					case Opcode.BRLE_iA_rB_iC: {
						// if A (immediate) <= R[B] then jump offset C.
						SByte a = BytecodeUtil.As(instruction);
						Byte b = BytecodeUtil.Bu(instruction);
						SByte offset = BytecodeUtil.Cs(instruction);
						if (value_le(make_int(a), localStack[b])){
							pc += offset;
						}
						break;
					}

					case Opcode.BREQ_rA_rB_iC: {
						// if R[A] == R[B] then jump offset C.
						Byte a = BytecodeUtil.Au(instruction);
						Byte b = BytecodeUtil.Bu(instruction);
						SByte offset = BytecodeUtil.Cs(instruction);
						if (value_equal(localStack[a], localStack[b])){
							pc += offset;
						}
						break;
					}

					case Opcode.BREQ_rA_iB_iC: {
						// if R[A] == B (immediate) then jump offset C.
						Byte a = BytecodeUtil.Au(instruction);
						SByte b = BytecodeUtil.Bs(instruction);
						SByte offset = BytecodeUtil.Cs(instruction);
						if (value_equal(localStack[a], make_int(b))){
							pc += offset;
						}
						break;
					}

					case Opcode.BRNE_rA_rB_iC: {
						// if R[A] != R[B] then jump offset C.
						Byte a = BytecodeUtil.Au(instruction);
						Byte b = BytecodeUtil.Bu(instruction);
						SByte offset = BytecodeUtil.Cs(instruction);
						if (!value_equal(localStack[a], localStack[b])){
							pc += offset;
						}
						break;
					}

					case Opcode.BRNE_rA_iB_iC: {
						// if R[A] != B (immediate) then jump offset C.
						Byte a = BytecodeUtil.Au(instruction);
						SByte b = BytecodeUtil.Bs(instruction);
						SByte offset = BytecodeUtil.Cs(instruction);
						if (!value_equal(localStack[a], make_int(b))){
							pc += offset;
						}
						break;
					}

					case Opcode.IFLT_rA_rB: {
						// if R[A] < R[B] is false, skip next instruction
						Byte a = BytecodeUtil.Au(instruction);
						Byte b = BytecodeUtil.Bu(instruction);
						if (!value_lt(localStack[a], localStack[b])) {
							pc++; // Skip next instruction
						}
						break;
					}

					case Opcode.IFLT_rA_iBC: {
						// if R[A] < BC (immediate) is false, skip next instruction
						Byte a = BytecodeUtil.Au(instruction);
						short bc = BytecodeUtil.BCs(instruction);
						if (!value_lt(localStack[a], make_int(bc))) {
							pc++; // Skip next instruction
						}
						break;
					}

                    case Opcode.IFLT_iAB_rC: {
						// if AB (immediate) < R[C] is false, skip next instruction
						short ab = BytecodeUtil.ABs(instruction);
                        Byte c = BytecodeUtil.Cu(instruction);
						if (!value_lt(make_int(ab), localStack[c])) {
							pc++; // Skip next instruction
						}
						break;
					}

                    case Opcode.IFLE_rA_rB: {
						// if R[A] <= R[B] is false, skip next instruction
						Byte a = BytecodeUtil.Au(instruction);
						Byte b = BytecodeUtil.Bu(instruction);
						if (!value_le(localStack[a], localStack[b])) {
							pc++; // Skip next instruction
						}
						break;
					}

					case Opcode.IFLE_rA_iBC: {
						// if R[A] <= BC (immediate) is false, skip next instruction
						Byte a = BytecodeUtil.Au(instruction);
						short bc = BytecodeUtil.BCs(instruction);
						if (!value_le(localStack[a], make_int(bc))) {
							pc++; // Skip next instruction
						}
						break;
					}

                    case Opcode.IFLE_iAB_rC: {
						// if AB (immediate) <= R[C] is false, skip next instruction
						short ab = BytecodeUtil.ABs(instruction);
                        Byte c = BytecodeUtil.Cu(instruction);
						if (!value_le(make_int(ab), localStack[c])) {
							pc++; // Skip next instruction
						}
						break;
					}

                    case Opcode.IFEQ_rA_rB: {
						// if R[A] == R[B] is false, skip next instruction
						Byte a = BytecodeUtil.Au(instruction);
						Byte b = BytecodeUtil.Bu(instruction);
						if (!value_equal(localStack[a], localStack[b])) {
							pc++; // Skip next instruction
						}
						break;
					}

                    case Opcode.IFEQ_rA_iBC: {
						// if R[A] == BC (immediate) is false, skip next instruction
						Byte a = BytecodeUtil.Au(instruction);
						short bc = BytecodeUtil.BCs(instruction);
						if (!value_equal(localStack[a], make_int(bc))) {
							pc++; // Skip next instruction
						}
						break;
					}

                    case Opcode.IFNE_rA_rB: {
						// if R[A] != R[B] is false, skip next instruction
						Byte a = BytecodeUtil.Au(instruction);
						Byte b = BytecodeUtil.Bu(instruction);
						if (value_equal(localStack[a], localStack[b])) {
							pc++; // Skip next instruction
						}
						break;
					}

                    case Opcode.IFNE_rA_iBC: {
						// if R[A] != BC (immediate) is false, skip next instruction
						Byte a = BytecodeUtil.Au(instruction);
						short bc = BytecodeUtil.BCs(instruction);
						if (value_equal(localStack[a], make_int(bc))) {
							pc++; // Skip next instruction
						}
						break;
					}

					case Opcode.ARGBLK_iABC: {
						// Begin argument block with specified count
						// ABC: number of ARG instructions that follow
						Int32 argCount = BytecodeUtil.ABCs(instruction);

						// Look ahead to find the CALL instruction (argCount instructions ahead)
						Int32 callPC = pc + argCount;
						if (callPC >= codeCount) {
							RaiseRuntimeError("ARGBLK: CALL instruction out of range");
							return make_null();
						}
						UInt32 callInstruction = curCode[callPC];
						Opcode callOp = (Opcode)BytecodeUtil.OP(callInstruction);

						// Extract call parameters based on opcode type
						FuncDef callee = null; // CPP: FuncDef callee;
						Int32 calleeBase = 0;
						Int32 resultReg = -1;

						if (callOp == Opcode.CALL_rA_rB_rC) {
							// CALL r[A], r[B], r[C]: invoke funcref in r[C], frame at r[B], result to r[A]
							Byte a = BytecodeUtil.Au(callInstruction);
							Byte b = BytecodeUtil.Bu(callInstruction);
							Byte c = BytecodeUtil.Cu(callInstruction);

							Value funcRefValue = localStack[c];
							if (!is_funcref(funcRefValue)) {
								RaiseRuntimeError("ARGBLK/CALL: Not a function reference");
								return make_null();
							}

							Int32 funcIndex = funcref_index(funcRefValue);
							if (funcIndex < 0 || funcIndex >= functions.Count) {
								RaiseRuntimeError("ARGBLK/CALL: Invalid function index");
								return make_null();
							}
							callee = functions[funcIndex];
							calleeBase = baseIndex + b;
							resultReg = a;
						} else {
							RaiseRuntimeError("ARGBLK must be followed by CALL");
							return make_null();
						}

						// Process arguments using helper
						Int32 nextPC = ProcessArguments(argCount, pc, baseIndex, calleeBase, callee, ref curFunc.Code);
						if (nextPC < 0) return make_null(); // Error already raised

						// Set up call frame using helper
						SetupCallFrame(argCount, calleeBase, callee);

						// Now execute the CALL (step 6): push CallInfo and switch to callee
						if (callStackTop >= callStack.Count) {
							RaiseRuntimeError("Call stack overflow");
							return make_null();
						}

						Int32 funcIndex2 = funcref_index(localStack[BytecodeUtil.Cu(callInstruction)]);
						Value outerVars = funcref_outer_vars(localStack[BytecodeUtil.Cu(callInstruction)]);
						callStack[callStackTop] = new CallInfo(nextPC, baseIndex, currentFuncIndex, resultReg, outerVars);
						callStackTop++;

						baseIndex = calleeBase;
						pc = 0; // Start at beginning of callee code
						curFunc = callee; // Switch to callee function
						codeCount = curFunc.Code.Count;
						curCode = curFunc.Code; // CPP: curCode = &curFunc.Code[0];
						curConstants = curFunc.Constants; // CPP: curConstants = &curFunc.Constants[0];
						currentFuncIndex = funcIndex2;
						EnsureFrame(baseIndex, callee.MaxRegs);
						break;
					}

					case Opcode.ARG_rA: {
						// The VM should never encounter this opcode on its own; it will
						// be processed as part of the ARGBLK opcode.  So if we get
						// here, it's an error.
						RaiseRuntimeError("Internal error: ARG without ARGBLK");
						return make_null();
					}

					case Opcode.ARG_iABC: {
						// The VM should never encounter this opcode on its own; it will
						// be processed as part of the ARGBLK opcode.  So if we get
						// here, it's an error.
						RaiseRuntimeError("Internal error: ARG without ARGBLK");
						return make_null();
					}

					case Opcode.CALLF_iA_iBC: {
						// A: arg window start (callee executes with base = base + A)
						// BC: function index
						Byte a = BytecodeUtil.Au(instruction);
						UInt16 funcIndex = BytecodeUtil.BCu(instruction);
						
						if (funcIndex >= functions.Count) {
							IOHelper.Print("CALLF to invalid func");
							return make_null();
						}
						
						FuncDef callee = functions[funcIndex];

						// Push return info
						if (callStackTop >= callStack.Count) {
							IOHelper.Print("Call stack overflow");
							return make_null();
						}
						callStack[callStackTop] = new CallInfo(pc, baseIndex, currentFuncIndex);
						callStackTop++;

						// Switch to callee frame: base slides to argument window
						baseIndex += a;
						pc = 0; // Start at beginning of callee code
						curFunc = callee; // Switch to callee function
						codeCount = curFunc.Code.Count;
						curCode = curFunc.Code; // CPP: curCode = &curFunc.Code[0];
						curConstants = curFunc.Constants; // CPP: curConstants = &curFunc.Constants[0];
						currentFuncIndex = funcIndex; // Switch to callee function index

						EnsureFrame(baseIndex, callee.MaxRegs);
						break;
					}
					
					case Opcode.CALLFN_iA_kBC: {
						// Call named (intrinsic?) function kBC,
						// with parameters/return at register A.
						Byte a = BytecodeUtil.Au(instruction);
						UInt16 constIdx = BytecodeUtil.BCu(instruction);
						Value funcName = curConstants[constIdx];
						// For now, we'll only support intrinsics.
						// ToDo: change this once we have variable look-up.
						DoIntrinsic(funcName, baseIndex + a);
						break;
					}

					case Opcode.CALL_rA_rB_rC: {
						// Invoke the FuncRef in R[C], with a stack frame starting at our register B,
						// and upon return, copy the result from r[B] to r[A].
						//
						// A: destination register for result
						// B: stack frame start register for callee
						// C: register containing FuncRef to call
						Byte a = BytecodeUtil.Au(instruction);
						Byte b = BytecodeUtil.Bu(instruction);
						Byte c = BytecodeUtil.Cu(instruction);

						Value funcRefValue = localStack[c];
						if (!is_funcref(funcRefValue)) {
							IOHelper.Print("CALL: Value in register is not a function reference");
							localStack[a] = funcRefValue;
							break;
						}

						Int32 funcIndex = funcref_index(funcRefValue);
						if (funcIndex < 0 || funcIndex >= functions.Count) {
							IOHelper.Print("CALL: Invalid function index in FuncRef");
							return make_null();
						}
						FuncDef callee = functions[funcIndex];
						Value outerVars = funcref_outer_vars(funcRefValue);

						// For naked CALL (without ARGBLK): set up parameters with defaults
						Int32 calleeBase = baseIndex + b;
						SetupCallFrame(0, calleeBase, callee); // 0 arguments, use all defaults

						if (callStackTop >= callStack.Count) {
							IOHelper.Print("Call stack overflow");
							return make_null();
						}
						callStack[callStackTop] = new CallInfo(pc, baseIndex, currentFuncIndex, a, outerVars);
						callStackTop++;

						// Set up call frame starting at baseIndex + b
						baseIndex = calleeBase;
						pc = 0; // Start at beginning of callee code
						curFunc = callee; // Switch to callee function
						codeCount = curFunc.Code.Count;
						curCode = curFunc.Code; // CPP: curCode = &curFunc.Code[0];
						curConstants = curFunc.Constants; // CPP: curConstants = &curFunc.Constants[0];
						currentFuncIndex = funcIndex; // Switch to callee function index
						EnsureFrame(baseIndex, callee.MaxRegs);
						break;
					}

					case Opcode.RETURN: {
						// Return value convention: value is in base[0]
						Value result = stack[baseIndex];
						if (callStackTop == 0) {
							// Returning from main function: update instance vars and set IsRunning = false
							PC = pc;
							BaseIndex = baseIndex;
							_currentFuncIndex = currentFuncIndex;
							CurrentFunction = curFunc;
							IsRunning = false;
							return result;
						}
						
						// If current call frame had a locals VarMap, gather it up
						CallInfoRef frame = callStack[callStackTop];
						if (!is_null(frame.LocalVarMap)) {
							varmap_gather(frame.LocalVarMap);
							frame.LocalVarMap = make_null();  // then clear from call frame
						}

						// Pop call stack
						callStackTop--;
						CallInfo callInfo = callStack[callStackTop];
						pc = callInfo.ReturnPC;
						baseIndex = callInfo.ReturnBase;
						currentFuncIndex = callInfo.ReturnFuncIndex; // Restore the caller's function index
						curFunc = functions[currentFuncIndex]; // Restore the caller's function
						codeCount = curFunc.Code.Count;
						curCode = curFunc.Code; // CPP: curCode = &curFunc.Code[0];
						curConstants = curFunc.Constants; // CPP: curConstants = &curFunc.Constants[0];
						
						if (callInfo.CopyResultToReg >= 0) {
							stack[baseIndex + callInfo.CopyResultToReg] = result;
						}
						break;
					}

					// CPP: VM_DISPATCH_END();
//*** BEGIN CS_ONLY ***
					default:
						IOHelper.Print("Unknown opcode");
						// Update instance variables before returning
						PC = pc;
						BaseIndex = baseIndex;
						_currentFuncIndex = currentFuncIndex;
						CurrentFunction = curFunc;
						return make_null();
				}
//*** END CS_ONLY ***
			}
			// CPP: VM_DISPATCH_BOTTOM();

			// Update instance variables after loop exit (e.g. from error condition)
			PC = pc;
			BaseIndex = baseIndex;
			_currentFuncIndex = currentFuncIndex;
			CurrentFunction = curFunc;
			return make_null();
		}

		private void EnsureFrame(Int32 baseIndex, UInt16 neededRegs) {
			// Simple implementation - just check bounds
			if (baseIndex + neededRegs > stack.Count) {
				// Simple error handling - just print and continue
				IOHelper.Print("Stack overflow error");
			}
		}

		private Value LookupVariable(Value varName) {
			// Look up a variable in outer context (and eventually globals)
			// Returns the value if found, or null if not found
			if (callStackTop > 0) {
				CallInfo currentFrame = callStack[callStackTop - 1];  // Current frame, not next frame
				if (!is_null(currentFrame.OuterVarMap)) {
					Value outerValue;
					if (map_try_get(currentFrame.OuterVarMap, varName, out outerValue)) {
						return outerValue;
					}
				}
			}

			// ToDo: check globals!

			// Variable not found anywhere
			RaiseRuntimeError(StringUtils.Format("Undefined identifier '{0}'", varName));
			return make_null();
		}
		
		private static readonly Value FuncNamePrint = make_string("print");
		private static readonly Value FuncNameInput = make_string("input");
		private static readonly Value FuncNameVal = make_string("val");
		private static readonly Value FuncNameRemove = make_string("remove");
		
		private void DoIntrinsic(Value funcName, Int32 baseReg) {
			// Run the named intrinsic, with its parameters and return value
			// stored in our stack starting at baseReg.
			
			// Prototype implementation:
			
			if (value_equal(funcName, FuncNamePrint)) {
				IOHelper.Print(StringUtils.Format("{0}", stack[baseReg]));
				stack[baseReg] = make_null();
			
			} else if (value_equal(funcName, FuncNameInput)) {
				String prompt = new String("");
				if (!is_null(stack[baseReg])) {
					prompt = StringUtils.Format("{0}", stack[baseReg]);
				}
				String result = IOHelper.Input(prompt);
				stack[baseReg] = 
				  make_string(result);	// CPP: make_string(result.c_str());
			
			} else if (value_equal(funcName, FuncNameVal)) {
				stack[baseReg] = to_number(stack[baseReg]);
			
			} else if (value_equal(funcName, FuncNameRemove)) {
				// Remove index r1 from map r0; return (in r0) 1 if successful,
				// 0 if index not found.
				Value container = stack[baseReg];
				int result = 0;
				if (is_list(container)) {
					result = list_remove(container, as_int(stack[baseReg+1])) ? 1 : 0;
				} else if (is_map(container)) {
					result = map_remove(container, stack[baseReg+1]) ? 1 : 0;
				} else {
					IOHelper.Print("ERROR: `remove` must be called on list or map");
				}
				stack[baseReg] = make_int(result);
			
			} else {
				IOHelper.Print(
				  StringUtils.Format("ERROR: Unknown function '{0}'", funcName)
				);
				stack[baseReg] = make_null();
				// ToDo: put VM in an error state, so it aborts.
			}
		}
	}
}

//*** END CS_ONLY ***