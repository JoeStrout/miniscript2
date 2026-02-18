// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: StringUtils.cs

#pragma once
#include "core_includes.h"
// String utilities: conversion between ints and Strings, etc.

#include "CS_String.h"
#include "IOHelper.g.h"
#include "value.h"
#include "value_string.h"
#include "value_list.h"
#include "value_map.h"
#include <sstream>
#include <cctype>

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






class StringUtils {
	public: static const String hexDigits;
	
	public: static String ToHex(UInt32 value);
	
	public: static String ToHex(Byte value);

	public: static Int32 ParseInt32(String str);

	public: static Double ParseDouble(String str);

	public: static String ZeroPad(Int32 value, Int32 digits = 5);
	
	public: static String Spaces(Int32 count);

	public: static String SpacePad(String text, Int32 width);

	public: static String Str(List<String> list);

	public: static String Str(Char c);
	//--- The following is all to support a Format function, equivalent to
	//--- the one in C#.  The C++ one requires templates and helper functions.
	public: static String FormatList(const String& fmt, const List<String>& values);

	// --- stringify helpers ---
	public:
	inline static String makeString(uint8_t, const String& s) { 
		return s;
	}
	inline static String makeString(const Char* s) {
		return String(s ? s : "");
	}
	inline static String makeString(Char c) {
		Char buf[2] = {c, '\0'};
		return String(buf);
	}
	// Value type support
	inline static String makeString(Value v) {
		if (is_null(v)) return String("null");
		if (is_int(v)) return makeString(as_int(v));
		if (is_double(v)) {
			// Use the proper MiniScript number formatting
			Value strVal = to_string(v);
			return String(as_cstring(strVal));
		}
		if (is_string(v)) {
			const Char* str = as_cstring(v);
			return str ? String(str) : String("<str?>");
		}
		if (is_list(v)) {
			std::ostringstream oss;
			oss << "[";
			// ToDo: watch out for recursion, or maybe just limit our depth in
			// general.  I think MS1.0 limits nesting to 16 levels deep.  But
			// whatever we do, we shouldn't just crash with a stack overflow.
			for (int i = 0; i < list_count(v); i++) {
				oss << (i != 0 ? ", " : "") << makeRepr(list_get(v, i)).c_str();
			}
			oss << "]";
			return String(oss.str().c_str());
		}
		if (is_map(v)) {
			std::ostringstream oss;
			oss << "{";
			// ToDo: watch out for recursion, or maybe just limit our depth in
			// general.  I think MS1.0 limits nesting to 16 levels deep.  But
			// whatever we do, we shouldn't just crash with a stack overflow.
			MapIterator iter = map_iterator(v); 
			Value key, value;
			bool first = true;
			while (map_iterator_next(&iter, &key, &value)) {
				if (first) first = false; else oss << ", ";
				oss << makeRepr(key).c_str() << ": "
					<< makeRepr(value).c_str();
			}
			oss << "}";
			return String(oss.str().c_str());
		}
		if (is_funcref(v)) {
			std::ostringstream oss;
			oss << "FuncRef(" << funcref_index(v) << ")";
			return String(oss.str().c_str());
		}
		std::ostringstream oss;
		oss << "<value:0x" << std::hex << v << ">";
		return String(oss.str().c_str());
	}
	inline static String makeRepr(const Value v) {
		if (is_string(v)) {
			const Char* str = as_cstring(v);
			if (!str) {
				return String("\"\"");
			}
			String s = String(str);
			// Replace quotes: " becomes ""
			String escaped = s.Replace(String("\""), String("\"\""));
			// Wrap in quotes
			return String("\"") + escaped + String("\"");
		}
		return makeString(v);
	}

	// Generic fallback for numbers and streamable types.
	template <typename T>
	inline static String makeString(const T& v) {
		std::ostringstream oss; oss << v;
		const std::string tmp = oss.str();
		return String(tmp.c_str());
	}
	
	template <typename... Args>
	inline static void addValues(List<String>& list, Args&&... args) {
		String tmp[] = { makeString(std::forward<Args>(args))... };
		for (const auto& s : tmp) list.Add(s);
	}
	
	public: 
	template <typename... Args>
	inline static String Format(const String& fmt, Args&&... args) {
		List<String> vals;
		addValues(vals, std::forward<Args>(args)...);
		return StringUtils::FormatList(fmt, vals);
	}

	// Convert a Value to its representation (quoted string for strings, plain for others)
	// This is used when displaying values in source code form.
}; // end of struct StringUtils


































































// INLINE METHODS

inline Int32 StringUtils::ParseInt32(String str) {
	return std::stoi(str.c_str());
}
inline Double StringUtils::ParseDouble(String str) {
	return std::stod(str.c_str());
}
inline String StringUtils::Str(Char c) {
	return String(c);
}

} // end of namespace MiniScript
