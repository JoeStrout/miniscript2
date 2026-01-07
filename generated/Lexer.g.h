// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: Lexer.cs

#pragma once
#include "core_includes.h"
// Hand-written lexer for MiniScript
// Simple expression tokenizer (to be expanded for full MiniScript grammar)


namespace MiniScript {

// FORWARD DECLARATIONS

struct CallInfo;
class CallInfoStorage;
struct VMVis;
class VMVisStorage;
struct Assembler;
class AssemblerStorage;
struct FuncDef;
class FuncDefStorage;
struct App;
class AppStorage;
struct Lexer;
class LexerStorage;

// DECLARATIONS


















class LexerStorage : public AbstractScanner<int, LexLocation>Storage {
	private: string input;
	private: int position;
	public: LexerStorage(string source);
	public: int yylex();
	public: void yyerror(string message, params object[] args);
}; // end of class LexerStorage

struct Lexer : public AbstractScanner<int, LexLocation> {
	public: Lexer(std::shared_ptr<AbstractScanner<int, LexLocation>Storage> stor) : AbstractScanner<int, LexLocation>(stor) {}
	private: LexerStorage* get() { return static_cast<LexerStorage*>(storage.get()); }
	private: string input() { return get()->input; }
	private: void set_input(string _v) { get()->input = _v; }
	private: int position() { return get()->position; }
	private: void set_position(int _v) { get()->position = _v; }
	public: Lexer(string source) : ?!?!?(std::make_shared<LexerStorage>(source)) {}
}; // end of struct Lexer


// INLINE METHODS

} // end of namespace MiniScript
