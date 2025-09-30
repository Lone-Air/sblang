/*
 * SB - Language
 * By Laman28
 * Compiler - Lexer
 * Not welcome to use /XD
 */

#ifndef _SBL_COMPILER_LEXER
#define _SBL_COMPILER_LEXER

typedef enum _sbTk{
    _sbSym = 1, _sbNum, _sbStr, _sbKey,
    _sbEnd,
    _sbIf, _sbElse, _sbWhile, _sbFor, _sbLoad, _sbFunction, _sbReturn, _sbStruct, _sbGlobal,
    _sbContinue, _sbBreak, _sbUnterminatedLine, _sbGoto
}_sbTk;

typedef struct _sbToken{
    char* tk;
    int column;
    int pos;
    int line;
    int type;
}_sbToken;

extern char backslash(char c);
extern _sbTk _keyword_detect(const char* key);

extern _sbToken* _sbPreLexer(const char* src);
extern _sbToken* _sbLexer(const char* src);

extern void freeTkList(_sbToken* tk);

#endif
