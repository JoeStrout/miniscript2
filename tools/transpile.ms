// This program transpiles (a limited subset of) C# code into C++.

import "listUtil"
import "stringUtil"
import "fancyMatch"  // overrides standard string.match and string.fill

TAB = char(9)

debugPrint = function(s)
	//print s
end function

Context = {}
Context.START = "START"  // before or after namespace, file scope
Context.NAMESPACE = "NAMESPACE"  // within a namespace, but not a class
Context.CLASS = "CLASS"  // within a class, but not within a method
Context.METHOD = "METHOD" // within a method

Converter = {}
Converter.context = Context.START
Converter.codeBlockDepth = 0  // code blocks *within* a method
Converter.hLines = null
Converter.cppLines = null
Converter.addCurly = false
Converter.className = ""
Converter.classNames = null // all class names we know about
Converter.didIncludes = false
Converter.failed = false
Converter.lineNum = 0
Converter.line = ""
Converter.addCurly = false
Converter.csOnly = false  // when true, skip conversion entirely
Converter.hOnly = false   // when true, output only to hLines
Converter.cppOnly = false // when true, output only to cppLines
Converter.inVmCase = 0    // when > 1, codeBlockDepth of the current VM_CASE block

Converter.Make = function
	noob = new self
	noob.classNames = ["IOHelper", "String", "Math", "List<string>"]
	return noob
end function

Converter.match = function(inp, hOut, cppOut)
	self.m = self.line.match(inp)
	if not self.m then return false
	if not self.m.hasIndex("className") then self.m.className = self.className
	if self.m.hasIndex("params") then
		self.m.paramNamesOnly = self.removeDefaultValues(self.m.params)
	end if
	debugPrint "[" + self.context + "] --> " + inp
	curly = " {" * self.addCurly
	if cppOut then
		self.cppLines.push self.indentation + cppOut.fill(self.m) + curly + self.comment
	end if
	if hOut then
		if hOut[-1] == ";" then curly = "" // header file may have ; instead of {
		self.hLines.push self.indentation + hOut.fill(self.m) + curly + self.comment
	end if
	return true
end function

Converter.fail = function(errMsg)
	print "ERROR: " + errMsg
	print "on line " + self.lineNum + ": " + self.line + " {" * self.addCurly
	self.failed = true
end function

Converter.processLine = function(line)
	// Start by splitting off the comment, if any
	// (ToDo: lex this properly in a string-literal-savvy way)
	parts = line.split("//", 2)
	if parts.len == 2 then
		line = parts[0]
		comment = "//" + parts[1]
	else
		comment = ""
	end if
	
	// Also split off leading whitespace (indentation)
	indentation = ""
	while line and (line[0] == " " or line[0] == TAB)
		indentation += line[0]
		line = line[1:]
	end while

	// If our comment starts with "// CPP:", then ignore the rest of the line
	// and substitute what appears after that in the CPP code.
	if comment.startsWith("// CPP:") then
		line = comment[7:].trimLeft
		if self.context == Context.METHOD then
			self.cppLines.push indentation + line
			if line and line[-1] == "{" then self.codeBlockDepth += 1
		else
			self.hLines.push indentation + line
		end if
		return
	end if
	
	// Check for other special control comments
	if comment == "//*** BEGIN CS_ONLY ***" then
		self.csOnly = true
	else if comment == "//*** END CS_ONLY ***" then
		self.csOnly = false
		return
	else if line == "/*** BEGIN H_ONLY ***" then
		debugPrint "H ONLY MODE!"
		self.hOnly = true
		return
	else if line == "*** END H_ONLY ***/" then
		debugPrint "H ONLY off"
		self.hOnly = false
		return
	else if line == "/*** BEGIN CPP_ONLY ***" then
		debugPrint "CPP ONLY MODE!"
		self.cppOnly = true
		return
	else if line == "*** END CPP_ONLY ***/" then
		debugPrint "CPP ONLY off"
		self.cppOnly = false
		return
	end if
	if self.csOnly then return
	if self.hOnly then
		self.hLines.push indentation + line + comment
		return
	else if self.cppOnly then
		self.cppLines.push indentation + line + comment
		return
	end if

	// If it's an empty line, or just a comment, then push and we're done
	if not line then
		if self.context != Context.METHOD then self.hLines.push indentation + comment
		self.cppLines.push indentation + comment
		return
	end if
	
	// Trim trailing whitespace
	while line and (line[-1] == " " or line[-1] == TAB)
		comment = line[-1] + comment
		line = line[:-1]
	end while
	
	debugPrint line
	
	// And split off an opening curly too
	if line[-1] == "{" then
		self.addCurly = true
		line = line[:-1]
		line = line.trimRight
		if self.context == Context.METHOD then self.codeBlockDepth += 1
		debugPrint "Context: " + self.context + " (code block depth " + self.codeBlockDepth + ")"
	else
		self.addCurly = false
	end if
	
	// Now, process the line
	self.line = line
	self.comment = comment
	self.indentation = indentation
	
	// Enforce coding standards
	self.enforceCodeStandards line + " {" * self.addCurly
	if self.failed then return
	
	// Standard syntax transformations
	self.line = self.tweakSyntax(line, self.context)

	if self.context == Context.METHOD then
		self.processMethodLine
	else
		self.processNonmethodLine
	end if
	
	// Closing curly: close the open code block or scope
	if self.line and self.line[0] == "}" then
		debugPrint "CLOSING CURLY!!"		
		fullLine = self.indentation + "}" + self.comment

		if self.context == Context.METHOD and self.codeBlockDepth > 0 then
			self.codeBlockDepth -= 1
			if self.codeBlockDepth < self.inVmCase then self.inVmCase = 0
		else if self.context == Context.METHOD then
			self.context = Context.CLASS
		else if self.context == Context.CLASS then
			// add ; after class declaration in .h file; omit from .cpp file
			self.hLines.push self.indentation + "}; // end of class " + self.className
			self.context = Context.NAMESPACE
		else
			self.hLines.push fullLine
			self.cppLines.push fullLine
			self.context = Context.START
		end if		
		debugPrint "Context down: " + self.context + " (code block depth " + self.codeBlockDepth + ")"
	end if

