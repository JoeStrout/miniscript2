Title: Custom GC system for Value types
Status: Accepted

Context:
MS1 used C#-native garbage collection for list/map/string data in C#, and a ref-counting system in C++.  These produced observably different behavior.  For MS2, we initially tried a general GC system in C++ meant to mimic managed memore in C#, but found it a pain to use.  We reduced its use to only Value data, relying on std::shared_ptr for host (compiler, app, etc.) data.  But it was still a pain to use.  And in C#, because there is no way to store a true reference in a NaN box, we relied on a "handle" system that effectively disabled garbage collection for lists/maps/strings entirely in C#.

We need a system that properly manages memory for Value types, behaving the same way in both C++ and C#.

Decision:
Build a custom GC system only for Value type data (maps, lists, strings, funcRefs, and a new "handle" type meant to be a wrapper of a native object).  We prototyped this first in MS2Prototypes/GCApproach2, and it appears to work well.

Alternatives Considered:
- reference counting: expensive and fiddly
- extensive C#-style GC system: requires protecting/unprotecting all local variables (adding them to the root set), which is error-prone and a chore
- MS1-style separate systems on each platform: produces behavioral differences between platforms, and also makes transpilation difficult

Consequences:
- same behavior on both platforms
- we have control over when GC happens
- no more GC_PROTECT and related macro calls all over the place
- we have to explicitly collect garbage at appropriate times (e.g. when the interpreter is yielding)
- long-lived values that might hold big data must be explicitly Retained and Released

