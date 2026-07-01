// This module defines symbol table info for the core C# classes
// which are not transpiled (because they're standard library
// classes in C#).

import "importUtil"
ensureImport "symbolInfo"


String = ClassInfo.Make("String")
String.fields.push VariableInfo.Make("Length", "UInt32")
String.methods.push MethodInfo.Make("[]", false, "Char")
String.methods.push MethodInfo.Make("Split", false, "String[]")
String.methods.push MethodInfo.Make("Substring", false, "String")
String.methods.push MethodInfo.Make("Left", false, "String")
String.methods.push MethodInfo.Make("Right", false, "String")
String.methods.push MethodInfo.Make("PadLeft", false, "String")
String.methods.push MethodInfo.Make("PadRight", false, "String")
String.methods.push MethodInfo.Make("Remove", false, "String")
String.methods.push MethodInfo.Make("Replace", false, "String")
String.methods.push MethodInfo.Make("ReplaceLineEndings", false, "String")
String.methods.push MethodInfo.Make("ToLower", false, "String")
String.methods.push MethodInfo.Make("ToLowerInvariant", false, "String")
String.methods.push MethodInfo.Make("ToUpper", false, "String")
String.methods.push MethodInfo.Make("ToUpperInvariant", false, "String")
String.methods.push MethodInfo.Make("Trim", false, "String")
String.methods.push MethodInfo.Make("TrimEnd", false, "String")
String.methods.push MethodInfo.Make("TrimStart", false, "String")
String.methods.push MethodInfo.Make("Contains", false, "Boolean")
String.methods.push MethodInfo.Make("EndsWith", false, "Boolean")
String.methods.push MethodInfo.Make("StartsWith", false, "Boolean")
String.methods.push MethodInfo.Make("IndexOf", false, "Integer")
String.methods.push MethodInfo.Make("IndexOfAny", false, "Integer")
String.methods.push MethodInfo.Make("LastIndexOf", false, "Integer")
String.methods.push MethodInfo.Make("LastIndexOfAny", false, "Integer")
String.methods.push MethodInfo.Make("IsNullOrEmpty", true, "Boolean")
String.methods.push MethodInfo.Make("IsNullOrWhitespace", true, "Boolean")
String.methods.push MethodInfo.Make("Join", true, "String")
// ToDo: other String fields and methods!

List = ClassInfo.Make("List")
List.fields.push VariableInfo.Make("Count", "UInt32")
List.methods.push MethodInfo.Make("[]", false, "<T>")
List.methods.push MethodInfo.Make("Add")
List.methods.push MethodInfo.Make("Clear")
List.methods.push MethodInfo.Make("IndexOf", false, "UInt32")
List.methods.push MethodInfo.Make("LastIndexOf", false, "UInt32")
List.methods.push MethodInfo.Make("Insert")
List.methods.push MethodInfo.Make("Remove")
List.methods.push MethodInfo.Make("RemoveAt")
List.methods.push MethodInfo.Make("Reverse")
List.methods.push MethodInfo.Make("Sort")

Dictionary = ClassInfo.Make("Dictionary")
Dictionary.fields.push VariableInfo.Make("Count", "UInt32")
Dictionary.fields.push VariableInfo.Make("Keys", "KeyCollection")
Dictionary.fields.push VariableInfo.Make("Values", "ValueCollection")
Dictionary.methods.push MethodInfo.Make("Add")
Dictionary.methods.push MethodInfo.Make("[]", false, "<T1, T2>")
Dictionary.methods.push MethodInfo.Make("ContainsKey", false, "Boolean")
Dictionary.methods.push MethodInfo.Make("TryGetValue", false, "Boolean")
Dictionary.methods.push MethodInfo.Make("Remove", false, "Boolean")
Dictionary.methods.push MethodInfo.Make("Clear")

Value = ClassInfo.Make("Value")
Value.type = ClassType.STRUCT
Value.methods.push MethodInfo.Make("IsNull", false, "Boolean")
Value.methods.push MethodInfo.Make("IsNumber", false, "Boolean")
Value.methods.push MethodInfo.Make("IsString", false, "Boolean")
Value.methods.push MethodInfo.Make("IsList", false, "Boolean")
Value.methods.push MethodInfo.Make("IsMap", false, "Boolean")
Value.methods.push MethodInfo.Make("IsError", false, "Boolean")
Value.methods.push MethodInfo.Make("IsFuncRef", false, "Boolean")
Value.methods.push MethodInfo.Make("IsHandle", false, "Boolean")
Value.methods.push MethodInfo.Make("DoubleValue", false, "Double")
Value.methods.push MethodInfo.Make("FloatValue", false, "Float")
Value.methods.push MethodInfo.Make("IntValue", false, "Int")
Value.methods.push MethodInfo.Make("UIntValue", false, "UInt")
Value.methods.push MethodInfo.Make("BoolValue", false, "Boolean")
Value.methods.push MethodInfo.Make("Hash", false, "UInt")
Value.methods.push MethodInfo.Make("Bits", false, "UInt64")
Value.methods.push MethodInfo.Make("GCSetIndex", false, "Int")
Value.methods.push MethodInfo.Make("ItemIndex", false, "Int")
Value.methods.push MethodInfo.Make("TinyLen", false, "Int")
Value.methods.push MethodInfo.Make("TinyCopyTo")
Value.methods.push MethodInfo.Make("IsTinyString", false, "Boolean")
Value.methods.push MethodInfo.Make("IsGCObject", false, "Boolean")
Value.methods.push MethodInfo.Make("IsHeapString", false, "Boolean")
Value.methods.push MethodInfo.Make("IsInternedString", false, "Boolean")
Value.methods.push MethodInfo.Make("IsInt", false, "Boolean")
Value.methods.push MethodInfo.Make("AsInt", false, "Int")
Value.methods.push MethodInfo.Make("AsDouble", false, "Double")
Value.methods.push MethodInfo.Make("NumericVal", false, "Double")
Value.methods.push MethodInfo.Make("TypeName", false, "String")

all = [String, List, Dictionary, Value]
for cls in all; cls.elsewhere = true; end for

if locals == globals then
	for cls in all
		print "Class: " + cls.name
		if cls.fields then
			print "  Fields:"
			for fld in cls.fields
				print "    • " + fld.summary
			end for
		end if
		if cls.methods then
			print "  Methods:"
			for meth in cls.methods
				print "    • " + meth.summary
			end for
		end if
		print
	end for
end if

