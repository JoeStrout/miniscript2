// This module provides a small utility to help with importing other
// modules in larger programs.  The advantage of ensureImport is that
// each module will only be imported once, even if it's called from
// multiple places; and even when those places are themselves an
// import file, the module will be imported at the global scope.
//
// Example usage:
//
// import "importUtil"
// ensureImport "grfon"
// ensureImport ["stringUtil", "listUtil", "mathUtil"]


// ensureImport: check whether the given module has been imported already
// (or is in the process of being imported).  If so, return immediately.
// If not, then import that module into the global namespace.  As a 
// convenience, you may also pass a list of module names to import.
globals.ensureImport = function(moduleName)
	if moduleName isa string then moduleName = [moduleName]
	for name in moduleName
		if globals.hasIndex(name) then continue
		globals[name] = "PENDING"	// (module is being imported now)
		import name
		globals[name] = locals[name]
	end for
end function

