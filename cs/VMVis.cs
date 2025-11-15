using System;
using static MiniScript.ValueHelpers;
// CPP: #include "value.h"
// CPP: #include "value_list.h"
// CPP: #include "Bytecode.g.h"
// CPP: #include "FuncDef.g.h"
// CPP: #include "IOHelper.g.h"
// CPP: #include "VM.g.h"
// CPP: #include "StringUtils.g.h"
// CPP: #include "MemPoolShim.g.h"
// CPP: #include "CS_Math.h"

/*** BEGIN CPP_ONLY ***
#include "StringUtils.g.h"
#ifdef _WIN32
	#include <windows.h>
#else
	#include <sys/ioctl.h>
	#include <unistd.h>
#endif
*** END CPP_ONLY ***/


namespace MiniScript {

	public class VMVis {
		private const String Esc = "\x1b";
		private const String Clear = Esc + "]2J";
		private const String Reset = Esc + "c";
		private const String Bold = Esc + "[1m";
		private const String Dim = Esc + "[2m";
		private const String Underline = Esc + "[4m";
		private const String Inverse = Esc + "[7m";
		private const String Normal = Esc + "[m";
		
		private const String CursorHome = Esc + "[f";
		private const Int32 CodeDisplayColumn = 0;
		private const Int32 RegisterDisplayColumn = 35;
		private const Int32 CallStackDisplayColumn = 70;

		private Int32 _screenWidth;
		private Int32 _screenHeight;

		// Pool management for temporary display strings
		private Byte _displayPool;
		private Byte _savedPool;
		
		// Be careful to get a *reference* to the VM rather than a deep copy, even
		// in C++.  ToDo: find a more elegant solution to this recurring issue.
		//*** BEGIN CS_ONLY ***
		private VM _vm;
		public VMVis(VM vm) {
			_vm = vm;
			UpdateScreenSize();
			_savedPool = MemPoolShim.GetDefaultStringPool();
			_displayPool = MemPoolShim.GetUnusedPool();
		}
		//*** END CS_ONLY ***
		/*** BEGIN H_ONLY ***
		private: VM& _vm;
		public: inline VMVis(VM& vm) : _vm(vm) {
			UpdateScreenSize();
			_savedPool = MemPoolShim::GetDefaultStringPool();
			_displayPool = MemPoolShim::GetUnusedPool();
		}
		*** END H_ONLY ***/

		public void UpdateScreenSize() {
			//*** BEGIN CS_ONLY ***
			try {
				_screenWidth = Console.WindowWidth;
				_screenHeight = Console.WindowHeight;
			} catch {
				_screenWidth = 80;
				_screenHeight = 24;
			}
			//*** END CS_ONLY ***
			/*** BEGIN CPP_ONLY ***
			#ifdef _WIN32
				_screenWidth = csbi.srWindow.Right - csbi.srWindow.Left + 1;
				_screenHeight = rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;	
			#else
				struct winsize w;
				ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
				_screenWidth = w.ws_col;
				_screenHeight = w.ws_row;
			#endif
			*** END CPP_ONLY ***/
		}

		public Int32 ScreenWidth { get { return _screenWidth; } }
		public Int32 ScreenHeight { get { return _screenHeight; } }

		public String CursorGoTo(int column, int row) {
			return StringUtils.Format("\x1b[{0};{1}H", row, column);
		}

		private void Write(String s) {
			Console.Write(s); // CPP: std::cout << s.c_str();
		}

		public void ClearScreen() {
			UpdateScreenSize();
			Write(Reset + Clear + CursorHome);
		}
		

		public void GoTo(int column, int row) {
			Write(CursorGoTo(column, row));
		}

		private void DrawCodeDisplay() {
			if (_vm.CurrentFunction == null) return;  // CPP:

			FuncDef func = _vm.CurrentFunction; // CPP: FuncDef& func = _vm.CurrentFunction;
			Int32 pc = _vm.PC;

			// Draw function name header
			GoTo(CodeDisplayColumn + 1, 1);
			String header = Bold + func.Name + Normal;
			Write(StringUtils.SpacePad(header, 32));

			// Draw code, with current line in bold
			Int32 startLine = Math.Max(0, pc - (_screenHeight - 4) / 2);
			Int32 endLine = Math.Min(func.Code.Count - 1, startLine + _screenHeight - 4);

			for (Int32 i = startLine; i <= endLine; i++) {
				String prefix = (i == pc) ? "PC: " + Bold : "    ";
				String addr = StringUtils.ZeroPad(i, 4);
				String instruction = Disassembler.ToString(func.Code[i]);
				String line = prefix + addr + ": " + instruction;
				if (i == pc) line += Normal;

				GoTo(CodeDisplayColumn + 1, i - startLine + 2);
				Write(StringUtils.SpacePad(line, i == pc ? 40 : 32));
			}

			// Clear remaining lines if we switched to a shorter function
			Int32 totalCodeLines = _screenHeight - 4;
			Int32 linesDrawn = endLine - startLine + 1;
			for (Int32 i = linesDrawn; i < totalCodeLines; i++) {
				GoTo(CodeDisplayColumn + 1, i + 2);
				Write("                                ");
			}
		}

