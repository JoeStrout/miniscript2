// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: Assembler.cs

#pragma once
#include "core_includes.h"
#include "value.h"
#include "value_string.h"
#include "Bytecode.g.h"
#include "FuncDef.g.h"


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















class AssemblerStorage : public std::enable_shared_from_this<AssemblerStorage> {
	friend struct Assembler;
	public: List<FuncDef> Functions = List<FuncDef>::New(); // all functions
	public: FuncDef Current = FuncDef::New(); // function we are currently building
	private: List<String> _labelNames = List<String>::New(); // label names within current function
	private: List<Int32> _labelAddresses = List<Int32>::New(); // corresponding instruction addresses within current function
	public: Boolean HasError;
	public: String ErrorMessage;
	public: Int32 CurrentLineNumber;
	public: String CurrentLine;

	// Multiple functions support
	
	// Error handling state

	// Helper to find a function by name (returns -1 if not found)
	public: Int32 FindFunctionIndex(String name);

	// Helper to find a function by name.
	// C#: returns null if not found; C++: returns an empty FuncDef.
	public: FuncDef FindFunction(String name);

	// Helper to check if a function exists
	public: Boolean HasFunction(String name);

	public: static List<String> GetTokens(String line);
	
	private: void SetCurrentLine(Int32 lineNumber, String line);
	
	public: void ClearError();
	
	private: void Error(String errMsg);
	
	// Assemble a single source line, add to our current function,
	// and also return its value (mainly for unit testing).
	public: UInt32 AddLine(String line);
	
	public: UInt32 AddLine(String line, Int32 lineNumber);
	
	// Helper to parse register like "r5" -> 5
	private: Byte ParseRegister(String reg);

	// Helper to parse a Byte number (handles negative numbers)
	private: Byte ParseByte(String num);
	
	// Helper to parse an Int16 number (handles negative numbers)
	private: Int16 ParseInt16(String num);

	// Helper to parse a 24-bit int number (handles negative numbers)
	private: Int32 ParseInt24(String num);

	// Helper to parse a 32-bit int number (handles negative numbers)
	private: Int32 ParseInt32(String num);

	// Helper to determine if a token is a label (ends with colon)
	private: static Boolean IsLabel(String token);

	// Helper to determine if a token is a function label (starts with @ and ends with colon)
	private: static Boolean IsFunctionLabel(String token);
	
	// Extract just the label part, e.g. "someLabel:" --> "someLabel"
	private: static String ParseLabel(String token);

	// Add a new empty function to our function list.
	// Return true on success, false if failed (because function already exists).
	private: bool AddFunction(String functionName);

	// Helper to find the address of a label
	private: Int32 FindLabelAddress(String labelName);

	// Helper to add a constant to the constants table and return its index
	private: Int32 AddConstant(Value value);

	// Helper to check if a token is a string literal (surrounded by quotes)
	private: static Boolean IsStringLiteral(String token);

	// Helper to check if a token needs to be stored as a constant
	// (string literals, floating point numbers, or integers too large for Int16)
	private: static Boolean NeedsConstant(String token);

	// Helper to create a Value from a token
	private: Value ParseAsConstant(String token);

	// Helper to parse a double from a string (basic implementation)
	private: Double ParseDouble(String str);

	// Multi-function assembly with support for @function: labels
	public: void Assemble(List<String> sourceLines);

	// Assemble a single function from sourceLines, starting at startLine,
	// and proceeding until we hit another function label or run out of lines.
	// Return the line number after this function ends.
	private: Int32 AssembleFunction(List<String> sourceLines, Int32 startLine);


}; // end of class AssemblerStorage

struct Assembler {
	protected: std::shared_ptr<AssemblerStorage> storage;
  public:
	Assembler(std::shared_ptr<AssemblerStorage> stor) : storage(stor) {}
	Assembler() : storage(nullptr) {}
	static Assembler New() { return Assembler(std::make_shared<AssemblerStorage>()); }
	friend bool IsNull(const Assembler& inst) { return inst.storage == nullptr; }
	private: AssemblerStorage* get() const { return static_cast<AssemblerStorage*>(storage.get()); }

