Title: Transpile C# to C++
Status: Accepted

Context:
We need solid reference implementations in both C# (mainly for Unity games, but also for things like the Farmville mod) and in C++ (for command-line MiniScript, raylib-miniscript, and embedding in other languages).  MS1 was written first in C# and then hand-ported to C++, but this was a frequent source of bugs and minor behavior differences.  We want to avoid that in MS2.

Decision:
Write the bulk of MiniScript 2 in C#, under certain well-documented restrictions, and then transpile this to equivalent C++.

Alternatives Considered:
- Hand-porting: we've been down that road and felt the pain it causes; would prefer not to do it again.
- Existing 3rd language: we looked for some language that would compile cleanly to both C# and C++, with comments etc. intact, but could not find one.
- Create a new 3rd language: seriously considered, but in the end decided that would be a big project in itself, and would greatly reduce the number of devs willing/able to contribute to the MS2 source base.

Consequences:
- We have to write a GC system for the C++ code that behaves similar to C#.
- We have to write some C++ classes to imitate common C# classes (String, List, Map, etc.).
- Most of the nonstandard/high-maintenance code (parser, compiler, code generator, test code, etc.) can be written just once, in C#, and compile to C++ code that is both readable and functionally identical.
- Occasionally, some code (particularly in "hot paths" like the VM core loop) will need hand-written C#/C++ versions; this will require uglifying the C# code a bit with C++ code embedded in comments.