end function

// Process a line of code *not* found within a method.
Converter.processNonmethodLine = function
	
	// using: ignored, but our chance to output standard includes
	if self.match("using ≤lib≥;", "", "") then
		if not self.didIncludes then
			self.hLines.push "#include ""core_includes.h"""
			self.cppLines.push "#include """ + (self.filename - ".cs" + ".g.h") + """"
			if self.filename == "Program.cs" then  // ToDo: find better way to decide this
				self.cppLines.push "#include ""IOHelper.g.h"""
			end if
			self.didIncludes = true
		end if
		return
	end if
	  
	// namespace line
	if self.match("namespace ≤name≥", "namespace ≤name≥", "namespace ≤name≥") then
		self.context = Context.NAMESPACE
		debugPrint "Context: " + self.context + " (code block depth " + self.codeBlockDepth + ")"
		return
	end if

	// class/struct declaration
	if self.match("≤qualifiers≥ class ≤name:w≥", "class ≤name≥", "") or
	  self.match("class ≤name≥", "class ≤name≥", "") then
		self.context = Context.CLASS
		debugPrint "Context: " + self.context + " (code block depth " + self.codeBlockDepth + ")"
		self.className = self.m.name
		if not self.classNames.contains(self.className) then
			self.classNames.push self.className
		end if
		debugPrint "Got class name: " + self.className + "; all classes: " + self.classNames
		return
	end if

	if self.match("≤qualifiers≥ struct ≤name:w≥", "struct ≤name≥", "") or
	  self.match("struct ≤name≥", "struct ≤name≥", "") then
		self.context = Context.CLASS
		debugPrint "Context: " + self.context + " (code block depth " + self.codeBlockDepth + ")"
		self.className = self.m.name
		if not self.classNames.contains(self.className) then
			self.classNames.push self.className
		end if
		debugPrint "Got class name: " + self.className + "; all classes: " + self.classNames
		return
	end if
	
	// enum declaration (becomes a class in C++)
	if self.match("≤scope≥ enum ≤name≥ : ≤type≥", "enum class ≤name≥ : ≤type≥", "") then
		if not self.checkType then return
		self.context = Context.CLASS
		debugPrint "Context: " + self.context + " (code block depth " + self.codeBlockDepth + ")"
		self.className = self.m.name
		if not self.classNames.contains(self.className) then
			self.classNames.push self.className
		end if
		debugPrint "Got class (enum) name: " + self.className + "; all classes: " + self.classNames
		return
	end if
	
	// enum value
	if self.match("≤name:w≥ = ≤value≥,", "≤name≥ = ≤value≥,", "") or
	   self.match("≤name:w≥ = ≤value≥",  "≤name≥ = ≤value≥", "") or
	   self.match("≤name:w≥,", "≤name≥,", "") or
	   self.match("≤name:w≥",  "≤name≥", "") then
		return
	end if
	
	// class field, with and without default value
	// (note: default values, i.e. i-class member initializers, are valid since C++11)
	if self.context == Context.CLASS and self.match("≤scope:w≥ ≤type:w≥ ≤name:w≥;",
	  "≤scope≥: ≤type≥ ≤name≥;", "") then return self.checkType
	if self.context == Context.CLASS and self.match("≤scope:w≥ ≤type:w≥ ≤name:w≥ = ≤value≥;",
	  "≤scope≥: ≤type≥ ≤name≥ = ≤value≥;", "") then return self.checkType

	// static class field, with and without default value
	if self.context == Context.CLASS and self.match("≤scope:w≥ static ≤type:w≥ ≤name:w≥;",
	  "≤scope≥: static ≤type≥ ≤name≥;", "") then return self.checkType
	if self.context == Context.CLASS and self.match("≤scope:w≥ static ≤type:w≥ ≤name:w≥ = ≤value≥;",
	  "≤scope≥: static ≤type≥ ≤name≥;", "≤type≥ ≤className≥::≤name≥ = ≤value≥;") then return self.checkType

	// (static) const class field with default value
	if self.context == Context.CLASS and 
	  self.match("≤scope:w≥ const ≤type:w≥ ≤name:w≥ = ≤value≥;",
	  "≤scope≥: static const ≤type≥ ≤name≥;",
	  "const ≤type≥ ≤className≥::≤name≥ = ≤value≥;") then return self.checkType

	// static readonly class field with default (usually non-constant) value
	if self.context == Context.CLASS and 
	  self.match("≤scope:w≥ static readonly ≤type:w≥ ≤name:w≥ = ≤value≥;",
	  "≤scope≥: static const ≤type≥ ≤name≥;",
	  "const ≤type≥ ≤className≥::≤name≥ = ≤value≥;") then return self.checkType

	// class property with expression body
	if self.context == Context.CLASS and self.match("≤scope:w≥ ≤type:w≥ ≤name:w≥ => ≤expr≥;",
	  "≤scope≥: ≤type≥ ≤name≥() { return ≤expr≥; }", "") then
		self.checkType
		// ToDo: note ≤name≥ in this class as something that will need () where used.
		return
	end if
	
	// public-get, private-set properties: we turn these into ordinary fields,
	// which loses the set protection, but it's fine
	if self.context == Context.CLASS and self.match("≤scope:w≥ ≤type:w≥ ≤name:w≥ { get; private set; }",
	  "≤scope≥: ≤type≥ ≤name≥;", "") then
		self.checkType
		return
	end if
	
	// expression-bodied method
	if self.match("≤scope:w≥ static ≤rtype:w≥ ≤name:w≥(≤params≥) => ≤expr≥;",
	  "≤scope≥: static ≤rtype≥ ≤name≥(≤params≥) { return ≤expr≥; }", "") then
		return
	end if

	if self.match("≤scope:w≥ ≤rtype:w≥ ≤name:w≥(≤params≥) => ≤expr≥;",
	  "≤scope≥: ≤rtype≥ ≤name≥(≤params≥) { return ≤expr≥; }", "") then
		return
	end if

	// constructor declaration
	if self.match("≤scope:w≥ ≤name:w≥(≤params≥)",
	  "≤scope≥: ≤name≥(≤params≥);",
	  "≤className≥::≤name≥(≤paramNamesOnly≥)") then
		self.context = Context.METHOD
		debugPrint "Context: " + self.context + " (code block depth " + self.codeBlockDepth + ")"
		return
	end if
	
	// conversion operator declaration
	if self.match("≤scope≥ implicit operator ≤name≥()",
	 "≤scope≥: operator ≤name≥();", "≤className≥::operator ≤name≥()") then
		self.context = Context.METHOD
		debugPrint "Context: " + self.context + " (code block depth " + self.codeBlockDepth + ")"
		return
	end if
	 

	// method declaration
	if self.match("≤scope:w≥ static ≤rtype:w≥ ≤name:w≥(≤params≥)",
	  "≤scope≥: static ≤rtype≥ ≤name≥(≤params≥);",
	  "≤rtype≥ ≤className≥::≤name≥(≤paramNamesOnly≥)") then
		self.context = Context.METHOD
		debugPrint "Context: " + self.context + " (code block depth " + self.codeBlockDepth + ")"
		return
	end if

	if self.match("≤scope:w≥ ≤rtype:w≥ ≤name:w≥(≤params≥)",
	  "≤scope≥: ≤rtype≥ ≤name≥(≤params≥);",
	  "≤rtype≥ ≤className≥::≤name≥(≤paramNamesOnly≥)") then
	  	if self.m.scope == "static" then return self.fail("scope qualifier required on static method")
		self.context = Context.METHOD
		debugPrint "Context: " + self.context + " (code block depth " + self.codeBlockDepth + ")"
		return
	end if
	
	// override method declaration
	if self.match("≤scope:w≥ override ≤rtype:w≥ ≤name:w≥(≤params≥)",
	  "≤scope≥: ≤rtype≥ ≤name≥(≤params≥);",
	  "≤rtype≥ ≤className≥::≤name≥(≤paramNamesOnly≥)") then
	  	if self.m.scope == "static" then return self.fail("scope qualifier required on static method")
		self.context = Context.METHOD
		debugPrint "Context: " + self.context + " (code block depth " + self.codeBlockDepth + ")"
		return
	end if
