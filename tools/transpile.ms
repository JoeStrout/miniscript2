import "stringUtil"
import "listUtil"
import "mapUtil"
import "fancyMatch"
import "qa"

//--------------------------------------------------------------------------------
// Constants
TAB = char(9)

Scope = {}						// Used for VariableInfo.scope and MethodInfo.scope
Scope.PUBLIC = "public"
Scope.PROTECTED = "protected"
Scope.PRIVATE = "private"
Scope.LOCAL = "local"

ClassType = {}					// Used for ClassInfo.type
ClassType.CLASS = "class"
ClassType.STRUCT = "struct"
ClassType.ENUM = "enum"
ClassType.INTERFACE = "interface"

State = {}						// used for Transpiler.state
State.START = "START"			// at start of file, before namespace
State.NAMESPACE = "NAMESPACE"	// within the namespace, but not in a class/struct/etc.
State.CLASS = "CLASS"			// within a class definition
State.METHOD = "METHOD"			// within a function/method definition
State.DONE = "DONE"				// past the end of the namespace (pretty much done)

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

roughTokenize = function(s)
	// Roughly tokenize the given string, which is mostly about separating
	// out the identifiers.  Anything that's not an identifier can be lumped
	// together with whitespace and neighboring operators, etc.  Example:
	//      "if foo.bar == 40+2 {" 
	// -->  ["if", " ", "foo", ".", "bar", " == 40+2 {"]
	result = []
	tokStart = 0
	inIdentifier = false
	inString = false
	for i in s.indexes
		c = s[i]
		if c == """" then inString = not inString
		if inString then continue
		if inIdentifier then
			if isIdentifierChar(c, false) then continue
			if i then result.push s[tokStart:i]
			tokStart = i
			inIdentifier = false
		else
			if not isIdentifierChar(c, true) then continue
			if i then result.push s[tokStart:i]
			tokStart = i
			inIdentifier = true
		end if
	end for
	result.push s[tokStart:]
	return result
end function

parameterNames = function(s)
	// Strip types and default values from the given parameter declaration.
	if not s then return s
	result = []
	//print s.splitQuoted(",")
	for param in s.splitQuoted(",")
		typeAndRest = param.trim.split(" ", 2)
		nameEtc = typeAndRest[1].split("=", 2)
		result.push nameEtc[0]
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
ClassInfo = {}
ClassInfo.name = ""
ClassInfo.type = ClassType.CLASS
ClassInfo.superclass = ""		// name of superclass, if any
ClassInfo.fields = null			// list of VariableInfo
ClassInfo.methods = null		// list of MethodInfo
ClassInfo.declLines = null		// class declaration lines (header file)
ClassInfo.wrapperLines = null	// wrapper class declaration lines (header file)
ClassInfo.storageLines = null	// storage class declaration lines (header file)
ClassInfo.inlineLines = null	// inline definition lines (header file)
ClassInfo.defLines = null		// out-of-line definition lines (cpp file)
ClassInfo.methodLines = null	// reference to either .inlineLines or .defLines
ClassInfo.nextMethodInline = false

ClassInfo.Make = function(name="")
	noob = new ClassInfo
	noob.name = name
	noob.type = ClassType.CLASS
	noob.fields = []
	noob.methods = []
	noob.clearOutput
	return noob
end function

ClassInfo.clearOutput = function
	self.declLines = []
	self.wrapperLines = []
	self.storageLines = []
	self.inlineLines = []
	self.defLines = []
end function

ClassInfo.lookup = function(identifier, allClasses)
	// Look up the given identifier among our symbols, including inherited ones.
	cls = self
	while cls
		result = list.find(cls.fields, "name", identifier)
		if result == null then result = list.find(cls.methods, "name", identifier)
		if result then return result
		if not cls.superclass then return null
		cls = list.find(allClasses, "name", cls.superclass)
	end while
end function

ClassInfo.rootClass = function(allClasses)
	cls = self
	while cls.superclass
		cls = list.find(allClasses, "name", cls.superclass)
	end while
	return cls
end function

//--------------------------------------------------------------------------------
VariableInfo = {}				// used for fields, parameters, and local vars
VariableInfo.name = ""
VariableInfo.type = ""
VariableInfo.scope = ""			// Scope.PUBLIC, etc.
VariableInfo.static = false

VariableInfo.Make = function(name="", type="", scope="", static=false)
	noob = new VariableInfo
	noob.name = name
	if type then noob.type = type
	if scope then noob.scope = scope
	if static then noob.static = static
	return noob
end function

//--------------------------------------------------------------------------------
MethodInfo = {}
MethodInfo.name = ""
MethodInfo.type = "void"		// (return type)
MethodInfo.scope = ""			// Scope.PUBLIC, etc.
MethodInfo.static = false
MethodInfo.params = null		// list of VariableInfo
MethodInfo.inline = false

MethodInfo.Make = function(name="", static=false)
	noob = new MethodInfo
	noob.name = name
	if static then noob.static = true
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
Transpiler.allClasses = null	// list of ClassInfo we're processing (in this file)
Transpiler.curClass = null		// ClassInfo that's currently being processed
Transpiler.curMethod = null		// MethodInfo that's currently being processed
Transpiler.localVars = null		// current set of variables for method we're in
Transpiler.headerLines = null	// extra lines for top of header file
Transpiler.cppLines = null		// extra lines for top of .cpp file
Transpiler.endLines = null		// end of namespace, and anything that comes after

Transpiler.Make = function
	noob = new Transpiler
	noob.allClasses = []
	return noob
end function

Transpiler.sourceName = function; return file.name(self.sourcePath); end function

// Transpiler.match: match the given pattern against our current line; if found,
// copy self.line.m to self.m (for convenience), and also fill in className and
// inline.
Transpiler.match = function(pattern)
	if self.line.match(pattern) then
		self.m = self.line.m
		if self.curClass then
			self.m.className = self.curClass.name
			if self.curClass.nextMethodInline then
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
	result = pattern.fill(self.m)
	if addComment and self.line.comment then result += " " + self.line.comment
	return result
end function

Transpiler.findClass = function(name)
	for cl in self.allClasses
		if cl.name == name then return cl
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
		info = self.curClass.lookup(identifier, self.allClasses)
		if info then return info
	end if
	// 3. class names
	for cls in self.allClasses
		if cls.name == identifier then return cls
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
	//print "TOKENS: " + tokens
	
	// Then iterate and analyze each one.  We'll encounter things we don't 
	// recognize; that's OK.  Just keep track of stuff we do.
	didAssignment = false
	curInfo = null
	for i in tokens.indexes
		if not tokens[i] then continue
		if tokens[i] == "." then   // dot operator: resolve var type and continue
			if curInfo isa VariableInfo then curInfo = self.lookup(curInfo.type) // ToDo: context?
			if i and tokens[i-1] == "this" then
				curInfo = self.curClass
				tokens[i] = "->"
			end if
			// look up RHS type and see if it's static; if so, it needs "::" rather than "."
			rhsStatic = true
			//print "====> DOT FOUND BETWEEN " + tokens[i-1] + " and " + tokens[i+1] + "; curInfo is " + curInfo
			if curInfo then
				rhsType = self.lookup(tokens[i+1], curInfo)
				//print "====> RHS TYPE IS " + rhsType + " ==== WITH STATIC " + rhsType.static
				if rhsType then rhsStatic = rhsType.static
			end if
			if curInfo == null or rhsStatic then tokens[i] = "::"
			continue
		else if tokens[i] == "new" then
			// when `new` is followed by the name of a C# class (which we convert
			// using our storage-wrapper pattern), just remove `new`.
			cl = self.findClass(tokens[i+2])  // (skipping space)
			if cl and cl.type == ClassType.CLASS then
				tokens[i] = ""
				continue
			end if
		else if tokens[i] == "this" and (i+1 >= tokens.len or tokens[i+1] != ".") then
			tokens[i] = self.curClass.name + "(shared_from_this())";
			curInfo = self.curClass;
			continue
		else if tokens[i][0] == "(" and curInfo then  // method call!
			curInfo = self.lookup(curInfo.type)	// TODO: type stack, store until ')'
			//print "METHOD CALL returns: " + curInfo
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
			end if
		end if
		nextInfo = self.lookup(tokens[i], curInfo)
		if self.curClass then curClassName = self.curClass.name else curClassName = ""
		if curInfo isa ClassInfo and curInfo.type == ClassType.CLASS and
		  nextInfo isa VariableInfo and curInfo.name != curClassName then
			// Found a field lookup!  Or maybe an assignment.  Check the next token
			// to determine which.
			if i+1 < tokens.len and tokens[i+1].trimLeft.startsWith("= ") then
				//print "FIELD ASSIGNMENT (" + curInfo.name + "." + nextInfo.name + ") FOUND!!!"
				// Change this to a set_ call.
				tokens[i] = "set_" + tokens[i] + "("
				tokens[i+1] = tokens[i+1][2:].trimLeft
				nextInfo = null
				didAssignment = true
			else
				//print "FIELD LOOKUP (" + curInfo.name + "." + nextInfo.name + ") FOUND!!!"
				// Just add () to call the getter.
				tokens[i] += "()"
				nextInfo = self.lookup(nextInfo.type)
			end if
		end if
		curInfo = nextInfo
	end for
	
	result = tokens.join("")
	if didAssignment then
		// insert closing ')' (for the set_X call) before the semicolon
		pos = result.lastIndexOf(";")
		if pos == null then pos = result.len
		result = result[:pos] + ")" + result[pos:]
	end if
	//print "Transformed statement: " + result
	return result
	
end function

Transpiler.enterState = function(newState, initAsNew=true)
	if newState == self.state then return
	if self.state then self.exitState newState
	debugPrint "Entering state: " + newState
	self.state = newState
	
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
			if self.line and self.line.m and self.line.m.hasIndex("name") then
				name = self.line.m.name
			else
				name = "?"
			end if
			self.curClass = self.findOrMakeClass(name)
			self.curClass.declLines += self.preLines
			self.preLines = []
			self.curClass.defLines.push ""
		end if
		self.curClass.nextMethodInline = false

	else if newState == State.METHOD then
		if self.line and self.line.m and self.line.m.hasIndex("name") then
			name = self.line.m.name
		else
			name = "?"
		end if
		self.curMethod = MethodInfo.Make(name)
		if self.curClass.nextMethodInline then
			self.curClass.methodLines = self.curClass.inlineLines
		else
			self.curClass.methodLines = self.curClass.defLines
		end if
		self.localVars = []

	end if
	
end function

Transpiler.exitState = function(toNewState)
	debugPrint "Exiting state: " + self.state

	if self.state == State.METHOD then
		self.localVars = []
	end if

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
	else if self.state == State.DONE then
		self.endLines.push self.line.full	// ToDo: support H: and CPP: comments here?
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
	else
		print "Ignored in state START: " + line.full
	end if
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
		self.curClass.declLines.push self.fill("class ≤name≥ {")
	// struct
	else if self.match("≤scope:w≥ struct ≤name:w≥ {") then
		self.enterState State.CLASS
		self.curClass.type = ClassType.STRUCT
		self.curClass.declLines.push line.full
	// class (abstract)
	else if self.match("≤scope:w≥ abstract class ≤name:w≥ {") then
		self.enterState State.CLASS
		// wrapper class (we'll take care to write this one out before all storage classes later)
		self.curClass.wrapperLines.push self.fill("struct ≤name≥ {");
		self.curClass.wrapperLines.push self.fill("	protected: std::shared_ptr<≤name≥Storage> storage;");
		self.curClass.wrapperLines.push "  public:"
		self.curClass.wrapperLines.push self.fill("	≤name≥(std::shared_ptr<≤name≥Storage> stor) : storage(stor) {}")
		self.curClass.wrapperLines.push self.fill("	≤name≥() : storage(nullptr) {}")
		self.curClass.wrapperLines.push self.fill("	friend bool IsNull(≤name≥ inst) { return inst.storage == nullptr; }")
		if self.subclassesOf(self.curClass) then
			self.curClass.wrapperLines.push self.fill("	template<typename WrapperType, typename StorageType>")
			self.curClass.wrapperLines.push self.fill("	friend WrapperType As(ASTNode node) {")
			self.curClass.wrapperLines.push self.fill("		StorageType* stor = dynamic_cast<StorageType*>(node.storage.get());")
			self.curClass.wrapperLines.push self.fill("		if (stor == nullptr) return WrapperType(nullptr);")
			self.curClass.wrapperLines.push self.fill("		return WrapperType(node.storage);")
			self.curClass.wrapperLines.push self.fill("	}")
		end if
		self.curClass.wrapperLines.push ""
		// but we still need the storage class, too (with a virtual destructor)
		self.curClass.storageLines.push self.fill("class ≤name≥Storage : public std::enable_shared_from_this<≤name≥Storage> {")
		self.curClass.storageLines.push self.fill("	public: virtual ~≤name≥Storage() {}")
		
	// class (subclass)
	else if self.match("≤scope:w≥ class ≤name:w≥ : ≤baseClass≥ {") then
		self.enterState State.CLASS
		self.curClass.superclass = self.m.baseClass
		// subclasses: first a storage class, in the storage lines...
		self.curClass.storageLines.push self.fill("class ≤name≥Storage : public ≤baseClass≥Storage {")
		// ...and then a wrapper class too
		self.curClass.wrapperLines.push self.fill("struct ≤name≥ : public ≤baseClass≥ {")
		self.curClass.wrapperLines.push self.fill("	public: ≤name≥(std::shared_ptr<≤baseClass≥Storage> stor) : ≤baseClass≥(stor) {}")
		self.curClass.wrapperLines.push self.fill("	private: ≤name≥Storage* get() { return static_cast<≤name≥Storage*>(storage.get()); }")

	// enum
	else if self.match("≤scope:w≥ enum ≤name:w≥ : ≤type≥ {") then
		self.enterState State.CLASS
		self.curClass.type = ClassType.ENUM
		self.curClass.declLines.push line.full		
	else if line.full.startsWith("}") then
		if not line.comment then line.comment = "// end of namespace " + self.namespace
		self.endLines.push "} " + line.comment
		self.enterState State.DONE
	end if
end function

Transpiler.updateStateCLASS = function
	line = self.line
	comment = self.line.comment

	if line.full.startsWith("}") then
		if self.curClass.type == ClassType.CLASS then
			self.curClass.storageLines.push "}; // end of class ≤name≥Storage".fill(self.curClass)
			self.curClass.wrapperLines.push "}; // end of struct ≤name≥".fill(self.curClass)
			if self.subclassesOf(self.curClass) then
				self.curClass.wrapperLines.push ""
				self.curClass.wrapperLines.push "template<typename WrapperType, typename StorageType> WrapperType As(≤name≥ inst);".fill(self.curClass)
			end if
		else
			self.curClass.declLines.push "}; // end of ≤type≥ ≤name≥".fill(self.curClass)
		end if
		self.enterState State.NAMESPACE
		return
	end if
	
	if line.trimmed.startsWith("[") then
		// decorator -- most are ignored, but check for inline flag
		if line.trimmed.contains("(AggressiveInlining)") then
			self.curClass.nextMethodInline = true
		end if
		return
	end if
	
	if self.curClass.type == ClassType.ENUM then
		// enum value, with/without numeric value and with/without comma
		if self.match("≤name:w≥ = ≤value:w≥,") or self.match("≤name:w≥,") or
		   self.match("≤name:w≥ = ≤value:w≥")  or self.match("≤name:w≥") then
			self.curClass.declLines.push line.full
		end if
	else
		// constant
		if self.match("≤scope:w≥ const ≤type:w≥ ≤name:w≥ = ≤value≥;") then
			self.curClass.declLines.push self.fill("	≤scope≥: static const ≤type≥ ≤name≥;")
			self.curClass.defLines.push self.fill("const ≤type≥ ≤className≥::≤name≥ = ≤value≥;")

		// ordinary field
		else if self.match("≤scope:w≥ ≤type:w≥ ≤name:w≥;") then
			if self.curClass.type == ClassType.CLASS then
				self.curClass.storageLines.push self.fill("	≤scope≥: ≤type≥ ≤name≥;")
				self.curClass.wrapperLines.push self.fill("	≤scope≥: ≤type≥ ≤name≥() { return get()->≤name≥; }")
				self.curClass.wrapperLines.push self.fill("	≤scope≥: void set_≤name≥(≤type≥ _v) { get()->≤name≥ = _v; }")
			else
				self.curClass.declLines.push self.fill("	≤scope≥: ≤type≥ ≤name≥;")
			end if
			if self.gatherMode then self.curClass.fields.push VariableInfo.Make(self.m.name, self.m.type, self.m.scope)

		// static field with default value
		else if self.match("≤scope:w≥ static ≤type:w≥ ≤name:w≥ = ≤value≥;") then
			self.curClass.declLines.push self.fill("	≤scope≥: static ≤type≥ ≤name≥;")
			self.curClass.defLines.push self.fill("≤type≥ ≤className≥::≤name≥ = ≤value≥;")
			if self.gatherMode then self.curClass.fields.push VariableInfo.Make(self.m.name, self.m.type, self.m.scope, true)

		// constructor method (no return type)
		else if self.match("≤scope:w≥ ≤name:w≥(≤params≥) {") then
			self.enterState State.METHOD
			self.m.paramNames = parameterNames(self.m.params)
			if self.curClass.type == ClassType.CLASS then
				self.curClass.storageLines.push self.fill("	≤scope≥: ≤name≥Storage(≤params≥);")
				self.m.rootClass = self.curClass.rootClass(self.allClasses).name
				self.curClass.wrapperLines.push self.fill(
				  "	≤scope≥: ≤name≥(≤params≥) : ≤rootClass≥(std::make_shared<≤name≥Storage>(≤paramNames≥)) {}")
				self.curClass.methodLines.push self.fill("≤inline≥≤className≥Storage::≤name≥Storage(≤params≥) {")
			else
				self.curClass.declLines.push self.fill("	≤scope≥: ≤name≥(≤params≥);")
				self.curClass.methodLines.push self.fill("≤inline≥≤className≥::≤name≥(≤params≥) {")
			end if
			if self.gatherMode then self.curClass.methods.push MethodInfo.Make(self.m.name)

		// static method
		else if self.match("≤scope:w≥ static ≤type:w≥ ≤name:w≥(≤params≥) {") then
			self.enterState State.METHOD
			if self.curClass.type == ClassType.CLASS then
				self.curClass.storageLines.push self.fill("	≤scope≥: static ≤type≥ ≤name≥(≤params≥);")
				self.curClass.methodLines.push self.fill("≤inline≥≤type≥ ≤className≥Storage::≤name≥(≤params≥) {")
			else
				self.curClass.declLines.push self.fill("	≤scope≥: static ≤type≥ ≤name≥(≤params≥);")
				self.curClass.methodLines.push self.fill("≤inline≥≤type≥ ≤className≥::≤name≥(≤params≥) {")
			end if
			if self.gatherMode then self.curClass.methods.push MethodInfo.Make(self.m.name, true)


		// standard method
		else if self.match("≤scope:w≥ ≤type:w≥ ≤name:w≥(≤params≥) {") then
			self.enterState State.METHOD
			if self.curClass.type == ClassType.CLASS then
				self.curClass.storageLines.push self.fill("	≤scope≥: ≤type≥ ≤name≥(≤params≥);")
				self.curClass.methodLines.push self.fill("≤inline≥≤type≥ ≤className≥Storage::≤name≥(≤params≥) {")
			else
				self.curClass.declLines.push self.fill("	≤scope≥: ≤type≥ ≤name≥(≤params≥);")
				self.curClass.methodLines.push self.fill("≤inline≥≤type≥ ≤className≥::≤name≥(≤params≥) {")
			end if
			if self.gatherMode then self.curClass.methods.push MethodInfo.Make(self.m.name)

		// abstract method: in C++ we actually implement this (inline) on the wrapper
		// struct, calling through to the storage, where it may be overridden.
		else if self.match("≤scope:w≥ abstract ≤type:w≥ ≤name:w≥(≤params≥);") then
			qa.assert self.curClass.type == ClassType.CLASS
			self.curClass.wrapperLines.push self.fill("	≤scope≥: ≤type≥ ≤name≥(≤params≥);")
			self.curClass.storageLines.push self.fill("	≤scope≥: virtual ≤type≥ ≤name≥(≤params≥) = 0;")
			self.m.paramNames = parameterNames(self.m.params)
			// Also todo: strip defaults but NOT types, for use in implementation;
			// we'll need that in several places here (anywhere we have a method body).
			self.curClass.inlineLines.push self.fill(
			  "inline ≤type≥ ≤className≥::≤name≥(≤params≥) { return storage->≤name≥(≤paramNames≥); }")
			if self.gatherMode then self.curClass.methods.push MethodInfo.Make(self.m.name)

		// override method
		else if self.match("≤scope:w≥ override ≤type:w≥ ≤name:w≥(≤params≥) {") then
			self.enterState State.METHOD
			if self.curClass.type == ClassType.CLASS then
				self.curClass.storageLines.push self.fill("	≤scope≥: ≤type≥ ≤name≥(≤params≥);")
				self.curClass.methodLines.push self.fill("≤inline≥≤type≥ ≤className≥Storage::≤name≥(≤params≥) {")
			else
				self.curClass.declLines.push self.fill("	≤scope≥: ≤type≥ ≤name≥(≤params≥);")
				self.curClass.methodLines.push self.fill("≤inline≥≤type≥ ≤className≥::≤name≥(≤params≥) {")
			end if
			if self.gatherMode then self.curClass.methods.push MethodInfo.Make(self.m.name)
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
	
	if line.full.startsWith("}") then
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
		print "NOTED LOCAL VAR WITH TYPECAST: " + self.localVars[-1]
		self.curClass.methodLines.push line.indentation +
		  self.fill("≤type≥ ≤name≥ = As<≤type2≥, ≤type2≥Storage>(≤expr≥);")
		return
	end if
	if self.match("var ≤name:w≥ = new ≤type:w≥(≤args≥);") then
		self.localVars.push VariableInfo.Make(self.m.name, self.m.type, Scope.LOCAL)
		print "NOTED LOCAL VAR WITH VAR: " + self.localVars[-1]
		self.curClass.methodLines.push line.indentation +
		  self.fill("≤type≥ ≤name≥ = new ≤type≥(≤args≥);")
		return
	end if
	if (self.match("≤type:w≥ ≤name:w≥;") or
	   self.match("≤type:w≥ ≤name:w≥ = ≤expr≥;")) and not isKeyword(self.m.type) then
		self.localVars.push VariableInfo.Make(self.m.name, self.m.type, Scope.LOCAL)
		print "NOTED LOCAL VAR: " + self.localVars[-1]
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

	// Generate the header
	headerLines.push "// AUTO-GENERATED FILE.  DO NOT MODIFY."
	headerLines.push "// Transpiled from: " + self.sourceName
	headerLines.push ""
	headerLines.push "#pragma once"
	headerLines.push "#include ""core_includes.h"""
	headerLines += self.headerLines

	headerLines.push ""
	headerLines.push "// FORWARD DECLARATIONS"
	headerLines.push ""
	for class in self.allClasses
		if class.type != ClassType.CLASS then continue
		headerLines.push "struct ≤name≥;".fill(class)
		headerLines.push "class ≤name≥Storage;".fill(class)
	end for

	headerLines.push ""
	headerLines.push "// DECLARATIONS"
	headerLines.push ""
	for class in self.allClasses
		headerLines += class.declLines
		headerLines.push ""
	end for
	// The storage/wrapper declarations must be done in a very specific order. 
	// First, the wrapper for any base class (since subclass storage is likely to need it).
	done = {}
	for class in self.allClasses
		if not class.wrapperLines or not self.subclassesOf(class) then continue
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
		cppLines += class.defLines
		cppLines.push ""
	end for
	cppLines.push ""
	cppLines.push "} // end of namespace " + self.namespace

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
	self.updateState	
