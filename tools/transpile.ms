// C# -> C++ Transpiler for MiniScript 2.0
//
// Usage:
//
//	miniscript transpile.ms -d -c $CONTEXT_PATH -o $OUTPUT_PATH input1 input2 ...
//
// Options:
//	-c path: path to a "context" directory; we will scan all .cs files in this
//		directory for context, but not actually transpile them.
//	-o path: path to output directory, where generated .h/.cpp files should go.
//	input1 etc.: paths to input directories OR files to transpile; when given
//		a directory, we transpile all .cs files therein.
// -d: "debug" mode; run unit tests, and generate verbose debug output.
//
// All options are optional.  Default path for -c and -o is the first input 
// directory (or directory of the first input file).  Debug mode is on by
// default if given only one input file; otherwise it is off.
// 
// Example: from the project root, transpile just FuncDef.cs, with context:
//	miniscript tools/transpile.ms -c cs -o generated cs/FuncDef.cs 

import "importUtil"
import "stringUtil"
import "listUtil"
import "mapUtil"
import "fancyMatch"
import "qa"
import "symbolInfo"
import "coreClassInfo"

//--------------------------------------------------------------------------------
// Constants
TAB = char(9)

State = {}						// used for Transpiler.state
State.START = "START"			// at start of file, before namespace
State.NAMESPACE = "NAMESPACE"	// within the namespace, but not in a class/struct/etc.
State.CLASS = "CLASS"			// within a class definition
State.METHOD = "METHOD"			// within a function/method definition
State.LAMBDA = "LAMBDA"			// within a lambda (inline function) definition
State.DONE = "DONE"				// past the end of the namespace (pretty much done)
State.CPP_ONLY = "CPPONLY"		// writing embedded C++ code to .cpp file only
State.H_ONLY = "HONLY"			// writing embedded C++ code to .h file only
State.CS_ONLY = "CSONLY"		// skipping C#-only code

//--------------------------------------------------------------------------------
// Global data & methods
debugMode = false

debugPrint = function(s)
	if debugMode then print s
end function

list.find = function(maps, key, valueToFind)
	// Search the given list of maps for one with [key] = valueToFind.
	// Return the item, or null if not found.
	for m in maps
		if m.get(@key) == @valueToFind then return m
	end for
	return null
end function

