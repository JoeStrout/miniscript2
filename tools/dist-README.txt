MiniScript 2.0 (@PLATFORM@)

Contents
--------

@EXE_LIST@
  lib/                     Standard library modules, available via `import`
  MiniScript-Manual.pdf    The MiniScript language manual
  CommandLine.pdf          Using command-line MiniScript
  NewInMS2.pdf             What's new in MiniScript 2.0
  LICENSE                  MIT license
@REQUIREMENTS@
Usage
-----

Start the interactive prompt (REPL):

    @RUN@

Run a script:

    @RUN@ myscript.ms

List all command-line options:

    @RUN@ --help

Library modules
---------------

Keep the lib folder next to the executable.  When you `import` a module,
MiniScript looks in your script's folder, in a lib folder beside your
script, and in this lib folder, so `import "listUtil"` works with no setup.
To search elsewhere, set the MS_IMPORT_PATH environment variable; see
CommandLine.pdf for details.
