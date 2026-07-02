// miniscript.h
//
// Public embedding API for MiniScript 2.0.
//
// Host applications that embed MiniScript should include THIS header — and only
// this header — rather than reaching into the individual auto-generated ".g.h"
// files.  Those files are transpiler output: their names and the way the code
// is split across them track the C# sources and may change from release to
// release.  This header is hand-written and stable; it pulls in the pieces a
// host actually needs and provides a small compatibility layer.
//
// A host still needs the core, cpp, and generated directories on its include
// path (the generated files include each other, and "core_includes.h", by bare
// name).
//
#pragma once

#include "Interpreter.g.h"     // Interpreter; transitively Context, IntrinsicResult, VM, Value
#include "Intrinsic.g.h"       // Intrinsic builder API: Create / AddParam / code / GetFunc
#include "CoreIntrinsics.g.h"  // core intrinsics, plus the host-metadata storage aliased below
#include "value.h"             // Value, and (via core_includes.h) String, ValueList, ValueDict

namespace MiniScript {

// Host application identity.  Set these before the first call to the `version`
// intrinsic (typically once, during host startup).  They are exposed here as
// MiniScript globals for parity with MiniScript 1.x, and are references onto the
// fields that the `version` intrinsic actually reads, so assigning them takes
// effect immediately.
//
// NOTE: unlike 1.x, hostVersion is a String (e.g. "0.3"), not a double.
extern String& hostName;
extern String& hostInfo;
extern String& hostVersion;

} // namespace MiniScript
