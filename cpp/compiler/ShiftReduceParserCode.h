// C++ port of Gardens Point Parser Generator Runtime
// Original Copyright (c) Wayne Kelly, John Gough, QUT 2005-2010
// Ported to C++ for MiniScript 2.0

#pragma once
#include "../core/MemPool.h"
#include "../core/CS_List.h"
#include "../core/CS_Dictionary.h"
#include "../core/CS_String.h"
#include <exception>
#include <initializer_list>

namespace QUT {
namespace Gppg {

// Forward declarations
template<typename TSpan> class IMerge;
class LexLocation;
template<typename TValue, typename TSpan> class AbstractScanner;
class State;
class Rule;
template<typename T> class PushdownPrefixState;
template<typename TValue, typename TSpan> class ShiftReduceParser;

// ==============================================================
// IMerge Interface
// ==============================================================
template<typename TSpan>
class IMerge {
public:
	virtual ~IMerge() {}
	virtual TSpan Merge(const TSpan& last) = 0;
};

// ==============================================================
// LexLocation - default location tracking class
// ==============================================================
class LexLocation : public IMerge<LexLocation> {
private:
	int startLine;
	int startColumn;
	int endLine;
	int endColumn;

public:
	// Constructors
	LexLocation() : startLine(0), startColumn(0), endLine(0), endColumn(0) {}
	LexLocation(int sl, int sc, int el, int ec)
		: startLine(sl), startColumn(sc), endLine(el), endColumn(ec) {}

	// Properties
	int StartLine() const { return startLine; }
	int StartColumn() const { return startColumn; }
	int EndLine() const { return endLine; }
	int EndColumn() const { return endColumn; }

	// IMerge implementation
	virtual LexLocation Merge(const LexLocation& last) override {
		return LexLocation(this->startLine, this->startColumn, last.endLine, last.endColumn);
	}
};

// ==============================================================
// AbstractScanner - scanner base class
// ==============================================================
template<typename TValue, typename TSpan>
class AbstractScanner {
public:
	TValue yylval;        // Lexical value set by scanner
	TSpan yylloc;         // Current location

	virtual ~AbstractScanner() {}

	// Main scanner entry point
	virtual int yylex() = 0;

	// Error reporting
	virtual void yyerror(const String& message) {}
};

// ==============================================================
// State - parser state representation
// ==============================================================
class State {
public:
	int number;                              // State index
	Dictionary<int, int>* ParserTable;       // Terminal -> ParseAction
	Dictionary<int, int>* Goto;              // NonTerminal -> State
	int defaultAction;                       // Default action for this state

	// Constructors
	State();
	State(std::initializer_list<int> actions);
	State(std::initializer_list<int> actions, std::initializer_list<int> goToList);
	State(int defaultAction);
	State(int defaultAction, std::initializer_list<int> goToList);

	~State();

private:
	void initializeParserTable(std::initializer_list<int> actions);
	void initializeGoto(std::initializer_list<int> goToList);
};

// ==============================================================
// Rule - production rule representation
// ==============================================================
class Rule {
public:
	int LeftHandSide;     // LHS non-terminal symbol
	int* RightHandSide;   // RHS symbols array
	int RhsLength;        // Length of RHS array

	// Constructor
	Rule();
	Rule(int left, std::initializer_list<int> right);
	~Rule();
};

// ==============================================================
// PushdownPrefixState - stack for parser
// ==============================================================
template<typename T>
class PushdownPrefixState {
private:
	List<T> array;

public:
	PushdownPrefixState(uint8_t poolNum = 0) : array(poolNum) {
		array.Add(T());  // Initialize with one default element
	}

	// Indexer
	T& operator[](int index) {
		return array[index];
	}

	const T& operator[](int index) const {
		return array[index];
	}

	// Properties
	int Depth() const {
		return array.Count();
	}

	// Stack operations
	void Push(const T& value) {
		array.Add(value);
	}

	T Pop() {
		if (array.Count() > 0) {
			T result = array[array.Count() - 1];
			array.RemoveAt(array.Count() - 1);
			return result;
		}
		return T();
	}

	T TopElement() const {
		if (array.Count() > 0) {
			return array[array.Count() - 1];
		}
		return T();
	}

