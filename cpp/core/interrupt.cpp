// interrupt.cpp
//
// Implementation of the user-interrupt API declared in interrupt.h.

#include "interrupt.h"
#include <csignal>

namespace MiniScript {
namespace Interrupt {

static volatile sig_atomic_t s_pending = 0;
static bool s_enabled = false;

static void Handler(int sig) {
	s_pending = 1;
	#ifdef _WIN32
	// The Windows CRT resets the disposition to SIG_DFL before calling us.
	signal(sig, Handler);
	#else
	(void)sig;
	#endif
}

void Enable() {
	if (s_enabled) return;
	#ifdef _WIN32
	signal(SIGINT, Handler);
	#else
	struct sigaction sa;
	sa.sa_handler = Handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;	// deliberately no SA_RESTART: let blocking reads see EINTR
	sigaction(SIGINT, &sa, nullptr);
	#endif
	s_enabled = true;
}

bool Enabled() {
	return s_enabled;
}

bool Pending() {
	return s_pending != 0;
}

void Clear() {
	s_pending = 0;
}

} // namespace Interrupt
} // namespace MiniScript