string.splitQuoted = function(delimiter=" ", maxCount=-1, openQuote="""", closeQuote=null)
	if not self then return [""]
	if closeQuote == null then closeQuote = openQuote
	result = []
	start = 0
	inQuote = false
	delimiterLen = delimiter.len
	i = 0
	while true
		if inQuote then
			// while in a quote, ignore anything but a close quote
			if self[i] == closeQuote then
				inQuote = false
				if not delimiter then
					result.push self[start:i+1]
					start = i+1
				end if
			end if
		else
			// while not in a quote, watch for open quotes or delimiters
			if self[i] == openQuote then
				inQuote = true
			else if not delimiter then
				result.push self[i]
			else if self[i:i+delimiterLen] == delimiter then
				result.push self[start:i]
				i += delimiterLen - 1
				start = i + 1
			end if
		end if
		i += 1
		if i >= self.len or (maxCount > 0 and result.len + 1 == maxCount) then
			result.push self[start:]
			return result
		end if
	end while
end function
				

fixStringInterpolation = function(s)
	// Look for C#-style string interpolation, and replace it with a
	// call to our Interp function.  Example:
	//		$"{greeting}, {name}!"  -->  Interp("{}, {}!", greeting, name)
	pos0 = -1
	while true
		pos0 = s.indexOf("$""", pos0)
		if pos0 == null then return s
		pos1 = s.indexOf("""", pos0+1)
		while pos1 and s[pos1-1] == "\"
			pos1 = s.indexOf("""", pos1)
		end while
		content = s[pos0+2:pos1]  // string content, not including quotes
		//print "content: " + content
		args = [content]
		i = -1
		while true
			i = content.indexOf("{", i)
			if i == null then break
			i2 = content.indexOf("}", i)
			args.push content[i+1:i2]
			content = content[:i+1] + content[i2:]
		end while
		args[0] = """" + content + """"
		revisedContent = "Interp(" + args.join(", ") + ")"
		//print "revisedContent: " + revisedContent
		s = s[:pos0] + revisedContent + s[pos1+1:]
	end while
	return s
end function

isIdentifierChar = function(c, isFirstChar)
	return ("a" <= c <= "z") or ("A" <= c <= "Z") or c == "_" or
	  (not isFirstChar and ("0" <= c <= "9"))
end function

_keywords = null
isKeyword = function(word)
	if _keywords == null then
		_keywords = {}
		for kw in split(
					"abstract as base bool break byte case catch char checked" +
					"class const continue decimal default delegate do double else" +
					"enum event explicit extern false finally fixed float for" +
					"foreach goto if implicit in int interface internal is lock" +
					"long namespace new null object operator out override params" +
					"private protected public readonly ref return sbyte sealed" +
					"short sizeof stackalloc static string struct switch this" +
					"throw true try typeof uint ulong unchecked unsafe ushort" +
					"using virtual void volatile while")
			_keywords.push kw
		end for
	end if
	return _keywords.hasIndex(word)
end function

splitParams = function(s)
	// Split the given parameter string into a list of parameters.
	// Basically split on comma, except for commas inside quotes or < >.
	result = []
	depth = 0		// angle bracket nesting depth
	inQuote = false
	start = 0
	i = 0
	while i < s.len
		c = s[i]
		if inQuote then
			if c == """" then
				// check for doubled quote (escaped quote)
				if i + 1 < s.len and s[i+1] == """" then
					i = i + 1  // skip the second quote
				else
					inQuote = false
				end if
			end if
		else if c == """" then
			inQuote = true
		else if c == "<" then
			depth = depth + 1
		else if c == ">" then
			depth = depth - 1
		else if c == "," and depth == 0 then
			result.push s[start:i].trim
			start = i + 1
		end if
		i = i + 1
	end while
	// push the last parameter
	tail = s[start:].trim
	if tail then result.push tail
	return result
end function

roughTokenize = function(s)
	// Roughly tokenize the given string, which is mostly about separating
	// out the identifiers.  Anything that's not an identifier can be lumped
	// together with whitespace and neighboring operators, etc.  Example:
	//	  "if foo.bar == 40+2 {" 
	// -->  ["if", " ", "foo", ".", "bar", " == 40+2 {"]
	result = []
	tokStart = 0
	inIdentifier = false
	inString = false
	backslashEscape = false
	i = -1
	while i+1 < s.len
		i += 1
		c = s[i]
		if backslashEscape then
			backslashEscape = false
			continue
		end if
		if c == "\" then
			backslashEscape = true
			continue
		end if
		if c == """" then inString = not inString
		if inString then continue
		if c == "(" or c == ")" then
			if i > tokStart then result.push s[tokStart:i]
			result.push c
			tokStart = i+1
			inIdentifier = false
		else if c == "." and i > 0 and "0123456789'".indexOf(s[i-1]) == null then
			if i > tokStart then result.push s[tokStart:i]
			result.push c
			tokStart = i+1
			inIdentifier = false
		else if inIdentifier then
			if isIdentifierChar(c, false) then continue
			if c == "<" then
				// templated type, e.g. Dictionary<TokenType, InfixParselet>
				// ....keep that whole thing together as one token.
				gtPos = s.indexOf(">", i)
				if gtPos != null then
					i = gtPos
					continue
				end if
			end if
			if i > tokStart then result.push s[tokStart:i]
			tokStart = i
			inIdentifier = false
		else if c == "/" and i > 0 and s[i-1] == "/" then
			// comment -- treat whole thing as one token
			break
		else
			if not isIdentifierChar(c, true) then continue
			if i > tokStart then result.push s[tokStart:i]
			tokStart = i
			inIdentifier = true
		end if
	end while
	result.push s[tokStart:]
	return result
end function

parameterNames = function(s)
	// Strip types and default values from the given parameter declaration.
	if not s then return s
	result = []
	unnamed = 0
	for param in splitParams(s)
		param = param.split("=", 2)[0]
		name = param.split(" ")[-1]
		result.push name
	end for
	return result.join(", ")
end function

//--------------------------------------------------------------------------------
Line = {}
Line.full = ""					// full text of line
Line.m = null					// match map (set by .match method)

Line.indentation = function
	line = self.full
	i = 0
	while i < line.len and (line[i] == " " or line[i] == TAB)
		i += 1
	end while
	self.indentation = line[:i]  // memoize on the instance
	return self.indentation
end function

Line.comment = function
	line = self.full
	i = 0
	comment = ""
	inQuote = false
	while i < line.len-2
		if line[i] == """" then inQuote = not inQuote
		if line[i:i+2] == "//" and not inQuote then
			comment = line[i:]
			break
		end if
		i += 1
	end while
	self.comment = comment  // memoize on the instance
	return comment
end function

Line.trimmed = function
	comment = self.comment
	if comment.len > 0 then
		line = self.full[self.indentation.len : -comment.len]
	else
		line = self.full[self.indentation.len:]
	end if
	line = line.trimRight
	self.trimmed = line  // again, memoize
	return line
end function

Line.match = function(pattern, againstFull=false)
	if againstFull then s = self.full else s = self.trimmed
	self.m = s.match(pattern)
	return self.m != null
end function

Line.Make = function(fullLine)
	noob = new Line
	noob.full = fullLine
	return noob
end function


//--------------------------------------------------------------------------------
Transpiler = {}
Transpiler.state = ""
Transpiler.gatherMode = false	// if true, just gathering symbols
Transpiler.sourcePath = ""		// file path to .cs file we're processing
Transpiler.outputFolder = ""	// path to folder for output files
Transpiler.namespace = ""		// namespace we're processing
Transpiler.preLines = null		// list of lines (usually comments) before method/field
Transpiler.line = null			// current line we're processing
Transpiler.allClasses = null	// list of ClassInfo we know about
Transpiler.curClass = null		// ClassInfo that's currently being processed
Transpiler.curMethod = null		// MethodInfo that's currently being processed
Transpiler.localVars = null		// current set of variables for method we're in
Transpiler.headerLines = null	// extra lines for top of header file
Transpiler.cppLines = null		// extra lines for top of .cpp file
Transpiler.endLines = null		// end of namespace, and anything that comes after
Transpiler.prevStates = []		// stack of previous states (see pushState/popState)
Transpiler.nextMethodInline = false  // if true, next method should be inlined
Transpiler.inVmCase = 1			// > 0 when in a VM dispatch case block

Transpiler.Make = function
	noob = new Transpiler
	noob.allClasses = coreClassInfo.all
	return noob
end function

Transpiler.sourceName = function; return file.name(self.sourcePath); end function

Transpiler.assert = function(condition, message)
	if condition then return
	print "Assert failed: " + message
	if self.line != null then print "on line: " + self.line.full
	exit
end function

// Transpiler.match: match the given pattern against our current line; if found,
// copy self.line.m to self.m (for convenience), and also fill in className and
// inline.
Transpiler.match = function(pattern)
	if self.line.match(pattern) then
		self.m = self.line.m
		if self.curClass then
			self.m.className = self.curClass.name
			if self.nextMethodInline then
				self.m.inline = "inline "
			else
				self.m.inline = ""
			end if
		end if
		return true
	end if
	self.line.m = null
	self.m = null
	return false
end function

// Transpiler.fill: fill the given pattern with values from self.m, and 
// optionally append the comment, if the current line has one.
Transpiler.fill = function(pattern, addComment=true)
	if pattern.contains("≤args≥") and not self.m.hasIndex("args") then
		self.m.args = parameterNames(self.m.params)
	end if
	if pattern.contains("≤params≥") and not self.m.hasIndex("fixedRefs") then
		// Add & to any interface types, e.g. IParser --> IParser&
		// and also fix output parameters, etc.
		s = self.transformParams(self.m.params)
		for cls in self.allClasses
			if cls.type == ClassType.INTERFACE then s = s.replace(cls.name, cls.name + "&")
		end for
		self.m.params = s
		self.m.fixedRefs = true
	end if
	result = pattern.fill(self.m)
	if addComment and self.line.comment then result += " " + self.line.comment
	return result
end function

Transpiler.findClass = function(name)
	for cl in self.allClasses
		if cl.name == name then return cl
		if name.contains("<") and name.split("<")[0] == cl.name then return cl
	end for
	return null
end function

Transpiler.findOrMakeClass = function(name)
	cl = self.findClass(name)
	if cl then return cl
	cl = ClassInfo.Make(name)
	self.allClasses.push cl
	return cl
end function

Transpiler.subclassesOf = function(class)
	if class isa ClassInfo then class = class.name
	result = []
	for cl in self.allClasses
		if cl.superclass == class then result.push cl
	end for
	return result
end function

Transpiler.hasValueLocals = function
	// Return whether any of our local variables are of type Value,
	// indicating a need to use GC_PUSH_SCOPE and GC_POP_SCOPE.
	// (OBSOLETE with new GC scheme)
//	for var in self.localVars
//		if var.type == "Value" then return true
//	end for
	return false
end function

Transpiler.lookup = function(identifier, context)
	// Return info on the given identifier.  If context is given, then look only
	// at types within that class; otherwise, start with locals and work up to
	// globals, considering current transpiler state.
	// Returns a VariableInfo, MethodInfo, or ClassInfo, or null if not recognized.

	// 0. Look in given context (only), if there is one
	if context isa ClassInfo then
		return context.lookup(identifier, self.allClasses)
	end if		
	
	// 1. locals, including curMethod parameters
	for var in self.localVars
		if var.name == identifier then return var
	end for
	
	// 2. fields and methods of the current class
	if self.curClass then
		if identifier == self.curClass.name then return self.curClass
		info = self.curClass.lookup(identifier, self.allClasses)
		if info then return info
	end if
	// 3. class names
	for cls in self.allClasses
		if cls.name == identifier then return cls
		bracketPos = identifier.indexOf("<")
		if bracketPos != null and identifier[:bracketPos] == cls.name then
			templateType = identifier[bracketPos+1 : identifier.indexOf(">")]
			return cls.specialize(templateType)
		end if
	end for
	// 4. give up :P
	return null
end function

Transpiler.transformStatement = function(trimmedLine)
	// Here's one of our hardest tasks: transforming a statement (trimmed
	// of indentation and comment) from C++ to C#, taking into account all
	// the symbols we know.  That means resolving identifiers to their 
	// type, so that we can add () after class field getters, and convert
	// assignment to class fields to a set_X() call.
	
	if not trimmedLine then return trimmedLine
	
	// Start with a rough tokenization, separating each identifier and
	// non-identifier string.
	tokens = roughTokenize(trimmedLine)
	
	deepDebug = trimmedLine.contains("kvp");
	ddprint = function(s); if deepDebug then print "====> " + s; end function
	summarize = function(obj)
		if obj == null then return "<null>"
		if not (obj isa map) or not obj.hasOrInherits("summary") then
			print "WTF?!?"
			print obj
			print stackTrace
			exit
		end if
		return obj.summary
	end function
	
	ddprint tokens
	
	// Then iterate and analyze each one.  We'll encounter things we don't 
	// recognize; that's OK.  Just keep track of stuff we do.
	didAssignment = false
	curInfo = null
	callTypeStack = []
	arrayType = null
	i = -1
	while i + 1 < tokens.len
		i += 1
		if not tokens[i] then continue
		if tokens[i] == "true" then
			tokens[i] = "Boolean(true)"  // make true/false literals Boolean rather than bool
		else if tokens[i] == "false" then
			tokens[i] = "Boolean(false)"
		else if tokens[i] == "out" or tokens[i] == "ref" then
			// This is used for out/ref arguments; in C++, make this into & to pass
			// the argument as a pointer (since it needs to work in C too, where
			// reference parameters are not supported).
			tokens[i] = "&"
			if i+1 < tokens.len and tokens[i+1] == " " then tokens.remove i+1
		else if tokens[i] == "." then   // dot operator: resolve var type and continue
			if i+1 < tokens.len and (tokens[i+1] == "MaxValue" or tokens[i+1] == "MinValue") then
				// We have macros like UInt16MaxValue, etc., in core_includes.h, so just:
				tokens[i-1] += tokens[i+1]
				tokens.remove i+1
				tokens.remove i
				i -= 1
				continue
			end if
			if i+1 < tokens.len and tokens[i+1] == "Invoke" then
				// invoking a function pointer in C++ is simpler, just call it
				tokens.remove i+1
				tokens.remove i
				i -=1
				continue
			end if
			ddprint "Got dot when curInfo=" + summarize(curInfo)
			if curInfo isa VariableInfo then
				type = curInfo.type
				curInfo = self.lookup(type - "Ref") // ToDo: context?
				if curInfo and type.endsWith("Ref") then
					curInfo = new curInfo
					curInfo.viaRef = true
					ddprint "Found ref type, bailing"
					continue
				end if
			end if
			ddprint "After lookup, curInfo=" + summarize(curInfo)
			if i and tokens[i-1] == "this" then
				curInfo = self.curClass
				tokens[i] = "->"
			end if
			// look up RHS type and see if it's static; if so, it needs "::" rather than "."
			rhsStatic = true
			if i and not isIdentifierChar(tokens[i-1][-1]) then
				// ...but if LHS token is not an identifier, then it *can't* be static
				ddprint "LHS token is " + tokens[i-1] + ", so we CAN'T use :: here"
				continue
			end if
			if curInfo and i+1 < tokens.len then
				rhsType = self.lookup(tokens[i+1], curInfo)
				ddprint "rhsType=" + summarize(rhsType)
				if rhsType then
					rhsStatic = rhsType.static
				else
					ddprint "Couldn't find " + tokens[i+1] + " in " + summarize(curInfo)
				end if
			end if
			if curInfo == null or rhsStatic then
				if curInfo == null then y = "curInfo == null" else y = "rhsStatic == true"
				ddprint "Because " + y + ", changing token " + i + " to ::"
				tokens[i] = "::"
			end if
			continue
		else if tokens[i] == "new" then
			// when `new` is followed by the name of a C# class (which we convert
			// using our storage-wrapper pattern), call the `New()` static method instead.
			cl = self.findClass(tokens[i+2])  // (skipping space)
			if cl and cl.type == ClassType.CLASS then
				tokens.remove i  // remove "new"; i now points to " "
				tok = tokens[i+1]  // should be something like `Foo` or `Dictionary<Foo, Bar>`
				tokens[i+1] = tok + "::New"
			else
				// if it's a struct OR an unknown type (which we assume is some other 
				// class, not one of our storage wrappers), then just remove `new`
				if cl == null then
					print "Unknown type " + tokens[i+2] + " found after `new`; assuming class/struct"
				end if
				tokens[i] = ""
				tokens[i+1] = ""
				continue
			end if
			// and, check for the initializer-list syntax, used with List for example:
			// new List<String> { "foo", "bar" } --> List<String>({ "foo", "bar" })
			if i+2 < tokens.len and tokens[i+2].startsWith(" {") then
				tokens[i+2] = "({" + tokens[i+2][2:]
				for j in range(i+2, tokens.len-1, 1)
					if tokens[j].endsWith("}") then
						tokens[j] = tokens[j][:-1] + "})"
						break
					else if tokens[j].endsWith("};") then
						tokens[j] = tokens[j][:-2] + "});"
						break
					end if
				end for
			end if
			continue
		else if tokens[i] == "this" and (i+1 >= tokens.len or tokens[i+1] != ".") then
			// Ensure we have a _this local variable, then reference that in place of `this`.
			if not self.curMethod.hasThisLocal then
				self.curClass.methodLines.insert self.curClass.methodStartLine + 1, 
				  "	≤name≥ _this(std::static_pointer_cast<≤name≥Storage>(shared_from_this()));".fill(self.curClass)
				self.curMethod.hasThisLocal = true
			end if
			tokens[i] = "_this";
			curInfo = self.curClass;
			continue
		else if tokens[i][0] == "(" then  // method call!  or maybe expression...
			if curInfo then
				callTypeStack.push self.lookup(curInfo.type)
				ddprint "METHOD CALL returns: " + summarize(callTypeStack[-1])
				curInfo = null
			else
				callTypeStack.push null
			end if
			continue
		else if tokens[i][0] == ")" and callTypeStack then
			ddprint "GOT `)` with callTypeStack: " + callTypeStack.applied(@summarize)
			curInfo = callTypeStack.pop
			continue
		else if tokens[i] == "[" and curInfo then   // array/list lookup
			self.assert not arrayType, "Nested array indexing not yet supported"
			// Get the type of the current reference
			arrayType = self.lookup(curInfo.type)
			continue
		else if tokens[i].startsWith("]") and arrayType then
			// Look up the `[]` (indexer) method in our array type
			self.assert arrayType, "Got [] without clear array type"
			curInfo = self.lookup("[]", arrayType)
			// Get the return type of that indexer method
			if curInfo then curInfo = self.lookup(curInfo.type)
			ddprint "resolved [] in " + summarize(arrayType) + " to: " + summarize(curInfo)
			arrayType = null
			continue
		else if tokens[i] == "null" then
			// How we translate null depends on the context...
			prevTok = tokens[i-1].trim
			if prevTok == "!=" or prevTok == "==" then
				// TODO: make helper method to find start of expression at token i-1.
				// For now we'll just do a crude hack:
				exprStart = i - 2
				while exprStart > 1 and tokens[exprStart-1] == "."
					exprStart -= 2
				end while
				tokens[exprStart] = "IsNull(" + tokens[exprStart]
				if prevTok == "!=" then tokens[exprStart] = "!" + tokens[exprStart]
				tokens[i-1] = ")"
				tokens[i] = ""
				curInfo = null
				continue
			else
				// Any other use, just change null to nullptr
				tokens[i] = "nullptr"
			end if
		end if
		nextInfo = self.lookup(tokens[i], curInfo)
		if self.curClass then curClassName = self.curClass.name else curClassName = ""
		ddprint "lookup of " + tokens[i] + ": " + summarize(nextInfo)
		if nextInfo isa VariableInfo and nextInfo.ref then
			// "reference" (pointer) type -- insert '*'
			tokens[i] = "*" + tokens[i]
			ddprint "Changed token to " + tokens[i]
		end if
		if curInfo isa ClassInfo and curInfo.type == ClassType.CLASS and
		  nextInfo isa VariableInfo then
			// Found a field lookup!  Or maybe an assignment.  Check the next token
			// to determine which.  And skip extra syntax when accessed via Ref.
			if curInfo.hasIndex("viaRef") then viaRef = curInfo.viaRef else viaRef = false
			if viaRef then ddprint "ACCESS VIA REF so skipping extra syntax"
			if i+1 < tokens.len and tokens[i+1].trimLeft.startsWith("= ") then
				ddprint "FIELD ASSIGNMENT (" + curInfo.name + "." + nextInfo.name + ") FOUND!!!"
				// Change this to a set_ call.
				if not viaRef then
					tokens[i] = "set_" + tokens[i] + "("
					tokens[i+1] = tokens[i+1][2:].trimLeft
					didAssignment = true
				end if
				nextInfo = null
			else
				ddprint "FIELD LOOKUP (" + curInfo.name + "." + nextInfo.name + ") FOUND!!!"
				// Just add () to call the getter.
				if not viaRef then tokens[i] += "()"
				nextInfo = self.lookup(nextInfo.type)
				ddprint "Field type (nextInfo): " + summarize(nextInfo)
			end if
		end if
		curInfo = nextInfo
	end while
	
	result = tokens.join("")
	if didAssignment then
		// insert closing ')' (for the set_X call) before the semicolon
		pos = result.lastIndexOf(";")
		if pos == null then pos = result.len
		result = result[:pos] + ")" + result[pos:]
	end if
	ddprint "Transformed statement: " + result
	return result
	
end function

Transpiler.transformParams = function(paramStr)
	// Fix up any language transformations needed for the given parameters string.
	result = []
	for part in splitParams(paramStr)
		m = part.match("out ≤type:w≥ ≤whatever≥")
		if not m then m = part.match("ref ≤type:w≥ ≤whatever≥")
		if m then
			// Turn these into pointer types, as it needs to be compatible with
			// C too (see map_try_get), which doesn't support reference parameters.
			result.push "≤type≥* ≤whatever≥".fill(m)
			continue
		else
			part = self.transformStatement(part)
		end if
		result.push part
	end for
	return result.join(", ")
end function

Transpiler.parseParams = function(paramStr)
	// Parse the given parameter string into a list of VariableInfo.
	result = []
	for part in splitParams(paramStr)
		m = part.match("≤type:w≥")
		if m then continue  // unnamed (because unused) parameter
		// standard parameters
		m = part.match("≤type:w≥ ≤name:w≥ = ≤value≥")
		if not m then m = part.match("≤type:w≥ ≤name:w≥=≤value≥")
		if not m then m = part.match("≤type:w≥ ≤name:w≥")
		if m then
			result.push VariableInfo.Make(m.name, m.type, Scope.LOCAL)
			continue
		end if
		// reference/output parameters
		m = part.match("ref ≤type:w≥ ≤name:w≥")
		if not m then m = part.match("out ≤type:w≥ ≤name:w≥")
		if m then
			result.push VariableInfo.Make(m.name, m.type, Scope.LOCAL)
			result[-1].ref = true
			continue
		end if
		// array parameters
		m = part.match("≤type:w≥[] ≤name:w≥")
		if not m then m = part.match("params ≤type:w≥[] ≤name:w≥")
		if m then
			result.push VariableInfo.Make(m.name, m.type, Scope.LOCAL)
			result[-1].array = true
		else
			self.assert false, "Could not parse parameter: " + part
		end if
	end for
	return result
end function

Transpiler.stripDefaultArgs = function
	// Strip default arguments from self.m.params (because we're about to use it
	// to generate a method body, and default arguments must be given only in
	// the method declaration).
	paramStr = self.m.params.trim
	if not paramStr then return
	params = splitParams(paramStr)
	for i in params.indexes
		eqPos = params[i].indexOf("=")
		if eqPos != null then params[i] = params[i][:eqPos]
	end for
	self.m.params = params.join(",")
end function

Transpiler.enterState = function(newState, initAsNew=true)
	if newState == self.state then return
	if self.state then self.exitState newState
	oldState = self.state
	self.state = newState
	debugPrint "Entering state: " + newState
	
	if newState == State.START then
		self.preLines = []
		self.line = ""
		self.headerLines = []
		self.cppLines = []
		self.endLines = []
		self.curClass = null
		self.curMethod = null
	
	else if newState == State.NAMESPACE then
		self.curClass = null
		
	else if newState == State.CLASS then
		if initAsNew then
			self.assert not self.line.indentation, "Classes should be flush left, not indented" 
			if self.line and self.line.m and self.line.m.hasIndex("name") then
				name = self.line.m.name
			else
				name = "?"
			end if
			self.curClass = self.findOrMakeClass(name)
			self.curClass.defLines.push ""
		end if
		self.nextMethodInline = false
		if self.m and self.m.hasIndex("name") then self.m.className = self.m.name

	else if newState == State.METHOD and oldState != State.LAMBDA then
		if self.curClass.type == ClassType.CLASS then
			self.curClass.storageLines += self.preLines
			self.curClass.wrapperLines += self.preLines
		else
			self.curClass.declLines += self.preLines
		end if
		self.preLines = []
		params = []
		if self.line and self.line.m and self.line.m.hasIndex("name") then
			name = self.line.m.name
			if self.line.m.hasIndex("params") then
				params = self.parseParams(self.line.m.params)
			end if
		else
			name = "?"
		end if
		self.curMethod = MethodInfo.Make(name)
		self.curMethod.params = params
		if self.nextMethodInline then
			self.curClass.methodLines = self.curClass.inlineLines
		else
			self.curClass.methodLines = self.curClass.defLines
		end if
		self.curClass.methodStartLine = self.curClass.methodLines.len
		self.localVars = params[:]
		self.inVmCase = 0
		self.pushedGCScope = false
	end if
	
end function

Transpiler.exitState = function(toNewState)
	debugPrint "Exiting state: " + self.state

	if self.state == State.METHOD then
		if toNewState != State.LAMBDA then self.localVars = []
		self.inVmCase = 0
		self.pushedGCScope = false
	end if
end function

Transpiler.pushState = function(newState)
	debugPrint "Storing " + self.state + " on stack"
	self.prevStates.push self.state
	self.state = ""
	self.enterState newState
end function

Transpiler.popState = function
	debugPrint "Popping state " + self.state + ", returning to " + self.prevStates[-1]
	self.exitState self.prevStates[-1]
	self.state = self.prevStates.pop
end function

Transpiler.updateState = function
	
	if self.state == State.START then
		self.updateStateSTART
	else if self.state == State.NAMESPACE then
		self.updateStateNAMESPACE
	else if self.state == State.CLASS then
		self.updateStateCLASS
	else if self.state == State.METHOD then
		self.updateStateMETHOD
	else if self.state == State.LAMBDA then
		// Inside a lambda is actually still just inside a method, except we watch
		// for this one exit condition
		if self.line.trimmed == "};" then
			self.curClass.methodLines.push "	});"
			self.enterState State.METHOD
		else
			self.updateStateMETHOD
		end if
	else if self.state == State.DONE then
		self.endLines.push self.line.full	// ToDo: support H: and CPP: comments here?
	else if self.state == State.CPP_ONLY then
		line = self.line.full
		if line and line[0] == TAB then line = line[1:] // (outdent 1 level)
		context = self.prevStates[-1]
		if context == State.METHOD then
			self.curClass.methodLines.push line
		else if context == State.CLASS then
			self.curClass.defLines.push line
		else
			self.cppLines.push line
		end if
	else if self.state == State.H_ONLY then
		context = self.prevStates[-1]
		if context == State.METHOD then
			line = self.line.full
			if line and line[0] == "	" then line = line[1:] // (outdent 1 level)
			self.curClass.methodLines.push line
		else if context == State.CLASS then
			if self.curClass.type == ClassType.CLASS then
				// This is a little tricky — which part of the header is this line meant for?
				// So far we have only needed it in the storage class, so for now we'll assume:
				self.curClass.storageLines.push self.line.full
			else
				self.curClass.declLines.push self.line.full
			end if
		else
			self.endLines.push self.line.full
		end if
	end if
	
end function

Transpiler.updateStateSTART = function
	line = self.line
	comment = self.line.comment

	if comment.startsWith("// CPP: ") then
		self.cppLines.push comment[8:]
	else if comment.startsWith("// H: ") then
		self.headerLines.push comment[6:]
	else if line.trimmed == "" then
		self.headerLines.push line.full
	else if self.match("namespace ≤name≥ {") then
		self.headerLines.push line.full
		self.namespace = line.m.name
		self.enterState State.NAMESPACE
	else if not line.trimmed.startsWith("using ") then
		print "Ignored in state START: " + line.full
	end if
end function

Transpiler.prepareRefClass = function
	// Enter the CLASS state based on match terms in self.m.  This handles
	// the various permutations: whether it has a base class (interface or
	// otherwise), and whether it has any subclasses.
	baseClass = ""
	baseIsInterface = false
	if self.m.hasIndex("baseClass") then
		baseClass = self.m.baseClass
		baseClassInfo = self.findClass(baseClass)
		if baseClassInfo then
			baseIsInterface = baseClassInfo.type == ClassType.INTERFACE
		else
			// If we can't find the base class -- perhaps because we haven't gotten
			// to it yet -- then assume an interface if its name is like IWhatever.
			baseIsInterface = (self.m.baseClass[0] == "I" and "A" <= self.m.baseClass[1] <= "Z")
		end if
	end if	
	
	self.enterState State.CLASS
	// add any pre-class comments
	self.curClass.wrapperLines += self.preLines; self.preLines = []
	
	// prepare the wrapper class
	if baseClass and not baseIsInterface then
		// With a (true) base class, we don't need to declare the storage pointer; and our
		// constructors all need to call through to the base class.  But that does mean
		// we have to defer fully defining the constructor until the inline section.
		self.curClass.superclass = self.m.baseClass
		self.curClass.wrapperLines.push self.fill("struct ≤name≥ : public ≤baseClass≥ {")
		self.curClass.wrapperLines.push self.fill("	friend class ≤name≥Storage;")
		self.curClass.wrapperLines.push self.fill("	≤name≥(std::shared_ptr<≤name≥Storage> stor);")
		self.curClass.inlineLines.push self.fill("inline ≤name≥::≤name≥(std::shared_ptr<≤name≥Storage> stor) : ≤baseClass≥(stor) {}")
		self.curClass.wrapperLines.push self.fill("	≤name≥() : ≤baseClass≥() {}")
		self.curClass.wrapperLines.push self.fill("	≤name≥(std::nullptr_t) : ≤baseClass≥(nullptr) {}")
	else
		// No base class (or just an interface): we need to declare the storage pointer and associated methods.
		if baseClass then  // (just an interface in this case)
			self.curClass.wrapperLines.push self.fill("struct ≤name≥ : public ≤baseClass≥ {")
		else
			self.curClass.wrapperLines.push self.fill("struct ≤name≥ {")
		end if
		self.curClass.wrapperLines.push self.fill("	friend class ≤name≥Storage;")
		self.curClass.wrapperLines.push self.fill("	protected: std::shared_ptr<≤name≥Storage> storage;")
		self.curClass.wrapperLines.push "  public:"
		self.curClass.wrapperLines.push self.fill("	≤name≥(std::shared_ptr<≤name≥Storage> stor) : storage(stor) {}")
		self.curClass.wrapperLines.push self.fill("	≤name≥() : storage(nullptr) {}")
		self.curClass.wrapperLines.push self.fill("	≤name≥(std::nullptr_t) : storage(nullptr) {}")
		self.curClass.wrapperLines.push self.fill("	friend bool IsNull(const ≤name≥& inst) { return inst.storage == nullptr; }")
	end if
	
	// every wrappr class needs a get() method, properly typed
	self.curClass.wrapperLines.push self.fill("	private: ≤name≥Storage* get() const;")
	self.curClass.inlineLines.push  self.fill("inline ≤name≥Storage* ≤className≥::get() const { return static_cast<≤name≥Storage*>(storage.get()); }")

	// prepare the storage class
	if baseIsInterface then
		self.curClass.storageLines.push self.fill("class ≤name≥Storage : public std::enable_shared_from_this<≤name≥Storage>, public ≤baseClass≥ {")
	else if baseClass then
		self.curClass.storageLines.push self.fill("class ≤name≥Storage : public ≤baseClass≥Storage {")
	else
		self.curClass.storageLines.push self.fill("class ≤name≥Storage : public std::enable_shared_from_this<≤name≥Storage> {")
	end if
	self.curClass.storageLines.push self.fill("	friend struct ≤name≥;")
	
	// if this is the base of the class hierarchy, *and* we have any subclasses,
	// then make an As method for downcasting to subclass types.
	if not baseClass and self.subclassesOf(self.curClass) then
		self.curClass.wrapperLines.push self.fill("	template<typename WrapperType, typename StorageType>")
		self.curClass.wrapperLines.push self.fill("	friend WrapperType As(≤name≥ inst) {")
		self.curClass.wrapperLines.push self.fill("		auto stor = std::dynamic_pointer_cast<StorageType>(inst.storage);")
		self.curClass.wrapperLines.push self.fill("		if (stor == nullptr) return WrapperType(nullptr);")
		self.curClass.wrapperLines.push self.fill("		return WrapperType(stor); ")
		self.curClass.wrapperLines.push self.fill("	}")
		// and in this case, we also need a virtual destructor
		self.curClass.storageLines.push self.fill("	public: virtual ~≤name≥Storage() {}")
	end if
	self.curClass.wrapperLines.push ""

end function

Transpiler.updateStateNAMESPACE = function
	line = self.line
	comment = self.line.comment

	if comment.startsWith("// CPP: ") then
		self.cppLines.push comment[8:]
	else if comment.startsWith("// H: ") then
		self.headerLines.push comment[6:]
	else if line.trimmed == "" then
		self.preLines.push line.full
	// static class
	else if self.match("≤scope:w≥ static class ≤name:w≥ {") then
		self.enterState State.CLASS
		self.curClass.type = ClassType.STRUCT
		self.curClass.static = true
		self.curClass.declLines += self.preLines; self.preLines = []
		self.curClass.declLines.push self.fill("class ≤name≥ {")
	// struct
	else if self.match("≤scope:w≥ struct ≤name:w≥ {") or
			self.match("≤scope:w≥ struct ≤name:w≥ : ≤baseClass≥ {")then
		self.enterState State.CLASS
		self.curClass.type = ClassType.STRUCT
		if self.m.hasIndex("baseClass") then self.curClass.superclass = self.m.baseClass
		self.curClass.declLines += self.preLines; self.preLines = []
		self.curClass.declLines.push self.fill("struct ≤name≥ {")
		
	// class (reference type -- becomes a storage class + wrapper)
	else if self.match("≤scope:w≥ class ≤name:w≥ {") or
	        self.match("≤scope:w≥ class ≤name:w≥ : ≤baseClass≥ {") or
	        self.match("≤scope:w≥ class ≤name:w≥ : ≤baseClass≥ where ≤whereClause≥ {") or
	        self.match("≤scope:w≥ abstract class ≤name:w≥ {") or
	        self.match("≤scope:w≥ abstract class ≤name:w≥ : ≤baseClass≥ {") then
		self.prepareRefClass

	// class interface
	else if self.match("≤scope:w≥ interface ≤name:w≥ {") then
		self.enterState State.CLASS
		self.curClass.type = ClassType.INTERFACE
		// wrapper class (we'll take care to write this one out before all storage classes later)
		self.curClass.declLines += self.preLines; self.preLines = []
		self.curClass.declLines.push self.fill("class ≤name≥ {")
		self.curClass.declLines.push "  public:"
		self.curClass.declLines.push self.fill("	virtual ~≤name≥() = default;")

	// enum
	else if self.match("≤scope:w≥ enum ≤name:w≥ : ≤type≥ {") then
		self.enterState State.CLASS
		self.curClass.type = ClassType.ENUM
		self.curClass.declLines += self.preLines; self.preLines = []
		self.curClass.declLines.push self.fill("enum class ≤name≥ : ≤type≥ {")

	else if line.full.startsWith("}") then
		if not line.comment then line.comment = "// end of namespace " + self.namespace
		self.endLines.push "} " + line.comment
		self.enterState State.DONE
	end if
end function

Transpiler.updateStateCLASS = function
	line = self.line
	comment = self.line.comment
	
	if comment.startsWith("// CPP: ") then
		self.curClass.defLines.push line.indentation + comment[8:]
		return
	else if comment.startsWith("// H: ") then
		if self.curClass.type == ClassType.CLASS then
			// This is a little tricky — which part of the header is this line meant for?
			// So far we have only needed it in the storage class, so for now we'll assume:
			self.curClass.storageLines.push line.indentation + comment[6:]
		else
			self.curClass.declLines.push line.indentation + comment[6:]
		end if
		return
	else if comment.startsWith("// H_WRAPPER: ") then
		if self.curClass.type == ClassType.CLASS then
			// ...If we DO need to add a line to the wrapper code only, then
			// use this H_WRAPPER tag.
			self.curClass.wrapperLines.push line.indentation + comment[14:]
		else
			print "ERROR: H_WRAPPER comment when not in a class"
			exit
		end if
		return
	end if
	
	if line.full.startsWith("}") then
		if self.curClass.type == ClassType.CLASS then
			if self.preLines then self.curClass.storageLines += self.preLines
			self.curClass.storageLines.push "}; // end of class ≤name≥Storage".fill(self.curClass)
			self.curClass.wrapperLines.push "}; // end of struct ≤name≥".fill(self.curClass)
			if self.subclassesOf(self.curClass) then
				self.curClass.wrapperLines.push ""
				self.curClass.wrapperLines.push "template<typename WrapperType, typename StorageType> WrapperType As(≤name≥ inst);".fill(self.curClass)
			end if
		else
			if self.preLines then self.curClass.declLines += self.preLines
			self.curClass.declLines.push "}; // end of ≤type≥ ≤name≥".fill(self.curClass)
		end if
		self.preLines = []
		self.enterState State.NAMESPACE
		return
	end if
	
	if line.trimmed.startsWith("[") then
		// decorator -- most are ignored, but check for inline flag
		// and thread-local flag
		if line.trimmed.contains("AggressiveInlining") then
			self.nextMethodInline = true
		else if not line.trimmed.startsWith("[ThreadStatic]") then
			return
		end if
	end if
	
	if self.curClass.type == ClassType.ENUM then
		// enum value, with/without numeric value and with/without comma
		if self.match("≤name:w≥ = ≤value:w≥,") or self.match("≤name:w≥,") or
		   self.match("≤name:w≥ = ≤value:w≥")  or self.match("≤name:w≥") then
			self.curClass.declLines.push line.full
		end if
	else
		// constant (or static readonly field, which in C++ is the same thing)
		if self.match("≤scope:w≥ const ≤type:w≥ ≤name:w≥ = ≤value≥;") or
		   self.match("≤scope:w≥ static readonly ≤type:w≥ ≤name:w≥ = ≤value≥;") then
			self.m.value = self.transformStatement(self.m.value)
			if self.curClass.type == ClassType.CLASS then
				self.curClass.storageLines.push self.fill("	≤scope≥: static const ≤type≥ ≤name≥;")
				self.curClass.defLines.push self.fill("const ≤type≥ ≤className≥Storage::≤name≥ = ≤value≥;")
				self.curClass.wrapperLines.push self.fill("	≤scope≥: ≤type≥ ≤name≥();")
				self.curClass.inlineLines.push  self.fill("inline ≤type≥ ≤className≥::≤name≥() { return get()->≤name≥; }")
			else
				self.curClass.declLines.push self.fill("	≤scope≥: static const ≤type≥ ≤name≥;")
				self.curClass.defLines.push self.fill("const ≤type≥ ≤className≥::≤name≥ = ≤value≥;")
			end if
			if not self.m.hasIndex("name") then
				// WTF?!?
				print "line: " + self.line.trimmed
				print "name not found in: " + self.m
				print stackTrace.join(char(10))
				exit
			end if
			if self.gatherMode then self.curClass.fields.push VariableInfo.Make(self.m.name, self.m.type, self.m.scope, true)

		// ordinary field (with or without initializer)
		else if self.match("≤scope:w≥ ≤type:w≥ ≤name:w≥;") or
			  self.match("≤scope:w≥ ≤type:w≥ ≤name:w≥ = ≤expr≥;") or
			  self.match("≤scope:w≥ ≤type:w≥ ≤name:w≥ { get; private set; }") or
			  self.match("≤scope:w≥ ≤type:w≥[] ≤name:w≥;") then
			self.m.dec = "≤scope≥: ≤type≥ ≤name≥".fill(self.m)
			if line.trimmed.contains(self.m.type + "[] ") then self.m.dec += "[]"
			if self.m.hasIndex("expr") then
				exprm = self.m.expr.match("new ≤type≥(≤params≥)")
				if exprm then
					self.m.dec += " = ≤type≥::New(≤params≥)".fill(exprm)
				else
				   	self.m.dec += " = " + self.m.expr
				end if
			end if
			if self.curClass.type == ClassType.CLASS then
				self.curClass.storageLines.push self.fill("	≤dec≥;")
				self.curClass.wrapperLines.push self.fill("	≤scope≥: ≤type≥ ≤name≥();")
				self.curClass.inlineLines.push  self.fill("inline ≤type≥ ≤className≥::≤name≥() { return get()->≤name≥; }")
				self.curClass.wrapperLines.push self.fill("	≤scope≥: void set_≤name≥(≤type≥ _v);")
				self.curClass.inlineLines.push  self.fill("inline void ≤className≥::set_≤name≥(≤type≥ _v) { get()->≤name≥ = _v; }")
			else
				self.curClass.declLines.push self.fill("	≤dec≥;")
			end if
			if self.gatherMode then self.curClass.fields.push VariableInfo.Make(self.m.name, self.m.type, self.m.scope)

		// static field with default value
		else if self.match("≤scope:w≥ static ≤type:w≥ ≤name:w≥ = ≤value≥;") then
			self.m.value = self.transformStatement(self.m.value)
			if self.curClass.type == ClassType.CLASS then
				self.curClass.storageLines.push self.fill("	≤scope≥: static ≤type≥ ≤name≥;")
				self.curClass.defLines.push self.fill("	≤type≥ ≤className≥Storage::≤name≥;")
				self.curClass.wrapperLines.push self.fill("	≤scope≥: ≤type≥ ≤name≥();")
				self.curClass.inlineLines.push  self.fill("inline ≤type≥ ≤className≥::≤name≥() { return get()->≤name≥; }")
				self.curClass.wrapperLines.push self.fill("	≤scope≥: void set_≤name≥(≤type≥ _v);")
				self.curClass.inlineLines.push  self.fill("inline void ≤className≥::set_≤name≥(≤type≥ _v) { get()->≤name≥ = _v; }")
			else
				self.curClass.declLines.push self.fill("	≤scope≥: static ≤type≥ ≤name≥;")
				self.curClass.defLines.push self.fill("≤type≥ ≤className≥::≤name≥ = ≤value≥;")
			end if
			if self.gatherMode then self.curClass.fields.push VariableInfo.Make(self.m.name, self.m.type, self.m.scope, true)

		// thread-local static field
		else if self.match("[ThreadStatic] ≤scope:w≥ static ≤type:w≥ ≤name:w≥;") then
			if self.curClass.type == ClassType.CLASS then
				self.curClass.storageLines.push self.fill("	≤scope≥: thread_local static ≤type≥ ≤name≥;")
				self.curClass.defLines.push self.fill("	thread_local ≤type≥ ≤className≥Storage::≤name≥;")
				self.curClass.wrapperLines.push self.fill("	≤scope≥: ≤type≥ ≤name≥();")
				self.curClass.inlineLines.push  self.fill("inline ≤type≥ ≤className≥::≤name≥() { return get()->≤name≥; }")
				self.curClass.wrapperLines.push self.fill("	≤scope≥: void set_≤name≥(≤type≥ _v);")
				self.curClass.inlineLines.push  self.fill("inline void ≤className≥::set_≤name≥(≤type≥ _v) { get()->≤name≥ = _v; }")
			else
				self.curClass.declLines.push self.fill("	≤scope≥: thread_only static ≤type≥ ≤name≥;")
				self.curClass.defLines.push self.fill("≤type≥ ≤className≥::≤name≥ = ≤value≥;")
			end if
			if self.gatherMode then self.curClass.fields.push VariableInfo.Make(self.m.name, self.m.type, self.m.scope, true)

		// constructor method (no return type), with or without initializer list
		else if self.match("≤scope:w≥ ≤name:w≥(≤params≥) : base(≤initParams≥) {") or
			    self.match("≤scope:w≥ ≤name:w≥(≤params≥) {") then
			if self.m.hasIndex("initParams") then
				self.m.args = self.m.initParams;
				self.m.initBase = " : " + self.curClass.superclass + "(" + self.m.args + ") "
				self.m.initBaseStorage = " : " + self.curClass.superclass + "Storage(" + self.m.args + ") "
			else
				self.m.initBase = " "
				self.m.initBaseStorage = " "
			end if
			self.enterState State.METHOD
			if self.curClass.type == ClassType.CLASS then
				self.curClass.wrapperLines.push self.fill("	≤scope≥: static ≤name≥ New(≤params≥) {")
				self.curClass.wrapperLines.push self.fill("		return ≤name≥(std::make_shared<≤name≥Storage>(≤args≥));")
				self.curClass.wrapperLines.push self.fill("	}")
				self.curClass.storageLines.push self.fill("	≤scope≥: ≤name≥Storage(≤params≥);")
				self.stripDefaultArgs
				self.curClass.methodLines.push self.fill("≤inline≥≤className≥Storage::≤name≥Storage(≤params≥)≤initBaseStorage≥{")
//				root = self.curClass.rootClass(self.allClasses)
//				if root then self.m.rootClass = root.name
			else
				self.curClass.declLines.push self.fill("	≤scope≥: ≤name≥(≤params≥);")
				self.stripDefaultArgs
				self.curClass.methodLines.push self.fill("≤inline≥≤className≥::≤name≥(≤params≥)≤initBase≥{")
			end if
			if self.gatherMode then self.curClass.methods.push MethodInfo.Make(self.m.name)

		// empty constructor
		else if self.match("≤scope:w≥ ≤name:w≥(≤params≥) {}") then
			if self.curClass.type == ClassType.CLASS then
				self.curClass.wrapperLines.push self.fill("	≤scope≥: static ≤name≥ New(≤params≥) {")
				self.curClass.wrapperLines.push self.fill("		return ≤name≥(std::make_shared<≤name≥Storage>(≤args≥));")
				self.curClass.wrapperLines.push self.fill("	}")
				self.curClass.storageLines.push self.fill("	≤scope≥: ≤name≥Storage(≤params≥) {}")
			else
				self.curClass.declLines.push self.fill("	≤scope≥: ≤name≥(≤params≥) {}")
			end if
			if self.gatherMode then self.curClass.methods.push MethodInfo.Make(self.m.name)

		// conversion operator -- sadly these have to be static in C# and non-static in C++,
		// so the code is always going to be quite different.  But usually short, so just
		// use the "// CPP:" pattern on each line of the body, and it should work.
		else if self.match("≤scope:w≥ static implicit operator ≤type≥(≤params≥) {") then
			self.enterState State.METHOD
			if self.curClass.type == ClassType.CLASS then
				self.curClass.storageLines.push self.fill("	≤scope≥: operator ≤type≥() const;")
				self.curClass.wrapperLines.push self.fill(
				  "	≤scope≥: operator ≤type≥() const { return (≤type≥)(*get()); }")
				self.curClass.methodLines.push self.fill("≤inline≥≤className≥Storage::operator ≤type≥() const {")
			else
				self.curClass.declLines.push self.fill("	≤scope≥: operator ≤type≥() const;")
				self.curClass.methodLines.push self.fill("≤inline≥≤className≥::operator ≤type≥() const {")
			end if
			
		// static method
		else if self.match("≤scope:w≥ static ≤type:w≥ ≤name:w≥(≤params≥) {") then
			self.enterState State.METHOD
			if self.curClass.type == ClassType.CLASS then
				self.curClass.storageLines.push self.fill("	≤scope≥: static ≤type≥ ≤name≥(≤params≥);")
				self.curClass.wrapperLines.push self.fill(
				  "	≤scope≥: static ≤type≥ ≤name≥(≤params≥) { return ≤className≥Storage::≤name≥(≤args≥); }")
				self.stripDefaultArgs
				self.curClass.methodLines.push self.fill("≤inline≥≤type≥ ≤className≥Storage::≤name≥(≤params≥) {")
			else
				self.curClass.declLines.push self.fill("	≤scope≥: static ≤type≥ ≤name≥(≤params≥);")
				self.stripDefaultArgs
				self.curClass.methodLines.push self.fill("≤inline≥≤type≥ ≤className≥::≤name≥(≤params≥) {")
			end if
			if self.gatherMode then self.curClass.methods.push MethodInfo.Make(self.m.name, true)

		// expression-bodied static method
		else if self.match("≤scope:w≥ static ≤type:w≥ ≤name:w≥(≤params≥) => ≤expr≥;") then
			if self.curClass.type == ClassType.CLASS then
				// ToDo: check this.  I'm not sure the following is correct.
				self.curClass.storageLines.push self.fill("	≤scope≥: static ≤type≥ ≤name≥(≤params≥);")
				self.curClass.methodLines.push self.fill("≤inline≥≤type≥ ≤className≥Storage::≤name≥(≤params≥)" +
					" { return " + self.m.expr + "; }")
			else
				self.curClass.declLines.push self.fill("	≤scope≥: static ≤type≥ ≤name≥(≤params≥)" +
					" { return " + self.m.expr + "; }")
			end if
			if self.gatherMode then self.curClass.methods.push MethodInfo.Make(self.m.name, true)

		// standard method
		else if self.match("≤scope:w≥ ≤type:w≥ ≤name:w≥(≤params≥) {") or
				self.match("≤scope:w≥ ref ≤type:w≥ ≤name:w≥(≤params≥) {")then
			if line.trimmed.contains(" ref ") then self.m.type += "&"
			self.enterState State.METHOD
			if self.curClass.type == ClassType.CLASS then
				self.curClass.storageLines.push self.fill("	≤scope≥: ≤type≥ ≤name≥(≤params≥);")
				if comment.contains("NO_INLINE") then
					self.curClass.wrapperLines.push self.fill("	≤scope≥: ≤type≥ ≤name≥(≤params≥);")
					self.stripDefaultArgs
					self.curClass.defLines.push self.fill(
					  "≤type≥ ≤className≥::≤name≥(≤params≥) { return get()->≤name≥(≤args≥); }")
				else
					self.curClass.wrapperLines.push self.fill("	≤scope≥: inline ≤type≥ ≤name≥(≤params≥);")
					self.stripDefaultArgs
					self.curClass.inlineLines.push self.fill(
					  "inline ≤type≥ ≤className≥::≤name≥(≤params≥) { return get()->≤name≥(≤args≥); }")
				end if
				self.curClass.methodLines.push self.fill("≤inline≥≤type≥ ≤className≥Storage::≤name≥(≤params≥) {")
			else
				self.curClass.declLines.push self.fill("	≤scope≥: ≤type≥ ≤name≥(≤params≥);")
				self.stripDefaultArgs
				self.curClass.methodLines.push self.fill("≤inline≥≤type≥ ≤className≥::≤name≥(≤params≥) {")
			end if
			if self.gatherMode then self.curClass.methods.push MethodInfo.Make(self.m.name)

		// virtual method
		// (ToDo: try to consolidate some of these method variations)
		else if self.match("≤scope:w≥ virtual ≤type:w≥ ≤name:w≥(≤params≥) {") then
			self.enterState State.METHOD
			if self.curClass.type == ClassType.CLASS then
				self.curClass.storageLines.push self.fill("	≤scope≥: virtual ≤type≥ ≤name≥(≤params≥);")
				self.curClass.wrapperLines.push self.fill("	≤scope≥: inline virtual ≤type≥ ≤name≥(≤params≥);")
				self.stripDefaultArgs
				self.curClass.inlineLines.push self.fill(
				  "inline ≤type≥ ≤className≥::≤name≥(≤params≥) { return get()->≤name≥(≤args≥); }")
				self.curClass.methodLines.push self.fill("≤inline≥≤type≥ ≤className≥Storage::≤name≥(≤params≥) {")
			else
				self.curClass.declLines.push self.fill("	≤scope≥: virtual ≤type≥ ≤name≥(≤params≥);")
				self.stripDefaultArgs
				self.curClass.methodLines.push self.fill("≤inline≥≤type≥ ≤className≥::≤name≥(≤params≥) {")
			end if
			if self.gatherMode then self.curClass.methods.push MethodInfo.Make(self.m.name)

		// interface method
		else if self.match("≤type:w≥ ≤name:w≥(≤params≥);") then
			self.assert self.curClass.type == ClassType.INTERFACE
			self.curClass.declLines.push self.fill("	virtual ≤type≥ ≤name≥(≤params≥) = 0;")
			if self.gatherMode then self.curClass.methods.push MethodInfo.Make(self.m.name)
		
		// abstract method: in C++ we actually implement this (inline) on the wrapper
		// struct, calling through to the storage, where it may be overridden.
		else if self.match("≤scope:w≥ abstract ≤type:w≥ ≤name:w≥(≤params≥);") then
			self.assert self.curClass.type == ClassType.CLASS
			self.curClass.wrapperLines.push self.fill("	≤scope≥: ≤type≥ ≤name≥(≤params≥);")
			self.curClass.storageLines.push self.fill("	≤scope≥: virtual ≤type≥ ≤name≥(≤params≥) = 0;")
			// Also todo: strip defaults but NOT types, for use in implementation;
			// we'll need that in several places here (anywhere we have a method body).
			self.stripDefaultArgs
			self.curClass.inlineLines.push self.fill(
			  "inline ≤type≥ ≤className≥::≤name≥(≤params≥) { return get()->≤name≥(≤args≥); }")
			if self.gatherMode then self.curClass.methods.push MethodInfo.Make(self.m.name)

		// override method
		else if self.match("≤scope:w≥ override ≤type:w≥ ≤name:w≥(≤params≥) {") then
			self.enterState State.METHOD
			if self.curClass.type == ClassType.CLASS then
				self.curClass.storageLines.push self.fill("	≤scope≥: ≤type≥ ≤name≥(≤params≥);")
				self.curClass.wrapperLines.push self.fill(
				  "	≤scope≥: ≤type≥ ≤name≥(≤params≥) { return get()->≤name≥(≤args≥); }")
				self.stripDefaultArgs
				self.curClass.methodLines.push self.fill("≤inline≥≤type≥ ≤className≥Storage::≤name≥(≤params≥) {")
			else
				self.curClass.declLines.push self.fill("	≤scope≥: ≤type≥ ≤name≥(≤params≥);")
				self.stripDefaultArgs
				self.curClass.methodLines.push self.fill("≤inline≥≤type≥ ≤className≥::≤name≥(≤params≥) {")
			end if
			if self.gatherMode then self.curClass.methods.push MethodInfo.Make(self.m.name)
		else if self.line.trimmed == "" then
			self.preLines.push self.line.full
		else if self.line.trimmed.contains("ctiveVM") then
			print "====> Didn't match anything: " + self.line.full
		end if
	end if
end function

Transpiler.updateStateMETHOD = function
	line = self.line
	comment = self.line.comment

	// outdent method lines by 1 tab
	if line.full.startsWith(TAB) then
		line.full = line.full[1:]
		line.indentation = line.indentation[1:]
	end if
	
	if comment.startsWith("// CPP:") then
		if comment.len > 8 then
			self.curClass.methodLines.push line.indentation + comment[8:]
			// check for local declarations even if they've been overridden for C++
			if self.match("≤type:w≥ ≤name:w≥ = ≤expr≥ as ≤type2:w≥;") or
			   self.match("≤type:w≥ ≤name:w≥ = ≤expr≥;") or
			   self.match("var ≤name:w≥ = new ≤type:w≥(≤args≥);")then
				self.localVars.push VariableInfo.Make(self.m.name, self.m.type, Scope.LOCAL)
			end if
		end if 
		return
	end if

	if line.full.startsWith("}") then
		if self.pushedGCScope and not self.curClass.methodLines[-1].contains("return") then
			self.curClass.methodLines.push "	GC_POP_SCOPE();"
		end if
		self.curClass.methodLines.push line.full
		self.enterState State.CLASS, false
		return
	end if
	
	// if in gather mode, just skip the method internals
	if self.gatherMode then return
	
	// check for local variable declarations
	if self.match("≤type:w≥ ≤name:w≥ = ≤expr≥ as ≤type2:w≥;") then
		if self.m.type == "var" then self.m.type = self.m.type2
		self.localVars.push VariableInfo.Make(self.m.name, self.m.type, Scope.LOCAL)
		self.m.expr = self.transformStatement(self.m.expr)
		self.curClass.methodLines.push line.indentation +
		  self.fill("≤type≥ ≤name≥ = As<≤type2≥, ≤type2≥Storage>(≤expr≥);")
		return
	end if
	if self.match("var ≤name:w≥ = new ≤type:w≥(≤args≥);") then
		self.localVars.push VariableInfo.Make(self.m.name, self.m.type, Scope.LOCAL)
		self.curClass.methodLines.push line.indentation +
		  self.fill("≤type≥ ≤name≥ = new ≤type≥(≤args≥);")
		return
	end if
	if (self.match("≤type:w≥ ≤name:w≥;") or
	   self.match("≤type:w≥ ≤name:w≥ =") or
	   self.match("≤type:w≥ ≤name:w≥ = ≤expr≥;")) and not isKeyword(self.m.type) then
		self.localVars.push VariableInfo.Make(self.m.name, self.m.type, Scope.LOCAL)
		// special case: watch for declarations of type Value, which need GC protection.
//		if self.m.type == "Value" then
//			// Also, make sure these are not too indented; Value declarations should always
//			// be at the root level of a method (especially, never in a loop).
//			self.assert line.indentation.len < 2 + (self.state == State.LAMBDA), 
//			   "Value declaration must be at method root"
//			// And be sure to call GC_PROTECT before the first such
//			if not self.pushedGCScope then
//			self.curClass.methodLines.push line.indentation + "GC_PUSH_SCOPE();"
//				self.pushedGCScope = true;
//			end if
//			s = "Value " + self.m.name
//			if self.m.get("expr") then s += " = " + self.transformStatement(self.m.expr);
//		s = line.indentation + s + "; GC_PROTECT(&" + self.m.name + ");"
//			if line.comment then s += " " + line.comment
//			self.curClass.methodLines.push s
//			return
//		end if
	end if
	if self.match("≤type:w≥[] ≤name:w≥ = new ≤type2≥[≤count≥];") then
		self.localVars.push VariableInfo.Make(self.m.name, self.m.type, Scope.LOCAL)
		self.localVars[-1].array = true
		self.curClass.methodLines.push line.indentation +
		  self.fill("≤type≥ ≤name≥[≤count≥];")
		return
	end if
	
	// check for other special patterns:
	// foreach statement
	if self.match("foreach (≤type:w≥ ≤var:w≥ in ≤seq≥)") then
		// ToDo: this block looks obsolete (see correct block below).
		// Try removing it when doing a cleanup pass.
		self.cppLines.push self.fill("for (int __≤var≥_idx=0, __≤var≥_qty=≤seq≥.Count(); __≤var≥_idx < __≤var≥_qty; __≤var≥_idx++)")
		self.cppLines.push self.fill("	≤type≥ ≤var≥ = ≤seq≥[__≤var≥_idx];")
		return
	end if
	if self.match("foreach (≤type:w≥ ≤var:w≥ in ≤seq≥) {") then
		self.localVars.push VariableInfo.Make(self.m.var, self.m.type, Scope.LOCAL)
		cline = line.indentation + self.fill("for (≤type≥ ≤var≥ : ≤seq≥) {") + line.comment;
		cline = fixStringInterpolation(cline)
		cline = self.transformStatement(cline)
		self.curClass.methodLines.push cline
		return
	end if
	
	// return statements, when we've pushed a GC scope (ToDo: remove obsolete GC handling!)
	if self.pushedGCScope then
		toks = roughTokenize(line.trimmed)
		idx = toks.indexOf("return")
		if idx != null then
			if idx > 0 then
				// harder case, a conditional like `if (foo) return expr;`
				cline = toks[:idx].join("")
				cline = self.transformStatement(fixStringInterpolation(cline))
				self.curClass.methodLines.push line.indentation + cline + " {" + line.comment;
//				self.curClass.methodLines.push line.indentation + "	GC_POP_SCOPE();"
				cline = toks[idx:].join("")
				cline = self.transformStatement(fixStringInterpolation(cline))
				self.curClass.methodLines.push line.indentation + TAB + cline;
				self.curClass.methodLines.push line.indentation + "}"
				return
			else
				// simple case, a return statement on its own line
//				self.curClass.methodLines.push line.indentation + "GC_POP_SCOPE();"
			end if
		end if
	end if
	// start of a lambda definition
	if self.match("≤var≥.≤member≥ = (Context ≤ctx≥, IntrinsicResult ≤part≥) => {") then
		// Special case of a NativeCallbackDelegate, which returns Value
		self.enterState State.LAMBDA
		self.curClass.methodLines.push self.fill(
		  "	≤var≥.set_≤member≥([](Context ≤ctx≥, IntrinsicResult ≤part≥) -> IntrinsicResult {")
		ctxInfo = VariableInfo.Make(self.m.ctx, "Context", Scope.LOCAL)
		if self.localVars.indexOf(ctxInfo) == null then self.localVars.push ctxInfo 
		partInfo = VariableInfo.Make(self.m.part, "IntrinsicResult", Scope.LOCAL)
		if self.localVars.indexOf(partInfo) == null then self.localVars.push partInfo 
		return
	else if self.match("≤var≥.≤member≥ = (≤params≥) => {") then
		// More general case: no way to know what it returns.  We'll assume nothing
		self.enterState State.LAMBDA
		self.curClass.methodLines.push self.fill(
		  "	≤var≥.set_≤member≥([](≤params≥) -> {")
		return
	else if line.trimmed.endsWith(" => {") then
		// Super-general case, we have no idea what's going on here.  But it's a lambda.
		self.enterState State.LAMBDA
	end if
	
	if self.curClass.name == "VM" then
		// Special case: VM dispatch handling
		if self.match("case Opcode.≤opcode≥: {") then
			self.curClass.methodLines.push line.indentation + self.fill("VM_CASE(≤opcode≥) {")
			self.inVmCase = 1
			return
		end if
		if self.inVmCase then
			self.inVmCase += line.trimmed.endsWith("{") - line.trimmed.startsWith("}")
		end if
		if self.line.trimmed == "break;" and self.inVmCase then
			self.curClass.methodLines.push line.indentation + "VM_NEXT();"
			return
		end if
	end if
	
	
	// otherwise, apply standard transformations, then write out
	cline = line.full
	cline = fixStringInterpolation(cline)
	cline = self.transformStatement(cline)
	self.curClass.methodLines.push cline

end function

Transpiler.generateOutput = function
	headerLines = []
	cppLines = []
	baseName = self.sourceName - ".cs"

	nonemptyCount = 0
	for c in self.allClasses; if c.hasLines then nonemptyCount += 1; end for
	
	if not self.headerLines and not self.cppLines and not nonemptyCount then
		print "Empty (probably C#-only); skipping."
		return
	end if

	// Generate the header
	headerLines.push "// AUTO-GENERATED FILE.  DO NOT MODIFY."
	headerLines.push "// Transpiled from: " + self.sourceName
	headerLines.push ""
	headerLines.push "#pragma once"
	headerLines.push "#include ""core_includes.h"""
	headerLines.push "#include ""forward_decs.g.h"""
	headerLines += self.headerLines

	headerLines.push ""
	headerLines.push "// DECLARATIONS"
	headerLines.push ""
	for class in self.allClasses
		if class.elsewhere then continue
		headerLines += class.declLines
		headerLines.push ""
	end for
	done = {}
	// The storage/wrapper declarations must be done in a very specific order. 
	// First, the wrapper for any base class (since subclass storage is likely to need it).
	for class in self.allClasses
		if class.elsewhere or not class.wrapperLines or not self.subclassesOf(class) then continue
		headerLines += class.wrapperLines
		headerLines.push ""
		done.push class.name
	end for
	
	// Then, all the storage classes.
	for class in self.allClasses
		if not class.storageLines then continue
		headerLines += class.storageLines
		headerLines.push ""
	end for
	// Finally, all the wrapper classes not done above.
	for class in self.allClasses
		if not class.wrapperLines or done.hasIndex(class.name) then continue
		headerLines += class.wrapperLines
		headerLines.push ""
	end for

	headerLines.push ""
	headerLines.push "// INLINE METHODS"
	headerLines.push ""
	for class in self.allClasses
		if not class.inlineLines then continue
		headerLines += class.inlineLines
		headerLines.push ""
	end for
	headerLines += self.endLines
	
	// Generate the cpp file
	cppLines.push "// AUTO-GENERATED FILE.  DO NOT MODIFY."
	cppLines.push "// Transpiled from: " + self.sourceName
	cppLines.push ""
	cppLines.push "#include """ + baseName + ".g.h"""
	cppLines += self.cppLines
	cppLines.push ""
	cppLines.push "namespace " + self.namespace + " {"
	cppLines.push ""
	for class in self.allClasses
		if not class.defLines then continue
		cppLines += class.defLines
		cppLines.push ""
	end for
	cppLines.push ""
	cppLines.push "} // end of namespace " + self.namespace

	// Compress runs of blank lines
	headerLines.compress ""
	cppLines.compress ""

	headerPath = file.child(self.outputFolder, baseName + ".g.h")
	cppPath = file.child(self.outputFolder, baseName + ".g.cpp")
	file.writeLines headerPath, headerLines
	debugPrint "Wrote " + headerLines.len + " lines to " + headerPath
	file.writeLines cppPath, cppLines
	debugPrint "Wrote " + cppLines.len + " lines to " + cppPath
	
end function

Transpiler.processLine = function(line)
	self.line = Line.Make(line)
	
	state = self.state
	line = self.line.trimmed
	comment = self.line.comment
	debugPrint "[≤state≥] ≤line≥ ≤comment≥".fill(locals)
	
	// Check for special control comments
	if comment == "//*** BEGIN CS_ONLY ***" then
		self.pushState State.CS_ONLY
	else if comment == "//*** END CS_ONLY ***" then
		self.assert self.state == State.CS_ONLY, "END CS_ONLY tag encountered when not in CS_ONLY state"
		self.popState
	else if line == "/*** BEGIN H_ONLY ***" then
		self.pushState State.H_ONLY
	else if line == "*** END H_ONLY ***/" then
		self.assert self.state == State.H_ONLY, "END H_ONLY tag encountered when not in H_ONLY state"
		self.popState
	else if line == "/*** BEGIN CPP_ONLY ***" then
		self.pushState State.CPP_ONLY
	else if line == "*** END CPP_ONLY ***/" then
		self.assert self.state == State.CPP_ONLY, "END CPP_ONLY tag encountered when not in CPP_ONLY state"
		self.popState
	else
		// Otherwise, just update based on this line and our current state
		self.updateState
	end if
	
end function

Transpiler.setOutputFolder = function(sourceDir, destPathOrNull=null)
	if destPathOrNull then
		self.outputFolder = resolvePath(destPathOrNull)
	else
		self.outputFolder = file.child(file.parent(sourceDir), "generated")
	end if
end function

Transpiler.process = function(filePath, gatherMode=false, outputFolder=null)
	if gatherMode then verb = "Scanning" else verb = "Transpiling"
	print verb + ": " + filePath
	self.sourcePath = filePath
	self.gatherMode = gatherMode
	self.state = ""
	self.enterState State.START
	for line in file.readLines(filePath)
		self.processLine line
	end for
	if not gatherMode then self.generateOutput
	debugPrint "Done processing: " + filePath
end function

Transpiler.clearOutput = function
	self.preLines = []
	self.headerLines = []
	self.cppLines = []
	self.endLines = []
	for cl in self.allClasses
		cl.clearOutput
	end for
end function


runUnitTests = function
	count = 0
	check = function(actual, expected)
		outer.count += 1
		if actual == expected then return
		print "UNIT TEST FAILED."
		print "EXPECTED: " + expected
		print "  ACTUAL: " + actual
		exit
	end function

	check "This is ""a quoted"" test".splitQuoted,
		["This", "is", """a quoted""", "test"]

	check "Double x=42, Widget widget, String foo=""bar, baz""".splitQuoted(","),
		["Double x=42", " Widget widget", " String foo=""bar, baz"""]
	
	check fixStringInterpolation("No interp here"), "No interp here"
	
	check fixStringInterpolation(
	  "x = $""{greeting}, {name}!"";"),
	  "x = Interp(""{}, {}!"", greeting, name);"

	check roughTokenize("if foo.bar == 40+2 {"),
	  ["if", " ", "foo", ".", "bar", " == 40+2 {"]

	check roughTokenize("if (x == ""a = (b)"") {"),
	  ["if", " ", "(", "x", " == ""a = (b)""", ")", " {"]
	
	check roughTokenize("if (""1.0"" == 2.0) {"),
	  ["if", " ", "(", """1.0"" == 2.0", ")", " {"]
	
	check roughTokenize("foo = new Dictionary<Bar, Baz>();"),
	  ["foo", " ", "=", " ", "new", " ", "Dictionary<Bar, Baz>", "();"]
	
	check roughTokenize("return new String(""[\"""") + String.Join(""\"", \"""", list) + ""\""]"";"),
	  ["return", " ", "new", " ", "String", "(", """[\""""", ")", " + ",
	   "String", ".", "Join", "(", """\"", \"""", ", "list", ")", " + ""\""]"";"]
	
	check roughTokenize("if (Functions[i].Name == name) return i;"),
	  ["if", " ", "(", "Functions", "[", "i", "]", ".", "Name", " == ", "name", ")", " ", "return", " ", "i", ";"]
	
	check parameterNames("Double x=42, Widget widget, String foo=""bar=baz"""),
	  "x, widget, foo"

	// To test transformStatement, we need to set up a context with some classes,
	// methods, and fields.
	t = Transpiler.Make
	cls = ClassInfo.Make("Widget")
	cls.fields.push VariableInfo.Make("size", "int", Scope.PUBLIC)
	cls.fields.push VariableInfo.Make("secret", "String", Scope.PRIVATE)
	t.allClasses.push cls
	cls = ClassInfo.Make("Factory")
	cls.fields.push VariableInfo.Make("defaultWidget", "Widget", Scope.PUBLIC)
	cls.fields[-1].static = true
	cls.methods.push MethodInfo.Make("MakeWidget")
	cls.methods[-1].type = "Widget"
	cls.methods[-1].static = true
	t.allClasses.push cls
	t.localVars = []
	t.localVars.push VariableInfo.Make("leftNum", "Widget", Scope.LOCAL)
	t.localVars.push VariableInfo.Make("rightNum", "Widget", Scope.LOCAL)
	
	check t.transformStatement("x = Factory.MakeWidget().size;"),
			"x = Factory::MakeWidget().size();"

	check t.transformStatement("x = Factory.defaultWidget.size;"),
			"x = Factory::defaultWidget().size();"

	check t.transformStatement("Factory.MakeWidget().size = 42;"),
			"Factory::MakeWidget().set_size(42);"

	check t.transformStatement("return new Holder(leftNum.size + rightNum.size);"),
			"return Holder(leftNum.size() + rightNum.size());"

	//print "All " + count + " unit tests completed successfully."
end function

resolvePath = function(path)
	if path[0] == "/" then return path
	p = file.child(file.curdir, path)
	info = file.info(p)
	if info == null then return null
	return info.path
end function

if locals == globals then
	debugMode = false
	
	filesToProcess = []
	outputDir = null
	contextDir = null
	i = 1
	while i < shellArgs.len
		if shellArgs[i] == "-d" then
			debugMode = true
		else if shellArgs[i] == "-o" then
			outputDir = resolvePath(shellArgs[i+1])
			i += 1
		else if shellArgs[i] == "-c" then
			contextDir = resolvePath(shellArgs[i+1])
			i += 1
		else
			argPath = resolvePath(shellArgs[i])
			if shellArgs[i].endsWith(".cs") then
				filesToProcess.push argPath
			else
				for filename in file.children(argPath)
					if filename.endsWith(".cs") then filesToProcess.push file.child(argPath, filename)
				end for
				if not outputDir then outputDir = argPath
			end if
		end if
		i += 1
	end while

	if debugMode then runUnitTests

	if not filesToProcess then
		print "No files to process.  Exiting."
		//exit
	end if
	
	t = Transpiler.Make
	t.setOutputFolder file.parent(filesToProcess[0]), outputDir
	if contextDir then print "Context directory: " + contextDir
	print "Files to process: " + filesToProcess
	print "Output directory: " + t.outputFolder
	if not file.info(t.outputFolder) then
		print "OUTPUT FOLDER DOES NOT EXIST -- NO OUTPUT WILL BE WRITTEN."
		if input("Proceed (y/n)? ").upper != "Y" then exit
	end if

	// First, go through files in "gather" mode to just gather up all the symbol info
	// (including all .cs files in the context directory).
	alreadyPrinted = {}
	printOnce = function(s)
		if alreadyPrinted.hasIndex(s) then return
		print s
		alreadyPrinted.push(s)
	end function
	if contextDir then
		for fname in file.children(contextDir)
			if fname.endsWith(".cs") then
				t.process file.child(contextDir, fname), true
				for cls in t.allClasses
					printOnce "- " + cls.summary
				end for
			end if
		end for
	end if
	for path in filesToProcess
		t.process path, true
		for cls in t.allClasses
			printOnce "- " + cls.summary
		end for
	end for
	
	// Then write out all the forward declarations to forward_decs.g.h
	decLines = []
	decLines.push "// FORWARD DECLARATIONS"
	decLines.push ""
	decLines.push "namespace MiniScript {"
	for class in t.allClasses
		if class.elsewhere then continue
		if class.type == ClassType.CLASS then
			decLines.push "struct ≤name≥;".fill(class)
			decLines.push "class ≤name≥Storage;".fill(class)
		else if class.type == ClassType.STRUCT and not class.static then
			decLines.push "struct ≤name≥;".fill(class)
		end if
	end for
	decLines.push "}"
	file.writeLines file.child(t.outputFolder, "forward_decs.g.h"), decLines
	print
	print "Wrote " + decLines.len + " lines to forward_decs.g.h"
	
	// Then process again, generating output
	print
	print 
	print
	
	if filesToProcess.len == 1 then debugMode = true
	
	for path in filesToProcess
		print
		t.clearOutput
		t.process path, false
	end for
end if