	bool IsEmpty() const {
		return array.Count() == 0;
	}
};

// ==============================================================
// Parser exceptions for control flow
// ==============================================================
class AcceptException : public std::exception {
public:
	const char* what() const noexcept override { return "Parser accept"; }
};

class AbortException : public std::exception {
public:
	const char* what() const noexcept override { return "Parser abort"; }
};

class ErrorException : public std::exception {
public:
	const char* what() const noexcept override { return "Parser error"; }
};

// ==============================================================
// ShiftReduceParser - main parser template class
// ==============================================================
template<typename TValue, typename TSpan>
class ShiftReduceParser {
protected:
	AbstractScanner<TValue, TSpan>* scanner;

	// Current semantic value and location
	TValue CurrentSemanticValue;
	TSpan CurrentLocationSpan;
	int NextToken;

private:
	TSpan LastSpan;
	State* FsaState;
	bool recovering;
	int tokensSinceLastError;

	PushdownPrefixState<State*> StateStack;
	PushdownPrefixState<TValue> valueStack;
	PushdownPrefixState<TSpan> locationStack;

	int errorToken;
	int endOfFileToken;
	String* nonTerminals;
	int nonTerminalsCount;
	State** states;
	int statesCount;
	Rule* rules;
	int rulesCount;

protected:
	// Properties
	AbstractScanner<TValue, TSpan>* Scanner() { return scanner; }
	void SetScanner(AbstractScanner<TValue, TSpan>* value) { scanner = value; }

	PushdownPrefixState<TValue>& ValueStack() { return valueStack; }
	PushdownPrefixState<TSpan>& LocationStack() { return locationStack; }

	bool YYRecovering() const { return recovering; }

	// Initialization methods for derived classes
	void InitRules(Rule* rulesArray, int count) {
		rules = rulesArray;
		rulesCount = count;
	}

	void InitStates(State** statesArray, int count) {
		states = statesArray;
		statesCount = count;
	}

	void InitSpecialTokens(int err, int end) {
		errorToken = err;
		endOfFileToken = end;
	}

	void InitNonTerminals(String* names, int count) {
		nonTerminals = names;
		nonTerminalsCount = count;
	}

	// Control flow methods
	static void YYAccept() {
		throw AcceptException();
	}

	static void YYAbort() {
		throw AbortException();
	}

	static void YYError() {
		throw ErrorException();
	}

	// YACC-like utility methods
	void yyclearin() {
		NextToken = 0;
	}

	void yyerrok() {
		recovering = false;
	}

	// Abstract methods that derived classes must implement
	virtual void Initialize() = 0;
	virtual void DoAction(int actionNumber) = 0;
	virtual String TerminalToString(int terminal) = 0;

	// Utility methods
	static String CharToString(char input);
	String SymbolToString(int symbol);

public:
	// Constructor
	ShiftReduceParser(AbstractScanner<TValue, TSpan>* scanner);
	virtual ~ShiftReduceParser();

	// Main parse method
	bool Parse();

private:
	// Internal parsing methods
	void Shift(int stateIndex);
	void Reduce(int ruleNumber);
	bool ErrorRecovery();
	void ReportError();
	void ShiftErrorToken();
	bool FindErrorRecoveryState();
	bool DiscardInvalidTokens();

