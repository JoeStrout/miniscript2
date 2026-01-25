// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: VMVis.cs

#pragma once
#include "core_includes.h"
#include "VM.g.h"



namespace MiniScript {

// FORWARD DECLARATIONS

struct VM;
class VMStorage;
struct Assembler;
class AssemblerStorage;
struct FuncDef;
class FuncDefStorage;

// DECLARATIONS






struct VMVis {
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
	private: Int32 _screenWidth = 0;
	private: Int32 _screenHeight = 0;
	private: VM _vm;
	

	
	public: VMVis(VM vm);

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
}; // end of struct VMVis












// INLINE METHODS

} // end of namespace MiniScript
