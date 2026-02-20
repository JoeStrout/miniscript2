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

struct CodeGenerator;
class CodeGeneratorStorage;
struct VM;
class VMStorage;
struct CodeEmitterBase;
class CodeEmitterBaseStorage;
struct BytecodeEmitter;
class BytecodeEmitterStorage;
struct AssemblyEmitter;
class AssemblyEmitterStorage;
struct Assembler;
class AssemblerStorage;
struct Parselet;
class ParseletStorage;
struct PrefixParselet;
class PrefixParseletStorage;
struct InfixParselet;
class InfixParseletStorage;
struct NumberParselet;
class NumberParseletStorage;
struct SelfParselet;
class SelfParseletStorage;
struct SuperParselet;
class SuperParseletStorage;
struct StringParselet;
class StringParseletStorage;
struct IdentifierParselet;
class IdentifierParseletStorage;
struct UnaryOpParselet;
class UnaryOpParseletStorage;
struct GroupParselet;
class GroupParseletStorage;
struct ListParselet;
class ListParseletStorage;
struct MapParselet;
class MapParseletStorage;
struct BinaryOpParselet;
class BinaryOpParseletStorage;
struct CallParselet;
class CallParseletStorage;
struct IndexParselet;
class IndexParseletStorage;
struct MemberParselet;
class MemberParseletStorage;
struct Intrinsic;
class IntrinsicStorage;
struct Parser;
class ParserStorage;
struct FuncDef;
class FuncDefStorage;
struct ASTNode;
class ASTNodeStorage;
struct NumberNode;
class NumberNodeStorage;
struct StringNode;
class StringNodeStorage;
struct IdentifierNode;
class IdentifierNodeStorage;
struct AssignmentNode;
class AssignmentNodeStorage;
struct IndexedAssignmentNode;
class IndexedAssignmentNodeStorage;
struct UnaryOpNode;
class UnaryOpNodeStorage;
struct BinaryOpNode;
class BinaryOpNodeStorage;
struct CallNode;
class CallNodeStorage;
struct GroupNode;
class GroupNodeStorage;
struct ListNode;
class ListNodeStorage;
struct MapNode;
class MapNodeStorage;
struct IndexNode;
class IndexNodeStorage;
struct SliceNode;
class SliceNodeStorage;
struct MemberNode;
class MemberNodeStorage;
struct MethodCallNode;
class MethodCallNodeStorage;
struct ExprCallNode;
class ExprCallNodeStorage;
struct WhileNode;
class WhileNodeStorage;
struct IfNode;
class IfNodeStorage;
struct ForNode;
class ForNodeStorage;
struct BreakNode;
class BreakNodeStorage;
struct ContinueNode;
class ContinueNodeStorage;
struct FunctionNode;
class FunctionNodeStorage;
struct SelfNode;
class SelfNodeStorage;
struct SuperNode;
class SuperNodeStorage;
struct ReturnNode;
class ReturnNodeStorage;

// DECLARATIONS

class AssemblerStorage : public std::enable_shared_from_this<AssemblerStorage> {
	friend struct Assembler;

	public: AssemblerStorage();
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
	friend class AssemblerStorage;
	protected: std::shared_ptr<AssemblerStorage> storage;
  public:
	Assembler(std::shared_ptr<AssemblerStorage> stor) : storage(stor) {}
	Assembler() : storage(nullptr) {}
	Assembler(std::nullptr_t) : storage(nullptr) {}
	friend bool IsNull(const Assembler& inst) { return inst.storage == nullptr; }
	private: AssemblerStorage* get() const;

	public: static Assembler New() {
		return Assembler(std::make_shared<AssemblerStorage>());
	}
	public: List<FuncDef> Functions(); // all functions
	public: void set_Functions(List<FuncDef> _v); // all functions
	public: FuncDef Current(); // function we are currently building
	public: void set_Current(FuncDef _v); // function we are currently building
	private: List<String> _labelNames(); // label names within current function
	private: void set__labelNames(List<String> _v); // label names within current function
	private: List<Int32> _labelAddresses(); // corresponding instruction addresses within current function
	private: void set__labelAddresses(List<Int32> _v); // corresponding instruction addresses within current function
	public: Boolean HasError();
	public: void set_HasError(Boolean _v);
	public: String ErrorMessage();
	public: void set_ErrorMessage(String _v);
	public: Int32 CurrentLineNumber();
	public: void set_CurrentLineNumber(Int32 _v);
	public: String CurrentLine();
	public: void set_CurrentLine(String _v);

	// Multiple functions support
	
	// Error handling state

	// Helper to find a function by name (returns -1 if not found)
	public: inline Int32 FindFunctionIndex(String name);

	// Helper to find a function by name.
	// C#: returns null if not found; C++: returns an empty FuncDef.
	public: inline FuncDef FindFunction(String name);

	// Helper to check if a function exists
	public: inline Boolean HasFunction(String name);

	public: static List<String> GetTokens(String line) { return AssemblerStorage::GetTokens(line); }
	
	private: inline void SetCurrentLine(Int32 lineNumber, String line);
	
	public: inline void ClearError();
	
	private: inline void Error(String errMsg);
	
	// Assemble a single source line, add to our current function,
	// and also return its value (mainly for unit testing).
	public: inline UInt32 AddLine(String line);
	
	public: inline UInt32 AddLine(String line, Int32 lineNumber);
	
	// Helper to parse register like "r5" -> 5
	private: inline Byte ParseRegister(String reg);

	// Helper to parse a Byte number (handles negative numbers)
	private: inline Byte ParseByte(String num);
	
	// Helper to parse an Int16 number (handles negative numbers)
	private: inline Int16 ParseInt16(String num);

