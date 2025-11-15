// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: VMVis.cs

#ifndef __VMVIS_H
#define __VMVIS_H

#include "core_includes.h"
#include "value.h"
#include "value_list.h"
#include "Bytecode.g.h"
#include "FuncDef.g.h"
#include "IOHelper.g.h"
#include "VM.g.h"
#include "StringUtils.g.h"
#include "MemPoolShim.g.h"
#include "CS_Math.h"



namespace MiniScript {

	class VMVis {
		private: static const String Esc;
		private: static const String Clear;
		private: static const String Reset;
		private: static const String Bold;
		private: static const String Dim;
		private: static const String Underline;
		private: static const String Inverse;
		private: static const String Normal;
		
		private: static const String CursorHome;
		private: static const Int32 CodeDisplayColumn;
		private: static const Int32 RegisterDisplayColumn;
		private: static const Int32 CallStackDisplayColumn;

		private: Int32 _screenWidth;
		private: Int32 _screenHeight;

		// Pool management for temporary display strings
		private: Byte _displayPool;
		private: Byte _savedPool;
		
		// Be careful to get a *reference* to the VM rather than a deep copy, even
		// in C++.  ToDo: find a more elegant solution to this recurring issue.
		private: VM& _vm;
		public: inline VMVis(VM& vm) : _vm(vm) {
			UpdateScreenSize();
			_savedPool = MemPoolShim::GetDefaultStringPool();
			_displayPool = MemPoolShim::GetUnusedPool();
		}

		public: void UpdateScreenSize();


		public: String CursorGoTo(int column, int row);

		private: void Write(String s);

		public: void ClearScreen();
		

		public: void GoTo(int column, int row);

		private: void DrawCodeDisplay();

		private: String GetValueTypeCode(Value v);

		private: String GetValueDisplayString(Value v);

		private: String GetVariableNameDisplay(Value nameVal);

		private: void DrawOneRegister(Int32 stackIndex, String label, Int32 displayRow);

		private: void DrawRegisters();

		private: void DrawCallStack();

		public: void UpdateDisplay();
	}; // end of class VMVis
}

#endif // __VMVIS_H
