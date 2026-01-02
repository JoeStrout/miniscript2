// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: App.cs

#include "App.g.h"
#include "UnitTests.g.h"
#include "VM.g.h"
#include "gc.h"
#include "gc_debug_output.h"
#include "value_string.h"
#include "dispatch_macros.h"
#include "VMVis.g.h"
#include "StringPool.h"
#include "MemPoolShim.g.h"
using namespace MiniScript;
gc_init();
"Build: C++ " VARIANT " version"
StringPool::dumpAllPoolState();
gc_dump_objects();
gc_mark_and_report();
dump_intern_table();

namespace MiniScript {















} // end of namespace MiniScript