	// Debug methods (can be no-ops in release)
	void DisplayStack();
	void DisplayRule(int ruleNumber);
	void DisplayProduction(const Rule& rule);
};

// ==============================================================
// ShiftReduceParser Template Implementation
// ==============================================================

template<typename TValue, typename TSpan>
ShiftReduceParser<TValue, TSpan>::ShiftReduceParser(AbstractScanner<TValue, TSpan>* scanner)
	: scanner(scanner), NextToken(0), FsaState(nullptr),
	  recovering(false), tokensSinceLastError(0),
	  errorToken(0), endOfFileToken(0),
	  nonTerminals(nullptr), nonTerminalsCount(0),
	  states(nullptr), statesCount(0),
	  rules(nullptr), rulesCount(0) {
	CurrentSemanticValue = TValue();
	CurrentLocationSpan = TSpan();
	LastSpan = TSpan();
}

template<typename TValue, typename TSpan>
ShiftReduceParser<TValue, TSpan>::~ShiftReduceParser() {
	// Note: We don't own scanner, states, rules, or nonTerminals
	// Those are typically static data from the generated parser
}

template<typename TValue, typename TSpan>
bool ShiftReduceParser<TValue, TSpan>::Parse() {
	Initialize();  // Allow derived classes to set up rules, states, etc.

	NextToken = 0;
	FsaState = states[0];

	StateStack.Push(FsaState);
	valueStack.Push(CurrentSemanticValue);
	locationStack.Push(CurrentLocationSpan);

	while (true) {
		int action = FsaState->defaultAction;

		if (FsaState->ParserTable != nullptr) {
			if (NextToken == 0) {
				LastSpan = scanner->yylloc;
				NextToken = scanner->yylex();
			}

			if (FsaState->ParserTable->ContainsKey(NextToken)) {
				action = (*FsaState->ParserTable)[NextToken];
			}
		}

		if (action > 0) {  // shift
			Shift(action);
		} else if (action < 0) {  // reduce
			try {
				Reduce(-action);
				if (action == -1) {  // accept
					return true;
				}
			} catch (const AbortException&) {
				return false;
			} catch (const AcceptException&) {
				return true;
			} catch (const ErrorException&) {
				if (!ErrorRecovery()) {
					return false;
				} else {
					throw;  // Rethrow if error recovery succeeded
				}
			}
		} else if (action == 0 && !ErrorRecovery()) {  // error
			return false;
		}
	}
}

template<typename TValue, typename TSpan>
void ShiftReduceParser<TValue, TSpan>::Shift(int stateIndex) {
	FsaState = states[stateIndex];

	valueStack.Push(scanner->yylval);
	StateStack.Push(FsaState);
	locationStack.Push(scanner->yylloc);

	if (recovering) {
		if (NextToken != errorToken) {
			tokensSinceLastError++;
		}
		if (tokensSinceLastError > 5) {
			recovering = false;
		}
	}

	if (NextToken != endOfFileToken) {
		NextToken = 0;
	}
}

template<typename TValue, typename TSpan>
void ShiftReduceParser<TValue, TSpan>::Reduce(int ruleNumber) {
	const Rule& rule = rules[ruleNumber];
	int rhLen = rule.RhsLength;

	// Default actions based on production length
	if (rhLen == 1) {
		CurrentSemanticValue = valueStack.TopElement();
		CurrentLocationSpan = locationStack.TopElement();
	} else if (rhLen == 0) {
		CurrentSemanticValue = TValue();
		// Merge scanner location with last span for empty production
		if (&scanner->yylloc != nullptr && &LastSpan != nullptr) {
			CurrentLocationSpan = scanner->yylloc.Merge(LastSpan);
		} else {
			CurrentLocationSpan = TSpan();
		}
	} else {
		// Default: $$ = $1
		CurrentSemanticValue = valueStack[locationStack.Depth() - rhLen];
		// Default: @$ = @1.Merge(@N)
		TSpan at1 = locationStack[locationStack.Depth() - rhLen];
		TSpan atN = locationStack[locationStack.Depth() - 1];
		CurrentLocationSpan = at1.Merge(atN);
	}

	// Execute semantic action
	DoAction(ruleNumber);

	// Pop RHS symbols from stacks
	for (int i = 0; i < rule.RhsLength; i++) {
		StateStack.Pop();
		valueStack.Pop();
		locationStack.Pop();
	}

	FsaState = StateStack.TopElement();

	// Follow goto transition
	if (FsaState->Goto != nullptr && FsaState->Goto->ContainsKey(rule.LeftHandSide)) {
		FsaState = states[(*FsaState->Goto)[rule.LeftHandSide]];
	}

	StateStack.Push(FsaState);
	valueStack.Push(CurrentSemanticValue);
	locationStack.Push(CurrentLocationSpan);
}

template<typename TValue, typename TSpan>
bool ShiftReduceParser<TValue, TSpan>::ErrorRecovery() {
	bool discard;

	if (!recovering) {
		ReportError();
	}

	if (!FindErrorRecoveryState()) {
		return false;
	}

	ShiftErrorToken();
	discard = DiscardInvalidTokens();
	recovering = true;
	tokensSinceLastError = 0;
	return discard;
}

template<typename TValue, typename TSpan>
void ShiftReduceParser<TValue, TSpan>::ReportError() {
	String errorMsg = String("Syntax error, unexpected ") + TerminalToString(NextToken);

	if (FsaState->ParserTable != nullptr && FsaState->ParserTable->Count() < 7) {
		bool first = true;
		for (int terminal : FsaState->ParserTable->GetKeys()) {
			if (first) {
				errorMsg = errorMsg + ", expecting ";
			} else {
				errorMsg = errorMsg + ", or ";
			}
			errorMsg = errorMsg + TerminalToString(terminal);
			first = false;
		}
	}

	scanner->yyerror(errorMsg);
}

template<typename TValue, typename TSpan>
void ShiftReduceParser<TValue, TSpan>::ShiftErrorToken() {
	int old_next = NextToken;
	NextToken = errorToken;

	Shift((*FsaState->ParserTable)[NextToken]);

	NextToken = old_next;
}

template<typename TValue, typename TSpan>
bool ShiftReduceParser<TValue, TSpan>::FindErrorRecoveryState() {
	while (true) {
		if (FsaState->ParserTable != nullptr &&
		    FsaState->ParserTable->ContainsKey(errorToken) &&
		    (*FsaState->ParserTable)[errorToken] > 0) {  // shift
			return true;
		}

		StateStack.Pop();
		valueStack.Pop();
		locationStack.Pop();

		if (StateStack.IsEmpty()) {
			return false;
		} else {
			FsaState = StateStack.TopElement();
		}
	}
}

template<typename TValue, typename TSpan>
bool ShiftReduceParser<TValue, TSpan>::DiscardInvalidTokens() {
	int action = FsaState->defaultAction;

	if (FsaState->ParserTable != nullptr) {
		while (true) {
			if (NextToken == 0) {
				NextToken = scanner->yylex();
			}

			if (NextToken == endOfFileToken) {
				return false;
			}

			if (FsaState->ParserTable->ContainsKey(NextToken)) {
				action = (*FsaState->ParserTable)[NextToken];
			}

			if (action != 0) {
				return true;
			} else {
				NextToken = 0;
			}
		}
	} else if (recovering && tokensSinceLastError == 0) {
		// Panic mode: discard tokens to break error recovery loop
		if (NextToken == endOfFileToken) {
			return false;
		}
		NextToken = 0;
		return true;
	} else {
		return true;
	}
}

template<typename TValue, typename TSpan>
String ShiftReduceParser<TValue, TSpan>::CharToString(char input) {
	switch (input) {
		case '\a': return String("'\\a'");
		case '\b': return String("'\\b'");
		case '\f': return String("'\\f'");
		case '\n': return String("'\\n'");
		case '\r': return String("'\\r'");
		case '\t': return String("'\\t'");
		case '\v': return String("'\\v'");
		case '\0': return String("'\\0'");
		default: {
			char buf[4] = {'\'', input, '\'', '\0'};
			return String(buf);
		}
	}
}

template<typename TValue, typename TSpan>
String ShiftReduceParser<TValue, TSpan>::SymbolToString(int symbol) {
	if (symbol < 0 && nonTerminals != nullptr) {
		int index = -symbol - 1;
		if (index >= 0 && index < nonTerminalsCount) {
			return nonTerminals[index];
		}
	}
	return TerminalToString(symbol);
}

template<typename TValue, typename TSpan>
void ShiftReduceParser<TValue, TSpan>::DisplayStack() {
	// Debug output - can be no-op or use stderr
	// fprintf(stderr, "State stack is now:");
	// for (int i = 0; i < StateStack.Depth(); i++) {
	//     fprintf(stderr, " %d", StateStack[i]->number);
	// }
	// fprintf(stderr, "\n");
}

template<typename TValue, typename TSpan>
void ShiftReduceParser<TValue, TSpan>::DisplayRule(int ruleNumber) {
	// Debug output - can be no-op
	// fprintf(stderr, "Reducing stack by rule %d, ", ruleNumber);
	// DisplayProduction(rules[ruleNumber]);
}

template<typename TValue, typename TSpan>
void ShiftReduceParser<TValue, TSpan>::DisplayProduction(const Rule& rule) {
	// Debug output - can be no-op
	// if (rule.RhsLength == 0) {
	//     fprintf(stderr, "/* empty */ ");
	// } else {
	//     for (int i = 0; i < rule.RhsLength; i++) {
	//         fprintf(stderr, "%s ", SymbolToString(rule.RightHandSide[i]).c_str());
	//     }
	// }
	// fprintf(stderr, "-> %s\n", SymbolToString(rule.LeftHandSide).c_str());
}

} // namespace Gppg
} // namespace QUT
