Title: **key module for command-line MiniScript**
Status: Accepted

## Context

With the vt lib module, we can control the terminal console cursor and text mode/color, allowing for sophisticated drawing.  But we are currently still limited to line input, which makes it impossible to make an interactive game or custom editor.

Mini Micro handles non-line input via a `key` module.  It provides both `key.pressed` (for reading raw keyboard state) and `key.available`/`key.get` (for single-character input).

## Decision

Give command-line MiniScript an intrinsic `key` module (map) containing:

- `get`: blocks until an input character is available, then returns it
- `available`: function that returns true when input is available, false otherwise
- `raw`: a simple user-settable property, defaulting to 0 (false)

The `raw` flag controls how `get` handles special keys like arrow keys, backspace/delete, etc.  When `raw` is true, `get` does no translation; it returns the exact character sequence sent by the terminal, which will differ between Windows and Mac/Linux.  But when `raw` is false, then `get` remaps those keys to standard control codes as follows:

- KEY_HOME          = 1,
- KEY_END           = 5,
- KEY_BACKSPACE     = 8,
- KEY_RETURN        = 10,
- KEY_LEFT          = 17,
- KEY_RIGHT         = 18,
- KEY_UP            = 19,
- KEY_DOWN          = 20,
- KEY_PAGEUP        = 21,
- KEY_PAGEDOWN      = 22,
- KEY_FORWARD_DELETE = 127

Any escape sequences not matching the above are returned as 0 when `raw` is false.

Note that on POSIX, getting these character-wise inputs requires putting the terminal into raw mode.  This is done automatically on the first call to `get` or `available`, and restored automatically on `input` or on program exit (including via control-C).  Specifically:

- `key.get` or `key.available` turn off line-editing and terminal echo.
- `input` and `exec` turn on line-editing and terminal echo.


## Alternatives Considered

- `key.pressed` for checking raw keyboard state: hard to do in a console environment; deferred for now.
- Explicit control of terminal raw state: more work for the user, and harder to get right; we prefer to make it "just work" if possible.

## Consequences

- Key presses at the start of a run or after `input`/`exec`, but before any call to the `key` module, will be echoed (which may not be intended).
- When `raw` is true, character sequences returned by special keys are platform-specific (likely the same on Mac and Linux, but different on Windows).

When `raw` is false (the default), there are these additional consequences:
- There is no way to distinguish an arrow key from the equivalent control key; e.g., left-arrow and ^Q both return char(17).  Same for Home (^A), End (^E), etc.
- Other potential special keys (e.g. F keys) are not accessible at all.

