// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: App.cs

#pragma once
#include "core_includes.h"


namespace MiniScriptApp {

// FORWARD DECLARATIONS

struct CallInfo;
class CallInfoStorage;
struct VMVis;
class VMVisStorage;
struct Assembler;
class AssemblerStorage;
struct FuncDef;
class FuncDefStorage;
struct App;
class AppStorage;
struct Lexer;
class LexerStorage;

// DECLARATIONS













	public: static bool debugMode;
	public: static bool visMode;
	


class AppStorage : public std::enable_shared_from_this<AppStorage> {
	public: static void Main(string[] args);
}; // end of class AppStorage

struct App {
	protected: std::shared_ptr<AppStorage> storage;
  public:
	App(std::shared_ptr<AppStorage> stor) : storage(stor) {}
	App() : storage(nullptr) {}
	friend bool IsNull(App inst) { return inst.storage == nullptr; }
	private: AppStorage* get() { return static_cast<AppStorage*>(storage.get()); }

}; // end of struct App


// INLINE METHODS

} // end of namespace MiniScriptApp