end function

// Process a line of code within a method.
Converter.processMethodLine = function
	// foreach statement
	if self.match("foreach (≤type:w≥ ≤var:w≥ in ≤seq≥)", "",
	  "for (int __≤var≥_idx=0, __≤var≥_qty=≤seq≥.Count(); __≤var≥_idx < __≤var≥_qty; __≤var≥_idx++)") then
		self.cppLines.push self.indentation + TAB + 
		  "≤type≥ ≤var≥ = ≤seq≥[__≤var≥_idx];".fill(self.m)
		return self.checkType
	end if
	
	if self.className == "VM" then
		// Special case: VM dispatch case
		if self.match("case Opcode::≤opcode≥:") or self.match("case Opcode.≤opcode≥:") then
			self.cppLines.push self.indentation + "VM_CASE(≤opcode≥)".fill(self.m) + " {" * self.addCurly;
			self.inVmCase = self.codeBlockDepth
			return
		end if
		
		if self.line == "break;" and self.inVmCase then
			self.cppLines.push self.indentation + "VM_NEXT();"
			return
		end if
	end if
	
	// Anything else: copy to CPP file
	curly = " {" * self.addCurly
	fullLine = self.indentation + self.line + curly + self.comment
	self.cppLines.push fullLine

end function

// Converter.tweakSyntax: do simple syntax transformations that can be done
// within pretty much any line of code.
Converter.tweakSyntax = function(line, context)
	// initialized char array declaration
	if self.match("char[] ≤name≥ = new char[≤len≥];") then
		self.m.lenPlus1 = str(self.m.len.val + 1)
		return "char ≤name≥[≤lenPlus1≥]; ≤name≥[≤len≥] = 0;".fill(self.m)
	end if
	
	// other local array declaration
	m = line.match("≤type:w≥[] ≤name≥ = new ≤type2:w≥[≤count≥];")
	if m then return "≤type≥ ≤name≥[≤count≥];".fill(m)	  

	// array syntax (especially as seen in parameters)
	line = line.replace("[] ", " *")
	
	// special case of a Main program args: add the count
	line = line.replace("string *args", "String args[], int argCount")
	
	// static class methods (invoked on the class name itself)
	for cls in self.classNames
		line = line.replace(cls+".", cls+"::")
	end for
	
	// Split to list
	m = line.match("≤pre≥.Split(≤args≥).ToList()≤post≥")
	if m then line = "≤pre≥.SplitToList(≤args≥)≤post≥".fill(m)
	
	// Object creation (assuming a compatible memory-managed type).
	line = line.replace(" new ", " ")
	line = line.replace("(new ", "(")
	if line.startsWith("new ") then line = line[4:]
	
	// null --> nullptr
	line = line.replace(" null", " nullptr")
	line = line.replace("(null", "(nullptr")
	
	// Count and Length -- properties in C#, but methods in C++.
	// We may need to handle this more generally.  But for now:
	line = line.replace(".Count", ".Count()")
	line = line.replace(".Length", ".Length()")
		
	// Integer min/max value: just remove the dot, creating constants
	// defined in core_includes.h.
	line = line.replace(".MinValue", "MinValue")
	line = line.replace(".MaxValue", "MaxValue")
	
	// Output parameters on method calls
	line = line.replace("(out ", "(&")
	line = line.replace(", out ", ", &")
	
	// Reference types, using an alias like FuncDefRef to mean FuncDef&
	if line.contains("Ref ") then
		words = line.split
		for i in words.indexes
			if words[i].endsWith("Ref") then words[i] = words[i][:-3] + "&"
		end for
		line = words.join
	end if
	
	// Reference parameters or arguments
	if context == Context.CLASS then
		// ref Type foo  -->  Type& foo
		lastPos = -1
		while true
			pos = line.indexOf("(ref ", lastPos)
			if pos == null then pos = line.indexOf(" ref ", lastPos);
			if pos == null then break
			//if lastPos < 0 then print "BEFORE: " + line
			spacePos = line.indexOf(" ", pos + 5)
			if spacePos == null then break
			line = line[:pos+1] + line[pos+5:spacePos] + "& " + line[spacePos+1:]
			//print "AFTER: " + line
			lastPos = pos
		end while
	else if context == Context.METHOD then
		// Within a method, just delete " ref "
		line = line.replace("(ref ", "(")
		line = line.replace(" ref ", " ")
	end if

	return line
