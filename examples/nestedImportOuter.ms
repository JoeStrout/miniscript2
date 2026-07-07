// nestedImportOuter: middle module for the nested-import test.
// It is imported by nestedImportMain.ms, and itself imports nestedImportInner.
// This is the case that used to fail: an import that runs inside another import.
// See nestedImportMain.ms for the whole story.

import "nestedImportInner"

// Build our own values on top of the inner module's.
fromInner = nestedImportInner.answer          // 42
doubled = nestedImportInner.double(fromInner) // 84
label = "outer-loaded"
