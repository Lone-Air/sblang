/*
 * SB - Language
 * By Laman28
 * Compiler - Error
 * Not welcome to use /XD
 */

#ifndef _SBL_ERROR
#define _SBL_ERROR

#include "../parser/parser.h"

extern bool syntaxErrorDetector;

extern void reset_error();

extern void lexError(const char* errinfo, int c, int l);
extern void memoryError(Parser* parser, const char* errinfo);
extern void syntaxError(Parser* parser, const char* format, ...);

extern void expect_token(Parser* parser, const char* expected);
extern void unexpected_token(Parser* parser);
extern void unclosed_delimiter(Parser* parser, const char* delimiter, int start_line, int start_column);
extern void missing_semicolon(Parser* parser);
extern bool require_semicolon(Parser* parser);

extern void bytecode_error(const char* format, ...);

#endif