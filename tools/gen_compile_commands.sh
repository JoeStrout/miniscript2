#!/bin/bash
# Generate compile_commands.json for clangd (and other clang tooling).
#
# Rather than intercepting a real build (the usual `bear -- make` approach,
# which on this machine would require building llvm and rust from source),
# this asks make to *describe* the build without running it:
#
#     make -n -B    ->  print every command, execute nothing
#
# so the flags recorded here are exactly the ones cpp/Makefile would use.
# Nothing is compiled and no object files are touched.
#
# Re-run this after adding source files, or after `tools/build.sh transpile`
# emits new generated/*.cpp. The output is machine-specific (absolute paths)
# and is gitignored.

set -e

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CPP_DIR="$PROJECT_ROOT/cpp"
OUTPUT="$PROJECT_ROOT/compile_commands.json"

if [ ! -f "$CPP_DIR/Makefile" ]; then
	echo "error: no Makefile found at $CPP_DIR/Makefile" >&2
	exit 1
fi

echo "=== Generating compile_commands.json ==="
echo "Source of truth: $CPP_DIR/Makefile"

# -n: dry run (print, don't execute).  -B: consider all targets out of date,
# so we get every compile command rather than only the stale ones.
make -C "$CPP_DIR" -n -B 2>/dev/null \
	| python3 -c '
import json, os, re, shlex, sys

cpp_dir = sys.argv[1]
output  = sys.argv[2]

# Match a compiler invocation that actually compiles a translation unit.
compiler = re.compile(r"^(g\+\+|c\+\+|clang\+\+|gcc|cc|clang)\s")

entries = []
seen = set()
for line in sys.stdin:
    line = line.strip()
    if not compiler.match(line) or " -c " not in line:
        continue
    try:
        argv = shlex.split(line)
    except ValueError:
        continue  # unbalanced quoting; not a command we can model

    # The source file is the argument following -c.
    try:
        src = argv[argv.index("-c") + 1]
    except (ValueError, IndexError):
        continue

    abs_src = os.path.normpath(os.path.join(cpp_dir, src))
    if abs_src in seen:
        continue  # e.g. a file built by both the normal and asan targets
    seen.add(abs_src)

    entries.append({
        "directory": cpp_dir,   # flags are relative to cpp/ (-Icore, -I../generated, -I.)
        "file": abs_src,
        "arguments": argv,
    })

if not entries:
    sys.exit("error: make produced no compile commands")

with open(output, "w") as f:
    json.dump(entries, f, indent=2)
    f.write("\n")

print("Wrote %d entries to %s" % (len(entries), output))
' "$CPP_DIR" "$OUTPUT"

echo "Done. Restart clangd (or your editor) to pick up the new database."
