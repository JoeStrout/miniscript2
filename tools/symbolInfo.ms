// This module defines data structures to represent classes, methods,
// and variables in C# source.  It is used by the transpiler to build
// the symbol table as we go (and by coreClassInfo.ms to define symbol
// information for our core, non-transpiled classes).

globals.Scope = {}				// Used for VariableInfo.scope and MethodInfo.scope
Scope.PUBLIC = "public"
Scope.PROTECTED = "protected"
Scope.PRIVATE = "private"
Scope.LOCAL = "local"

globals.ClassType = {}			// Used for ClassInfo.type
ClassType.CLASS = "class"
ClassType.STRUCT = "struct"
ClassType.ENUM = "enum"
ClassType.INTERFACE = "interface"

//--------------------------------------------------------------------------------
globals.ClassInfo = {}
ClassInfo.name = ""
ClassInfo.type = ClassType.CLASS
ClassInfo.superclass = ""		// name of superclass, if any
ClassInfo.fields = null			// list of VariableInfo
ClassInfo.methods = null		// list of MethodInfo
ClassInfo.elsewhere = false		// if true, this class is not in the current file
ClassInfo.declLines = null		// class declaration lines (header file)
ClassInfo.wrapperLines = null	// wrapper class declaration lines (header file)
ClassInfo.storageLines = null	// storage class declaration lines (header file)
ClassInfo.inlineLines = null	// inline definition lines (header file)
ClassInfo.defLines = null		// out-of-line definition lines (cpp file)
ClassInfo.methodLines = null	// reference to either .inlineLines or .defLines
ClassInfo.T = null				// if specialized, this is the template type

ClassInfo.Make = function(name="")
	noob = new ClassInfo
	noob.name = name
	noob.type = ClassType.CLASS
	noob.fields = []
	noob.methods = []
	noob.clearOutput
	return noob
end function

ClassInfo.specialize = function(T)
	noob = new self
	noob.T = T
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
		if result and result.type == "<T>" then  // special case: resolve the template type
			result = new result
			result.type = self.T
		end if
		if result then return result
		if not cls.superclass then return null
		cls = list.find(allClasses, "name", cls.superclass)
	end while
end function

ClassInfo.rootClass = function(allClasses)
	cls = self
	while cls and cls.superclass
		cls = list.find(allClasses, "name", cls.superclass)
	end while
	return cls
end function

ClassInfo.summary = function
	if self.T then return self.type + " " + self.name + "<" + self.T + ">"
	return "CLASS: " + self.type + " " + self.name
end function

//--------------------------------------------------------------------------------
globals.VariableInfo = {}				// used for fields, parameters, and local vars
VariableInfo.name = ""
VariableInfo.type = ""
VariableInfo.scope = ""			// Scope.PUBLIC, etc.
VariableInfo.static = false
VariableInfo.array = false

VariableInfo.Make = function(name="", type="", scope="", static=false)
	noob = new VariableInfo
	noob.name = name
	if type then noob.type = type
	if scope then noob.scope = scope
	if static then noob.static = static
	return noob
end function

VariableInfo.summary = function
	parts = []
	if self.scope then parts.push self.scope
	if self.static then parts.push "static"
	parts.push self.type
	parts.push self.name
	if self.array then parts[-1] += "[]"
	return "VAR: " + parts.join
end function

VariableInfo.elementType = function
	elem = {} + self
	elem.array = false
	return elem
end function

//--------------------------------------------------------------------------------
globals.MethodInfo = {}
MethodInfo.name = ""
MethodInfo.type = "void"		// (return type)
MethodInfo.scope = ""			// Scope.PUBLIC, etc.
MethodInfo.static = false
MethodInfo.params = null		// list of VariableInfo
MethodInfo.inline = false
MethodInfo.hasThisLocal = false // true when we've defined _this

MethodInfo.Make = function(name="", static=false, type=null)
	noob = new MethodInfo
	noob.name = name
	if static then noob.static = true
	if type != null then noob.type = type
	return noob
end function

MethodInfo.summary = function
	parts = []
	if self.scope then parts.push self.scope
	if self.static then parts.push "static"
	parts.push self.type
	paramStrs = []
	if self.params then
		for param in self.params
			paramStrs.push param.summary
		end for
	end if
	parts.push self.name + "(" + paramStrs.join(", ") + ")"
	return "METHOD: " + parts.join
end function
