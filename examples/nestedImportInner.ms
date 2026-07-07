// nestedImportInner: leaf module for the nested-import test.
// It imports nothing itself.  Loaded (indirectly) by nestedImportMain.ms
// via nestedImportOuter.ms.  See nestedImportMain.ms for the whole story.

answer = 42

double = function(x)
	return x * 2
end function
