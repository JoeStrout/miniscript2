// C++ port of Gardens Point Parser Generator Runtime
// Original Copyright (c) Wayne Kelly, John Gough, QUT 2005-2010
// Ported to C++ for MiniScript 2.0

#include "ShiftReduceParserCode.h"
#include <cstdlib>
#include <cstring>

namespace QUT {
namespace Gppg {

// ==============================================================
// State Implementation
// ==============================================================

State::State() : number(0), ParserTable(nullptr), Goto(nullptr), defaultAction(0) {
}

State::State(std::initializer_list<int> actions) : number(0), Goto(nullptr), defaultAction(0) {
	ParserTable = new Dictionary<int, int>();
	initializeParserTable(actions);
}

State::State(std::initializer_list<int> actions, std::initializer_list<int> goToList)
	: number(0), defaultAction(0) {
	ParserTable = new Dictionary<int, int>();
	Goto = new Dictionary<int, int>();
	initializeParserTable(actions);
	initializeGoto(goToList);
}

State::State(int defaultAction) : number(0), ParserTable(nullptr), Goto(nullptr), defaultAction(defaultAction) {
}

State::State(int defaultAction, std::initializer_list<int> goToList)
	: number(0), ParserTable(nullptr), defaultAction(defaultAction) {
	Goto = new Dictionary<int, int>();
	initializeGoto(goToList);
}

State::~State() {
	if (ParserTable) delete ParserTable;
	if (Goto) delete Goto;
}

void State::initializeParserTable(std::initializer_list<int> actions) {
	auto it = actions.begin();
	while (it != actions.end()) {
		int key = *it++;
		if (it != actions.end()) {
			int value = *it++;
			ParserTable->Add(key, value);
		}
	}
}

void State::initializeGoto(std::initializer_list<int> goToList) {
	auto it = goToList.begin();
	while (it != goToList.end()) {
		int key = *it++;
		if (it != goToList.end()) {
			int value = *it++;
			Goto->Add(key, value);
		}
	}
}

// ==============================================================
// Rule Implementation
// ==============================================================

Rule::Rule() : LeftHandSide(0), RightHandSide(nullptr), RhsLength(0) {
}

Rule::Rule(int left, std::initializer_list<int> right)
	: LeftHandSide(left), RhsLength(static_cast<int>(right.size())) {
	if (RhsLength > 0) {
		RightHandSide = new int[RhsLength];
		int i = 0;
		for (int val : right) {
			RightHandSide[i++] = val;
		}
	} else {
		RightHandSide = nullptr;
	}
}

Rule::~Rule() {
	if (RightHandSide) {
		delete[] RightHandSide;
	}
}

} // namespace Gppg
} // namespace QUT
