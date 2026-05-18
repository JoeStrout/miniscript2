#!/bin/bash
# Sync selected .ms files from ~/svnrepo/minimicro-sysdisk/sys/lib into this directory.
# Rules:
#   - No difference: print ": Unchanged"
#   - Source is newer: copy and print ": Updated"
#   - Destination is newer: print error and skip

SRC_DIR="$HOME/svnrepo/minimicro-sysdisk/sys/lib"
DEST_DIR="$(dirname "$0")"

FILES=(
    dateTime.ms
    grfon.ms
    importUtil.ms
    json.ms
    listUtil.ms
    mapUtil.ms
    mathUtil.ms
    matrixUtil.ms
    qa.ms
    stringUtil.ms
    tsv.ms
)

for file in "${FILES[@]}"; do
    src="$SRC_DIR/$file"
    dest="$DEST_DIR/$file"

    if [ ! -f "$src" ]; then
        echo "$file: ERROR - source not found at $src"
        continue
    fi

    if [ ! -f "$dest" ]; then
        cp "$src" "$dest"
        echo "$file: Copied (new)"
        continue
    fi

    if diff -q "$src" "$dest" > /dev/null 2>&1; then
        echo "$file: Unchanged"
        continue
    fi

    src_time=$(stat -f "%m" "$src" 2>/dev/null || stat -c "%Y" "$src")
    dest_time=$(stat -f "%m" "$dest" 2>/dev/null || stat -c "%Y" "$dest")

    if [ "$src_time" -gt "$dest_time" ]; then
        cp "$src" "$dest"
        echo "$file: Updated"
    else
        echo "$file: ERROR - local file is newer than source; skipping"
    fi
done
