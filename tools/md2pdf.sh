#!/bin/bash
# md2pdf.sh -- render a Markdown file to a neatly formatted PDF.
#
# Uses pandoc (GitHub-flavored Markdown, including [!NOTE]-style alerts)
# plus WeasyPrint as the PDF engine, styled by tools/md2pdf.css.
#
# Both tools live in the `docs` micromamba environment; there are no
# Homebrew bottles for either on Intel macOS, so they come from conda-forge
# instead.  To recreate that environment:
#
#   micromamba create -y -n docs -c conda-forge pandoc pango python=3.12 pip
#   micromamba run -n docs pip install weasyprint
#   # ctypes' find_library needs unversioned names, or it picks up
#   # Homebrew's glib and WeasyPrint segfaults:
#   cd ~/micromamba/envs/docs/lib
#   ln -sf libgobject-2.0.0.dylib libgobject-2.0.dylib
#   ln -sf libglib-2.0.0.dylib libglib-2.0.dylib
#
# Usage: tools/md2pdf.sh input.md [output.pdf]
#        tools/md2pdf.sh docs/NewInMS2.md

set -euo pipefail

ENV_BIN="$HOME/micromamba/envs/docs/bin"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CSS="$SCRIPT_DIR/md2pdf.css"

if [ $# -lt 1 ]; then
	echo "Usage: $(basename "$0") input.md [output.pdf]" >&2
	exit 1
fi

INPUT="$1"
OUTPUT="${2:-${INPUT%.md}.pdf}"

if [ ! -f "$INPUT" ]; then
	echo "md2pdf: no such file: $INPUT" >&2
	exit 1
fi
if [ ! -x "$ENV_BIN/pandoc" ]; then
	echo "md2pdf: pandoc not found in $ENV_BIN (see the header of this script)" >&2
	exit 1
fi

export PATH="$ENV_BIN:$PATH"

pandoc "$INPUT" \
	--from=gfm+alerts \
	--to=html5 \
	--lua-filter="$SCRIPT_DIR/md2pdf-toc.lua" \
	--standalone \
	--css="$CSS" \
	--embed-resources \
	--metadata pagetitle="$(basename "${INPUT%.md}")" \
	--pdf-engine=weasyprint \
	--output="$OUTPUT" \
	2> >(grep -v -E '^WARNING: (Ignored|Expected a media type|Invalid media type)' >&2)

echo "wrote $OUTPUT"