	public: List<FuncDef> Functions() { return get()->Functions; } // all functions
	public: void set_Functions(List<FuncDef> _v) { get()->Functions = _v; } // all functions
	public: FuncDef Current() { return get()->Current; } // function we are currently building
	public: void set_Current(FuncDef _v) { get()->Current = _v; } // function we are currently building
	private: List<String> _labelNames() { return get()->_labelNames; } // label names within current function
	private: void set__labelNames(List<String> _v) { get()->_labelNames = _v; } // label names within current function
	private: List<Int32> _labelAddresses() { return get()->_labelAddresses; } // corresponding instruction addresses within current function
	private: void set__labelAddresses(List<Int32> _v) { get()->_labelAddresses = _v; } // corresponding instruction addresses within current function
	public: Boolean HasError() { return get()->HasError; }
	public: void set_HasError(Boolean _v) { get()->HasError = _v; }
	public: String ErrorMessage() { return get()->ErrorMessage; }
	public: void set_ErrorMessage(String _v) { get()->ErrorMessage = _v; }
	public: Int32 CurrentLineNumber() { return get()->CurrentLineNumber; }
	public: void set_CurrentLineNumber(Int32 _v) { get()->CurrentLineNumber = _v; }
	public: String CurrentLine() { return get()->CurrentLine; }
	public: void set_CurrentLine(String _v) { get()->CurrentLine = _v; }

	// Multiple functions support
	
	// Error handling state

	// Helper to find a function by name (returns -1 if not found)
	public: Int32 FindFunctionIndex(String name) { return get()->FindFunctionIndex(name); }

	// Helper to find a function by name.
	// C#: returns null if not found; C++: returns an empty FuncDef.
	public: FuncDef FindFunction(String name) { return get()->FindFunction(name); }

	// Helper to check if a function exists
	public: Boolean HasFunction(String name) { return get()->HasFunction(name); }

	public: static List<String> GetTokens(String line) { return AssemblerStorage::GetTokens(line); }
	
	private: void SetCurrentLine(Int32 lineNumber, String line) { return get()->SetCurrentLine(lineNumber, line); }
	
	public: void ClearError() { return get()->ClearError(); }
	
	private: void Error(String errMsg) { return get()->Error(errMsg); }
	
	// Assemble a single source line, add to our current function,
	// and also return its value (mainly for unit testing).
	public: UInt32 AddLine(String line) { return get()->AddLine(line); }
	
	public: UInt32 AddLine(String line, Int32 lineNumber) { return get()->AddLine(line, lineNumber); }
	
	// Helper to parse register like "r5" -> 5
	private: Byte ParseRegister(String reg) { return get()->ParseRegister(reg); }

	// Helper to parse a Byte number (handles negative numbers)
	private: Byte ParseByte(String num) { return get()->ParseByte(num); }
	
	// Helper to parse an Int16 number (handles negative numbers)
	private: Int16 ParseInt16(String num) { return get()->ParseInt16(num); }

	// Helper to parse a 24-bit int number (handles negative numbers)
	private: Int32 ParseInt24(String num) { return get()->ParseInt24(num); }

	// Helper to parse a 32-bit int number (handles negative numbers)
	private: Int32 ParseInt32(String num) { return get()->ParseInt32(num); }

	// Helper to determine if a token is a label (ends with colon)
	private: static Boolean IsLabel(String token) { return AssemblerStorage::IsLabel(token); }

	// Helper to determine if a token is a function label (starts with @ and ends with colon)
	private: static Boolean IsFunctionLabel(String token) { return AssemblerStorage::IsFunctionLabel(token); }
	
	// Extract just the label part, e.g. "someLabel:" --> "someLabel"
	private: static String ParseLabel(String token) { return AssemblerStorage::ParseLabel(token); }

	// Add a new empty function to our function list.
	// Return true on success, false if failed (because function already exists).
	private: bool AddFunction(String functionName) { return get()->AddFunction(functionName); }

	// Helper to find the address of a label
	private: Int32 FindLabelAddress(String labelName) { return get()->FindLabelAddress(labelName); }

	// Helper to add a constant to the constants table and return its index
	private: Int32 AddConstant(Value value) { return get()->AddConstant(value); }

	// Helper to check if a token is a string literal (surrounded by quotes)
	private: static Boolean IsStringLiteral(String token) { return AssemblerStorage::IsStringLiteral(token); }

	// Helper to check if a token needs to be stored as a constant
	// (string literals, floating point numbers, or integers too large for Int16)
	private: static Boolean NeedsConstant(String token) { return AssemblerStorage::NeedsConstant(token); }

	// Helper to create a Value from a token
	private: Value ParseAsConstant(String token) { return get()->ParseAsConstant(token); }

	// Helper to parse a double from a string (basic implementation)
	private: Double ParseDouble(String str) { return get()->ParseDouble(str); }

	// Multi-function assembly with support for @function: labels
	public: void Assemble(List<String> sourceLines) { return get()->Assemble(sourceLines); }

	// Assemble a single function from sourceLines, starting at startLine,
	// and proceeding until we hit another function label or run out of lines.
	// Return the line number after this function ends.
	private: Int32 AssembleFunction(List<String> sourceLines, Int32 startLine) { return get()->AssembleFunction(sourceLines, startLine); }
}; // end of struct Assembler


// INLINE METHODS

} // end of namespace MiniScript