	// Helper to parse a 24-bit int number (handles negative numbers)
	private: inline Int32 ParseInt24(String num);

	// Helper to parse a 32-bit int number (handles negative numbers)
	private: inline Int32 ParseInt32(String num);

	// Helper to determine if a token is a label (ends with colon)
	private: static Boolean IsLabel(String token) { return AssemblerStorage::IsLabel(token); }

	// Helper to determine if a token is a function label (starts with @ and ends with colon)
	private: static Boolean IsFunctionLabel(String token) { return AssemblerStorage::IsFunctionLabel(token); }
	
	// Extract just the label part, e.g. "someLabel:" --> "someLabel"
	private: static String ParseLabel(String token) { return AssemblerStorage::ParseLabel(token); }

	// Add a new empty function to our function list.
	// Return true on success, false if failed (because function already exists).
	private: inline bool AddFunction(String functionName);

	// Helper to find the address of a label
	private: inline Int32 FindLabelAddress(String labelName);

	// Helper to add a constant to the constants table and return its index
	private: inline Int32 AddConstant(Value value);

	// Helper to check if a token is a string literal (surrounded by quotes)
	private: static Boolean IsStringLiteral(String token) { return AssemblerStorage::IsStringLiteral(token); }

	// Helper to check if a token needs to be stored as a constant
	// (string literals, floating point numbers, or integers too large for Int16)
	private: static Boolean NeedsConstant(String token) { return AssemblerStorage::NeedsConstant(token); }

	// Helper to create a Value from a token
	private: inline Value ParseAsConstant(String token);

	// Helper to parse a double from a string (basic implementation)
	private: inline Double ParseDouble(String str);

	// Multi-function assembly with support for @function: labels
	public: inline void Assemble(List<String> sourceLines);

	// Assemble a single function from sourceLines, starting at startLine,
	// and proceeding until we hit another function label or run out of lines.
	// Return the line number after this function ends.
	private: inline Int32 AssembleFunction(List<String> sourceLines, Int32 startLine);
}; // end of struct Assembler

// INLINE METHODS

inline AssemblerStorage* Assembler::get() const { return static_cast<AssemblerStorage*>(storage.get()); }
inline List<FuncDef> Assembler::Functions() { return get()->Functions; } // all functions
inline void Assembler::set_Functions(List<FuncDef> _v) { get()->Functions = _v; } // all functions
inline FuncDef Assembler::Current() { return get()->Current; } // function we are currently building
inline void Assembler::set_Current(FuncDef _v) { get()->Current = _v; } // function we are currently building
inline List<String> Assembler::_labelNames() { return get()->_labelNames; } // label names within current function
inline void Assembler::set__labelNames(List<String> _v) { get()->_labelNames = _v; } // label names within current function
inline List<Int32> Assembler::_labelAddresses() { return get()->_labelAddresses; } // corresponding instruction addresses within current function
inline void Assembler::set__labelAddresses(List<Int32> _v) { get()->_labelAddresses = _v; } // corresponding instruction addresses within current function
inline Boolean Assembler::HasError() { return get()->HasError; }
inline void Assembler::set_HasError(Boolean _v) { get()->HasError = _v; }
inline String Assembler::ErrorMessage() { return get()->ErrorMessage; }
inline void Assembler::set_ErrorMessage(String _v) { get()->ErrorMessage = _v; }
inline Int32 Assembler::CurrentLineNumber() { return get()->CurrentLineNumber; }
inline void Assembler::set_CurrentLineNumber(Int32 _v) { get()->CurrentLineNumber = _v; }
inline String Assembler::CurrentLine() { return get()->CurrentLine; }
inline void Assembler::set_CurrentLine(String _v) { get()->CurrentLine = _v; }
inline Int32 Assembler::FindFunctionIndex(String name) { return get()->FindFunctionIndex(name); }
inline FuncDef Assembler::FindFunction(String name) { return get()->FindFunction(name); }
inline Boolean Assembler::HasFunction(String name) { return get()->HasFunction(name); }
inline void Assembler::SetCurrentLine(Int32 lineNumber,String line) { return get()->SetCurrentLine(lineNumber, line); }
inline void Assembler::ClearError() { return get()->ClearError(); }
inline void Assembler::Error(String errMsg) { return get()->Error(errMsg); }
inline UInt32 Assembler::AddLine(String line) { return get()->AddLine(line); }
inline UInt32 Assembler::AddLine(String line,Int32 lineNumber) { return get()->AddLine(line, lineNumber); }
inline Byte Assembler::ParseRegister(String reg) { return get()->ParseRegister(reg); }
inline Byte Assembler::ParseByte(String num) { return get()->ParseByte(num); }
inline Int16 Assembler::ParseInt16(String num) { return get()->ParseInt16(num); }
inline Int32 Assembler::ParseInt24(String num) { return get()->ParseInt24(num); }
inline Int32 Assembler::ParseInt32(String num) { return get()->ParseInt32(num); }
inline bool Assembler::AddFunction(String functionName) { return get()->AddFunction(functionName); }
inline Int32 Assembler::FindLabelAddress(String labelName) { return get()->FindLabelAddress(labelName); }
inline Int32 Assembler::AddConstant(Value value) { return get()->AddConstant(value); }
inline Value Assembler::ParseAsConstant(String token) { return get()->ParseAsConstant(token); }
inline Double Assembler::ParseDouble(String str) { return get()->ParseDouble(str); }
inline void Assembler::Assemble(List<String> sourceLines) { return get()->Assemble(sourceLines); }
inline Int32 Assembler::AssembleFunction(List<String> sourceLines,Int32 startLine) { return get()->AssembleFunction(sourceLines, startLine); }

} // end of namespace MiniScript