end function

Converter.removeDefaultValues = function(paramStr)
	params = paramStr.split(",")
	for i in params.indexes
		eqPos = params[i].indexOf("=")
		if eqPos != null then params[i] = params[i][:eqPos]
	end for
	return params.join(",")
end function

// Converter.enforceCodeStandards: examine the given line for things that a
// C# programmer might do, that we can't convert or just don't want to allow.
Converter.enforceCodeStandards = function(line)
	if line == " {" then
		self.fail "Opening brace on its own line is disallowed"
	else if line.endsWith("=>") or line.startsWith("=>") then
		self.fail "Expression-bodied properties or methods must be all on one line"
	end if
end function

disallowedTypes = {
	"sbyte": "SByte",
	"byte": "Byte",
	"short": "Int16",
	"ushort": "UInt16",
	"int": "Int32",
	"uint": "UInt32",
	"long": "Int64",
	"ulong": "UInt64" }

// Converter.checkType: ensure that the given type is something we can deal
// with, and throw and error if it is disallowed.  Return true if OK, 
// false if failed.
Converter.checkType = function(type)
	if type == null then type = self.m["type"]
	if not disallowedTypes.hasIndex(type) then return true
	self.fail "Type " + type + " disallowed; use " + disallowedTypes[type] + " instead"
	return false
