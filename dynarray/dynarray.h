/*
 * SB - Language
 * By Laman28
 * LIB - Dynamic array
 * Not welcome to use /XD
 */

#ifndef _SBL_LIB_DYNARRAY
#define _SBL_LIB_DYNARRAY

#define DAT_TYPE_CHAR 1
#define DAT_TYPE_STR 2

#include <sys/types.h>

typedef union Data{
    char* dat_char;
    char** dat_str;
}Data;

typedef struct Array{
    short dat_type;
    int length;
    Data* data;
}Array;

extern Array* new_array(int type);

extern void append_char(Array* arr, char c);
extern void append_str(Array* arr, const char* s);

extern void append(Array* arr, char c, const char* s);

extern void delete_array(Array* arr);

extern void set_zero(void* ptr, ssize_t size);

#endif

