// main.cpp
// Entry point that calls TestRunner::Main()

#include "gc.h"
#include "TestRunner.g.h"

int main() {
	gc_init();
	MiniScript::TestRunner::Main();
	gc_shutdown();
	return 0;
}
