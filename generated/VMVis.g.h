// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: VMVis.cs

#pragma once
#include "core_includes.h"



namespace MiniScript {

// FORWARD DECLARATIONS

struct CallInfo;
class CallInfoStorage;
struct AcceptException;
class AcceptExceptionStorage;
struct VMVis;
class VMVisStorage;
struct Assembler;
class AssemblerStorage;
struct FuncDef;
class FuncDefStorage;
struct Lexer;
class LexerStorage;

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
	private: Int32 _screenWidth;
	private: Int32 _screenHeight;
	private: Byte _displayPool;
	private: Byte _savedPool;
	public: void UpdateScreenSize();
}; // end of class VMVisStorage

struct VMVis {
	protected: std::shared_ptr<VMVisStorage> storage;
  public:
	VMVis(std::shared_ptr<VMVisStorage> stor) : storage(stor) {}
	VMVis() : storage(nullptr) {}
	friend bool IsNull(VMVis inst) { return inst.storage == nullptr; }
	private: VMVisStorage* get() { return static_cast<VMVisStorage*>(storage.get()); }

	private: Int32 _screenWidth() { return get()->_screenWidth; }
	private: void set__screenWidth(Int32 _v) { get()->_screenWidth = _v; }
	private: Int32 _screenHeight() { return get()->_screenHeight; }
	private: void set__screenHeight(Int32 _v) { get()->_screenHeight = _v; }
	private: Byte _displayPool() { return get()->_displayPool; }
	private: void set__displayPool(Byte _v) { get()->_displayPool = _v; }
	private: Byte _savedPool() { return get()->_savedPool; }
	private: void set__savedPool(Byte _v) { get()->_savedPool = _v; }
}; // end of struct VMVis


// INLINE METHODS














