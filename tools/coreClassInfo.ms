// This module defines symbol table info for the core C# classes
// which are not transpiled (because they're standard library
// classes in C#).

import "importUtil"
ensureImport "symbolInfo"


String = ClassInfo.Make("String")
String.fields.push VariableInfo.Make("Length", "UInt32")
String.methods.push MethodInfo.Make("Substring")
String.methods.push MethodInfo.Make("Join", true)
// ToDo: other String fields and methods!

List = ClassInfo.Make("List")
List.fields.push VariableInfo.Make("Count", "UInt32")
// ToDo: other List fields and methods!

all = [String, List]
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

