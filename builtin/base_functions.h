/*
* SB - Language
 * By Laman28
 * Module - builtin
 * Not welcome to use /XD
 */

#ifndef SBLANG_BASE_FUNCTIONS_H
#define SBLANG_BASE_FUNCTIONS_H

#include "../vm/vm.h"

Value builtin_print(VM* vm, Value* args, int arg_count);
Value builtin_input(VM* vm, Value* args, int arg_count);
Value builtin_len(VM* vm, Value* args, int arg_count);
Value builtin_type(VM* vm, Value* args, int arg_count);
Value builtin_exit(VM* vm, Value* args, int arg_count);

void register_builtin_functions(VM* vm);

#endif