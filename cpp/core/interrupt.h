// interrupt.h
//
// User-interrupt (Ctrl-C) handling for interactive hosts such as the REPL.
//
// By default, Ctrl-C (SIGINT) terminates the process, which is right for a
// script run from the command line.  An interactive host instead calls
// Enable(), after which Ctrl-C merely sets a flag.  The host polls Pending()
// between slices of VM execution, stops the run when it is set, and calls
// Clear() before carrying on.
//
// The handler is installed without SA_RESTART, so a blocking read (the `input`
// intrinsic, key.get) is interrupted: the read fails with EINTR, and code that
// would normally retry on EINTR should check Pending() and give up instead.

#ifndef INTERRUPT_H
#define INTERRUPT_H

namespace MiniScript {
namespace Interrupt {

// Install the SIGINT handler, so Ctrl-C sets the pending flag instead of
// terminating the process.  Safe to call repeatedly.
void Enable();

// True if Enable() has been called.
bool Enabled();

// True if Ctrl-C has been pressed since the last Clear().
bool Pending();

// Forget any pending interrupt.
void Clear();

} // namespace Interrupt
} // namespace MiniScript

#endif // INTERRUPT_H
