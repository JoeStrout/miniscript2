// keyboard.cpp
//
// Implementation of the console keyboard API declared in keyboard.h.
// See that header for the rationale behind cbreak mode and the API contract.

#include "keyboard.h"

#ifdef _WIN32

#include <conio.h>
#include <io.h>
#include <cstdio>   // _fileno, EOF

namespace MiniScript {
namespace Keyboard {

// On Windows the console delivers single keystrokes directly through the
// conio functions, so there is no terminal mode to manage. We still track a
// flag so InRawMode() reports something sensible.
static bool s_rawActive = false;

bool EnterRawMode() {
	s_rawActive = (_isatty(_fileno(stdin)) != 0);
	return s_rawActive;
}

void ExitRawMode() {
	s_rawActive = false;
}

bool InRawMode() {
	return s_rawActive;
}

void EnterCookedMode() {
	// Windows conio does not change terminal mode, so there is nothing to do.
	s_rawActive = false;
}

bool KeyAvailable() {
	return _kbhit() != 0;
}

int ReadKey() {
	int c = _getch();
	if (c == EOF) return -1;
	return c & 0xFF;
}

int ReadKeyTranslated() {
	int c = _getch();
	if (c == EOF) return -1;

	// Extended keys arrive as a 0x00 or 0xE0 prefix followed by a scan code.
	if (c == 0x00 || c == 0xE0) {
		int s = _getch();
		if (s == EOF) return -1;
		switch (s) {
			case 75: return KEY_LEFT;
			case 77: return KEY_RIGHT;
			case 72: return KEY_UP;
			case 80: return KEY_DOWN;
			case 83: return KEY_FORWARD_DELETE;
			case 71: return KEY_HOME;
			case 79: return KEY_END;
			case 73: return KEY_PAGEUP;
			case 81: return KEY_PAGEDOWN;
			default: return KEY_NONE;
		}
	}

	if (c == 13) return KEY_RETURN;   // Enter key sends CR; normalize to 10
	// Backspace already arrives as 8 on Windows, so it passes straight through.
	return c & 0xFF;
}

} // namespace Keyboard
} // namespace MiniScript

#else // POSIX

#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <cstdlib>  // atexit
#include <cerrno>
#include <csignal>

namespace MiniScript {
namespace Keyboard {

static struct termios s_origTermios;     // terminal settings to restore
static bool s_origSaved       = false;   // have we captured s_origTermios?
static bool s_rawActive       = false;   // is cbreak mode currently active?
static bool s_handlersInstalled = false; // atexit + signal hooks installed?

// Restore the captured original terminal settings. optActions is the tcsetattr
// "when" flag: TCSADRAIN preserves typed-ahead (for handing off to line input);
// TCSAFLUSH discards it (for final cleanup, so stray keys don't reach the shell).
static void RestoreOriginal(int optActions) {
	if (s_origSaved) tcsetattr(STDIN_FILENO, optActions, &s_origTermios);
	s_rawActive = false;
}

static void RestoreAtExit() {
	ExitRawMode();
}

// Signals after which we want the terminal restored before the process dies.
// SIGINT/SIGQUIT (Ctrl-C, Ctrl-\) and SIGTERM/SIGHUP would otherwise terminate
// us with the terminal still in cbreak mode; SIGSEGV/SIGABRT cover crashes.
static const int kRestoreSignals[] = {
	SIGINT, SIGQUIT, SIGTERM, SIGHUP, SIGSEGV, SIGABRT
};

// Restore the terminal, then re-raise the signal with the default disposition so
// the process terminates (or dumps core) exactly as it normally would.
// tcsetattr/signal/raise are async-signal-safe.
static void SignalHandler(int sig) {
	RestoreOriginal(TCSAFLUSH);
	signal(sig, SIG_DFL);
	raise(sig);
}

bool EnterRawMode() {
	if (s_rawActive) return true;
	if (!isatty(STDIN_FILENO)) return false;
	if (tcgetattr(STDIN_FILENO, &s_origTermios) != 0) return false;
	s_origSaved = true;

	struct termios raw = s_origTermios;
	// Disable only canonical mode and echo: this is "cbreak" mode. We leave
	// ISIG (so Ctrl-C / Ctrl-Z still work), IXON (flow control), and OPOST
	// (output newline translation) alone, so ordinary printing and signals
	// behave exactly as before.
	raw.c_lflag &= ~(ICANON | ECHO);
	// read() returns as soon as one byte is available, with no inter-byte
	// timer. KeyAvailable() handles the non-blocking check separately.
	raw.c_cc[VMIN]  = 1;
	raw.c_cc[VTIME] = 0;

	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) return false;
	s_rawActive = true;

