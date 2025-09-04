/*
 * SB - Language
 * By Laman28
 * Compiler - Error
 * Not welcome to use /XD
 */

#ifndef _SBL_ERROR
#define _SBL_ERROR

#include "../parser/parser.h"
#include "../vm/vm.h"

extern bool syntaxErrorDetector;
extern bool errorDetector;

extern void reset_error();
extern void replcs(bool b);
extern void init_repl_check_syntax();

extern void lexError(const char* errinfo, int c, int l);
extern void memoryError(Parser* parser, const char* errinfo);
extern void syntaxError(Parser* parser, const char* format, ...);

extern void expect_token(Parser* parser, const char* expected);
extern void unexpected_token(Parser* parser);
extern void unclosed_delimiter(Parser* parser, const char* delimiter, int start_line, int start_column);
extern void missing_semicolon(Parser* parser);
extern bool require_semicolon(Parser* parser);

/* Set VM error */
extern void vm_error(_sbVM* vm, VMError error, const char* format, ...);

/* Get error type string */
extern const char* vm_error_string(VMError error);

/* Print error information */
extern void vm_print_error(_sbVM* vm);

extern void bytecode_error(const char* format, ...);

#endif