end function

Transpiler.setOutputFolder = function(sourceDir, destPathOrNull=null)
	if destPathOrNull then
		self.outputFolder = resolvePath(destPathOrNull)
	else
		self.outputFolder = file.child(file.parent(sourceDir), "generated")
	end if
end function

Transpiler.process = function(filePath, gatherMode=false, outputFolder=null)
	debugPrint "Processing: " + filePath
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
	  ["if", " (", "x", " == ""a = (b)"") {"]
	
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
			"return new Holder(leftNum.size() + rightNum.size());"

	//print "All " + count + " unit tests completed successfully."
end function

resolvePath = function(path)
	if path[0] == "/" then return path
	if version.hostName == "Mini Micro" then return file.child(file.curdir, path)
	return file.child(file.curdir, path)
end function

if locals == globals then
	debugMode = true
	
	if debugMode then runUnitTests
	
	t = Transpiler.Make
	dir = resolvePath(shellArgs[1])
	if shellArgs.len > 3 and shellArgs[2] == "-o" then
		t.setOutputFolder dir, shellArgs[3]
	else
		t.setOutputFolder dir
	end if
	print "Processing directory: " + dir 
	print "Output directory:     " + t.outputFolder
	// First, go through files in "gather" mode to just gather up all the symbol info
	for filename in file.children(dir)
		if not filename.endsWith(".cs") or filename == "main.cs" then continue
		t.process file.child(dir, filename), true
	end for
	// Then process again, generating output
	print
	print 
	for cl in t.allClasses
		print "≤name≥ superclass: ≤superclass≥".fill(cl)
		print cl.name + " subclasses: " + t.subclassesOf(cl)
	end for
	print
	
	t.clearOutput
	for filename in file.children(dir)
		if not filename.endsWith(".cs") or filename == "main.cs" then continue
		print
		print "PROCESSING: " + filename
		print
		t.process file.child(dir, filename), false
		t.clearOutput
	end for
end if