	if (!s_handlersInstalled) {
		atexit(RestoreAtExit);
		int n = (int)(sizeof(kRestoreSignals) / sizeof(kRestoreSignals[0]));
		for (int i = 0; i < n; i++) signal(kRestoreSignals[i], SignalHandler);
		s_handlersInstalled = true;
	}
	return true;
}

void ExitRawMode() {
	if (!s_rawActive) return;
	RestoreOriginal(TCSAFLUSH);   // final restore: discard any pending raw input
}

void EnterCookedMode() {
	if (!s_rawActive) return;
	RestoreOriginal(TCSADRAIN);   // preserve typed-ahead for the following read
}

bool InRawMode() {
	return s_rawActive;
}

bool KeyAvailable() {
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(STDIN_FILENO, &fds);
	struct timeval tv;
	tv.tv_sec  = 0;
	tv.tv_usec = 0;
	int r = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
	return r > 0 && FD_ISSET(STDIN_FILENO, &fds);
}

int ReadKey() {
	unsigned char c;
	ssize_t n;
	// Retry if interrupted by a signal handler.
	do {
		n = read(STDIN_FILENO, &c, 1);
	} while (n < 0 && errno == EINTR);
	if (n == 1) return (int)c;
	return -1;  // 0 == EOF, <0 == error
}

// ── ReadKeyTranslated support ────────────────────────────────────────────

// Milliseconds to wait for the next byte of a possible escape sequence. Real
// terminals deliver a whole sequence in one burst, so this only adds latency
// when the user actually presses a bare Escape key.
static const int kEscTimeoutMs = 40;

// Read one byte, waiting up to ms milliseconds. Returns true and stores the
// byte if one arrived; false on timeout, EOF, or error.
static bool ReadByteTimed(int ms, unsigned char* out) {
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(STDIN_FILENO, &fds);
	struct timeval tv;
	tv.tv_sec  = ms / 1000;
	tv.tv_usec = (ms % 1000) * 1000;
	int r = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
	if (r <= 0 || !FD_ISSET(STDIN_FILENO, &fds)) return false;
	ssize_t n;
	do {
		n = read(STDIN_FILENO, out, 1);
	} while (n < 0 && errno == EINTR);
	return n == 1;
}

// Parse the remainder of a CSI sequence (the bytes after "ESC ["). Returns the
// matching UnifiedKey, or KEY_NONE for anything we don't recognize.
static int ParseCSI() {
	unsigned char buf[8];
	int n = 0;
	unsigned char b;
	// Collect parameter/intermediate bytes until a final byte (0x40..0x7E).
	while (n < (int)sizeof(buf) && ReadByteTimed(kEscTimeoutMs, &b)) {
		buf[n++] = b;
		if (b >= 0x40 && b <= 0x7E) break;
	}
	if (n == 0) return KEY_NONE;
	unsigned char final = buf[n - 1];

	if (final == '~') {
		// Numeric form: ESC [ <num> ~
		int param = 0;
		for (int i = 0; i < n - 1 && buf[i] >= '0' && buf[i] <= '9'; i++) {
			param = param * 10 + (buf[i] - '0');
		}
		switch (param) {
			case 1: case 7: return KEY_HOME;
			case 4: case 8: return KEY_END;
			case 3:         return KEY_FORWARD_DELETE;
			case 5:         return KEY_PAGEUP;
			case 6:         return KEY_PAGEDOWN;
			default:        return KEY_NONE;
		}
	}

	// Letter form: ESC [ A, etc.
	switch (final) {
		case 'A': return KEY_UP;
		case 'B': return KEY_DOWN;
		case 'C': return KEY_RIGHT;
		case 'D': return KEY_LEFT;
		case 'H': return KEY_HOME;
		case 'F': return KEY_END;
		default:  return KEY_NONE;
	}
}

// Parse the remainder of an SS3 sequence (the byte after "ESC O"), used by some
// terminals in application-cursor-key mode.
static int ParseSS3() {
	unsigned char b;
	if (!ReadByteTimed(kEscTimeoutMs, &b)) return KEY_NONE;
	switch (b) {
		case 'A': return KEY_UP;
		case 'B': return KEY_DOWN;
		case 'C': return KEY_RIGHT;
		case 'D': return KEY_LEFT;
		case 'H': return KEY_HOME;
		case 'F': return KEY_END;
		default:  return KEY_NONE;
	}
}

int ReadKeyTranslated() {
	unsigned char c;
	ssize_t n;
	do {
		n = read(STDIN_FILENO, &c, 1);
	} while (n < 0 && errno == EINTR);
	if (n != 1) return -1;

	if (c == 127) return KEY_BACKSPACE;   // terminal Backspace key sends DEL
	if (c == 13)  return KEY_RETURN;      // normalize CR (ICRNL usually gives 10)
	if (c != 27)  return (int)c;          // ordinary byte, case preserved

	// ESC: a bare Escape, or the start of a sequence. Peek for a continuation.
	unsigned char b;
	if (!ReadByteTimed(kEscTimeoutMs, &b)) return 27;  // bare Escape
	if (b == '[') return ParseCSI();
	if (b == 'O') return ParseSS3();
	return KEY_NONE;  // ESC + other (e.g. Alt-combo): unrecognized, swallow
}

} // namespace Keyboard
} // namespace MiniScript

#endif // _WIN32 / POSIX