end function

WARNING = [
	"// AUTO-GENERATED FILE.  DO NOT MODIFY.",
	"// Transpiled from: ",
	"",
]

Converter.convertClass = function(sourcePath)
	fileNameBase = file.name(sourcePath) - ".cs"
	
	guardSymbol = ("__" + file.name(sourcePath) - ".cs" + "_H").upper
	warning = WARNING[:]
	warning[1] += file.name(sourcePath)
	self.hLines = warning + ["#ifndef " + guardSymbol, "#define " + guardSymbol, ""]
	self.cppLines = warning
	self.didIncludes = false
	self.filename = file.name(sourcePath)
	self.lineNum = 0
	self.failed = false
	self.csOnly = false
	self.hOnly = false
	self.cppOnly = false

	for line in file.readLines(sourcePath)
		self.lineNum += 1
		//debugPrint line
		self.processLine line
		if self.failed then return
	end for

	if self.hLines.len <= 6 and self.cppLines.len <= 3 then
		print fileNameBase + ".cs: skipped (C# only)"
		return
	end if
	totalLines = self.hLines.len + self.cppLines.len
	print "≤fileNameBase≥.cs --> ≤fileNameBase≥.g.h/.cpp (≤totalLines≥ lines)".fill(locals)

	self.hLines.push ""
	self.hLines.push "#endif // " + guardSymbol

	outFolder = "generated/"
	debugPrint "-----------------------------------"
	debugPrint "HEADER:"
	debugPrint self.hLines.join(char(10))
	file.writeLines outFolder + fileNameBase + ".g.h", self.hLines
	debugPrint
	debugPrint "-----------------------------------"
	debugPrint "CPP:"
	debugPrint self.cppLines.join(char(10))
	file.writeLines outFolder + fileNameBase + ".g.cpp", self.cppLines
	debugPrint
end function

Converter.findClassNames = function(sourcePath)
	for line in file.readLines(sourcePath)
		m = line.match("≤scope≥ class ≤class:w≥ {≤≥")
		if not m then m = line.match("≤scope≥ enum ≤class:w≥ : ≤type:w≥ {≤≥")
		if m then
			print "Found class: " + m.class + " in: " + sourcePath
			self.classNames.push m.class
		end if
	end for
end function

if locals == globals then
	cvt = Converter.Make

	for path in shellArgs[1:]
		cvt.findClassNames path
	end for

	for path in shellArgs[1:]
		cvt.convertClass path
		if cvt.failed then exit -1
	end for
end if

