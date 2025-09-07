/*
* SB - Language
 * By Laman28
 * Module - builtin
 * Not welcome to use /XD
 */

#ifndef SBLANG_BASE_FUNCTIONS_H
#define SBLANG_BASE_FUNCTIONS_H

#include "../vm/vm.h"

extern char* sep_s;

extern char* double_to_string(double value);
extern _sbValue toString(_sbVM* vm, _sbValue value, bool _repr);
extern const char* v_type(_sbValue value);
extern char* repr(const char* s);
extern void register_builtin_functions(_sbVM* vm);

#endif