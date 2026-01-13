// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: App.cs

#pragma once
#include "core_includes.h"


namespace MiniScriptApp {

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














	public: static bool debugMode;
	public: static bool visMode;

class AppStorage : public std::enable_shared_from_this<AppStorage> {
	friend struct App;
	
	public: static void Main(string[] args);
}; // end of class AppStorage

struct App {
	protected: std::shared_ptr<AppStorage> storage;
  public:
	App(std::shared_ptr<AppStorage> stor) : storage(stor) {}
	App() : storage(nullptr) {}
	static App New() { return App(std::make_shared<AppStorage>()); }
	friend bool IsNull(const App& inst) { return inst.storage == nullptr; }
	private: AppStorage* get() const { return static_cast<AppStorage*>(storage.get()); }

	
	public: static void Main(string[] args) { return AppStorage::Main(args); }
}; // end of struct App


// INLINE METHODS

} // end of namespace MiniScriptApp
