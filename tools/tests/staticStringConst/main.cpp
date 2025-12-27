#include <iostream>
#include "expected/Op.g.h"

using namespace MiniScript;

int main() {
	std::cout << "Op.PLUS: " << Op::PLUS << std::endl;
	std::cout << "Op.MINUS: " << Op::MINUS << std::endl;
	std::cout << "Op.TIMES: " << Op::TIMES << std::endl;
	return 0;
}
