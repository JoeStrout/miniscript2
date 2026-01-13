// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: VMVis.cs

#pragma once
#include "core_includes.h"



namespace MiniScript {

// FORWARD DECLARATIONS

struct CallInfo;
class CallInfoStorage;
struct VM;
class VMStorage;
struct VMVis;
class VMVisStorage;
struct Assembler;
class AssemblerStorage;
struct FuncDef;
class FuncDefStorage;
struct App;
class AppStorage;

// DECLARATIONS






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
	private: VM& _vm;
	public: inline VMVis(VM& vm) : _vm(vm) {
		UpdateScreenSize();
		_savedPool = MemPoolShim::GetDefaultStringPool();
		_displayPool = MemPoolShim::GetUnusedPool();
	}









class VMVisStorage : public std::enable_shared_from_this<VMVisStorage> {
	friend struct VMVis;
	private: Int32 _screenWidth;
	private: Int32 _screenHeight;
	private: Byte _displayPool;
	private: Byte _savedPool;
	


	// Pool management for temporary display strings
	
	// Be careful to get a *reference* to the VM rather than a deep copy, even
	// in C++.  ToDo: find a more elegant solution to this recurring issue.

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
}; // end of class VMVisStorage

struct VMVis {
	protected: std::shared_ptr<VMVisStorage> storage;
  public:
	VMVis(std::shared_ptr<VMVisStorage> stor) : storage(stor) {}
	VMVis() : storage(nullptr) {}
	static VMVis New() { return VMVis(std::make_shared<VMVisStorage>()); }
	friend bool IsNull(const VMVis& inst) { return inst.storage == nullptr; }
	private: VMVisStorage* get() const { return static_cast<VMVisStorage*>(storage.get()); }

	private: Int32 _screenWidth() { return get()->_screenWidth; }
	private: void set__screenWidth(Int32 _v) { get()->_screenWidth = _v; }
	private: Int32 _screenHeight() { return get()->_screenHeight; }
	private: void set__screenHeight(Int32 _v) { get()->_screenHeight = _v; }
	private: Byte _displayPool() { return get()->_displayPool; }
	private: void set__displayPool(Byte _v) { get()->_displayPool = _v; }
	private: Byte _savedPool() { return get()->_savedPool; }
	private: void set__savedPool(Byte _v) { get()->_savedPool = _v; }
	


	// Pool management for temporary display strings
	
	// Be careful to get a *reference* to the VM rather than a deep copy, even
	// in C++.  ToDo: find a more elegant solution to this recurring issue.

	public: void UpdateScreenSize() { return get()->UpdateScreenSize(); }


	public: String CursorGoTo(int column, int row) { return get()->CursorGoTo(column, row); }

	private: void Write(String s) { return get()->Write(s); }

	public: void ClearScreen() { return get()->ClearScreen(); }
	

	public: void GoTo(int column, int row) { return get()->GoTo(column, row); }

	private: void DrawCodeDisplay() { return get()->DrawCodeDisplay(); }

	private: String GetValueTypeCode(Value v) { return get()->GetValueTypeCode(v); }

	private: String GetValueDisplayString(Value v) { return get()->GetValueDisplayString(v); }

	private: String GetVariableNameDisplay(Value nameVal) { return get()->GetVariableNameDisplay(nameVal); }

	private: void DrawOneRegister(Int32 stackIndex, String label, Int32 displayRow) { return get()->DrawOneRegister(stackIndex, label, displayRow); }

	private: void DrawRegisters() { return get()->DrawRegisters(); }

	private: void DrawCallStack() { return get()->DrawCallStack(); }

	public: void UpdateDisplay() { return get()->UpdateDisplay(); }
}; // end of struct VMVis


// INLINE METHODS

} // end of namespace MiniScript
