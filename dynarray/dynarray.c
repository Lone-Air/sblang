/*
 * SB - Language
 * By Laman28
 * LIB - Dynamic array
 * Not welcome to use /XD
 */

#include "dynarray.h"
#include <stdlib.h>
#include <string.h>

Array* new_array(int type){
    Array* _newarray;
    _newarray = (Array*)malloc(sizeof(Array));
    set_zero(_newarray, sizeof(Array));
    _newarray->length = 0;
    _newarray->data = (Data*)malloc(sizeof(Data));
    set_zero(_newarray->data, sizeof(Data));
    switch (type){
      case DAT_TYPE_CHAR: // Type: String
        _newarray->data->dat_char = (char*)malloc(2 * sizeof(char)); // First time of allocating
        set_zero(_newarray->data->dat_char, 2 * sizeof(char));
        break;
      case DAT_TYPE_STR: // Type: String List
        _newarray->data->dat_str = (char**)malloc(sizeof(char*)); // First time of allocating for the main pointer
        *(_newarray->data->dat_str) = (char*)malloc(sizeof(char)); // First time of aloocting for the sub-pointer
        set_zero(_newarray->data->dat_str, sizeof(char*));
        break;
    }
    _newarray->dat_type = type;
    return _newarray;
}

void append_char(Array* arr, char c){
    arr->data->dat_char = (char*)realloc(arr->data->dat_char, (arr->length + 2) * sizeof(char));
    // First time: mem = 1 + 1 = 2. So, it should be allocated for 2 bytes
    arr->data->dat_char[arr->length] = c;

    arr->data->dat_char[arr->length + 1] = '\0'; // End of single string

    arr->length ++;
}

void append_str(Array* arr, const char* s){
    arr->data->dat_str = (char**)realloc(arr->data->dat_str, (arr->length + 2) * sizeof(char*)); 
    // First time: mem = 1 + 1 = 2. So, it should be allocted for twice of `sizeof(char*)`

    arr->data->dat_str[arr->length] = (char*)realloc(arr->data->dat_str[arr->length], (strlen(s) + 1) * sizeof(char));
    set_zero(arr->data->dat_str[arr->length], (strlen(s) + 1) * sizeof(char));
    strcpy(arr->data->dat_str[arr->length], s); // Copy the source string to the sub-pointer

    arr->data->dat_str[arr->length + 1] = (char*)malloc(sizeof(char)); // Allocate for next pointer

    arr->length ++;
}


void set_zero(void* ptr, ssize_t size){
    memset(ptr, 0, size);
}

void delete_array(Array* arr){
    switch (arr->dat_type){
      case DAT_TYPE_CHAR:
          free(arr->data->dat_char);
          break;
      case DAT_TYPE_STR:
          for(int i=0; i <= arr->length; i++){
              free(arr->data->dat_str[i]); // Free the sub-pointer
          }
          free(arr->data->dat_str); // Free the main pointer
          break;
    }
    free(arr->data);
    free(arr); // Free the structures
}

void append(Array* arr, char c, const char* s){
    switch (arr->dat_type){
      case DAT_TYPE_CHAR:
        append_char(arr, c);
        break;
      case DAT_TYPE_STR:
        append_str(arr, s);
        break;
    }
}

#ifdef _SBL_LIB_DYNARRAY_TEST

#include <stdio.h>

int main(){
    Array* test1 = new_array(DAT_TYPE_CHAR);
    append(test1, 'a', NULL);
    append(test1, 'b', NULL);
    fprintf(stdout, "test1: %s\n", test1->data->dat_char);
    delete_array(test1);

    Array* test2 = new_array(DAT_TYPE_STR);
    append(test2, 0, "Str1");
    append(test2, 0, "Str2");
    for(int i=0; i < test2->length; i++){
        fprintf(stdout, "test2[%d]: %s\n", i, test2->data->dat_str[i]);
    }
    delete_array(test2);
    return 0;
}

#endif