		private String GetValueTypeCode(Value v) {
			if (is_null(v)) return "nul";
			if (is_int(v)) return "int";
			if (is_double(v)) return "dbl";
			if (is_string(v)) return "str";
			if (is_list(v)) return "lst";
			if (is_map(v)) return "map";
			if (is_funcref(v)) return "fun";
			return "unk";
		}

		private String GetValueDisplayString(Value v) {
			if (is_null(v)) return "";
			return StringUtils.Format("{0}", v);
		}

		private String GetVariableNameDisplay(Value nameVal) {
			if (is_null(nameVal)) {
				return "        "; // 8 spaces for unnamed variables
			}
			String name = StringUtils.Format("{0}", nameVal);
			if (name.Length <= 8) {
				return StringUtils.SpacePad(name, 8);
			} else {
				return name.Substring(0, 7) + "…"; // 7 chars + ellipsis
			}
		}

		private void DrawOneRegister(Int32 stackIndex, String label, Int32 displayRow) {
			Value val = _vm.GetStackValue(stackIndex);
			Value nameVal = _vm.GetStackName(stackIndex);
			String varName = GetVariableNameDisplay(nameVal);
			String typeCode = GetValueTypeCode(val);
			String valueStr = GetValueDisplayString(val);
			String line = varName + label + Dim + typeCode + Normal + " " +
			  StringUtils.SpacePad(valueStr, 24);

			GoTo(RegisterDisplayColumn + 1, displayRow);
			Write(StringUtils.SpacePad(line, 44));
		}

		private void DrawRegisters() {
			if (!_vm.IsRunning) return;

			// Draw header
			GoTo(RegisterDisplayColumn + 1, 1);
			Write(StringUtils.SpacePad(Bold + "Registers" + Normal, 32));

			// Get current base index
			Int32 baseIndex = _vm.BaseIndex;
			Int32 stackSize = _vm.StackSize();

			Int32 displayRow = 2;
			Int32 totalRegLines = _screenHeight - 4;

			// Draw r7 down to r0
			for (Int32 reg = 7; reg >= 0 && displayRow <= totalRegLines + 1; reg--) {
				Int32 stackIndex = baseIndex + reg;
				if (stackIndex < stackSize) {
					String label = StringUtils.Format("r{0} ", reg);
					DrawOneRegister(stackIndex, label, displayRow);
				}
				displayRow++;
			}

			// Draw stack entries below baseIndex
			for (Int32 i = baseIndex - 1; i >= 0 && displayRow <= totalRegLines + 1; i--) {
				String label = "   ";  // unlabeled
				DrawOneRegister(i, label, displayRow);
				displayRow++;
			}

			// Clear remaining lines
			for (Int32 i = displayRow; i <= totalRegLines + 1; i++) {
				GoTo(RegisterDisplayColumn + 1, i);
				Write("                                            ");
			}
		}

		private void DrawCallStack() {
			if (!_vm.IsRunning) return;

			// Draw header
			GoTo(CallStackDisplayColumn + 1, 1);
			Write(StringUtils.SpacePad(Bold + "Call Stack" + Normal, 20));

			Int32 displayRow = 2;
			Int32 maxRows = _screenHeight - 3;

			// Show current function at top
			if (_vm.CurrentFunction) {
				String currentFunc = "* " + _vm.CurrentFunction.Name;
				GoTo(CallStackDisplayColumn + 1, displayRow);
				Write(StringUtils.SpacePad(currentFunc, 20));
				displayRow++;
			}

			// Show call stack frames (most recent first)
			Int32 callDepth = _vm.CallStackDepth();
			for (Int32 i = callDepth - 1; i >= 0 && displayRow <= maxRows; i--) {
				CallInfo frame = _vm.GetCallStackFrame(i);
				String funcName = _vm.GetFunctionName(frame.ReturnFuncIndex);
				String prefix = "  "; // indent to show stack depth
				String line = prefix + funcName + ":" + StringUtils.ZeroPad(frame.ReturnPC, 3);

				GoTo(CallStackDisplayColumn + 1, displayRow);
				Write(StringUtils.SpacePad(line, 20));
				displayRow++;
			}

			// Clear remaining lines
			for (Int32 i = displayRow; i <= maxRows; i++) {
				GoTo(CallStackDisplayColumn + 1, i);
				Write(StringUtils.SpacePad("", 20));
			}
		}

		public void UpdateDisplay() {
			// Switch to temporary display pool for all string operations
			if (_displayPool != 0) MemPoolShim.SetDefaultStringPool(_displayPool);

			DrawCodeDisplay();
			DrawRegisters();
			DrawCallStack();
			GoTo(1, _screenHeight - 2);

			// Clear the display pool and restore original pool
			if (_displayPool != 0) {
				MemPoolShim.ClearStringPool(_displayPool);
				MemPoolShim.SetDefaultStringPool(_savedPool);
			}
		}
	}
}
