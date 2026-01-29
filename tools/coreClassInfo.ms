// This module defines symbol table info for the core C# classes
// which are not transpiled (because they're standard library
// classes in C#).

import "importUtil"
ensureImport "symbolInfo"


String = ClassInfo.Make("String")
String.fields.push VariableInfo.Make("Length", "UInt32")
String.methods.push MethodInfo.Make("[]", false, "char")
String.methods.push MethodInfo.Make("Substring", false, "String")
String.methods.push MethodInfo.Make("Left", false, "String")
String.methods.push MethodInfo.Make("Right", false, "String")
String.methods.push MethodInfo.Make("Contains", false, "Boolean")
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
Dictionary.methods.push MethodInfo.Make("Add")
Dictionary.methods.push MethodInfo.Make("[]", false, "<T1, T2>")
Dictionary.methods.push MethodInfo.Make("ContainsKey", false, "Boolean")
Dictionary.methods.push MethodInfo.Make("TryGetValue", false, "Boolean")
Dictionary.methods.push MethodInfo.Make("Remove", false, "Boolean")
Dictionary.methods.push MethodInfo.Make("Clear")
Dictionary.methods.push MethodInfo.Make("GetKeys", false, "Keys")

all = [String, List, Dictionary]
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

