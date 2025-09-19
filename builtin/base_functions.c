/*
 * SB - Language
 * By Laman28
 * Module - builtin
 * Not welcome to use /XD
 */

#include "base_functions.h"
#include "../error/error.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

/* Structure to track visited list addresses for cycle detection */
typedef struct {
    void** addresses;
    size_t count;
    size_t capacity;
} AddressSet;

#ifdef ENABLE_READLINE
#include <readline/readline.h>
#endif

char* sep_s;
_sbVM* current_vm;

#ifdef ENABLE_READLINE

static int rl_tab_event(int count, int key) {
    if (is_empty(rl_line_buffer)) {
        rl_insert_text("\t");
        return 0;
    }

    //rl_complete_internal(0);
    //rl_forced_update_display();

    return rl_complete(count, key);
}

static char* first_x_data(const char* _s, size_t x){
    char* result = (char*)calloc(x+1, sizeof(char));
    assert(result != nullptr);
    for(int i = 0; i < x; i++){
        result[i] = _s[i];
        if(i == x - 1)
            result[i + 1] = '\0';
    }
    return result;
}

static char* rl_generator(const char* part, int state){
    char* _ptr;
    char* _result = nullptr;
    char* _temp;

    int ok = 0;

    if (current_vm->locals) {
        for(int i = 0; i < current_vm->locals->count; i++){
            _ptr = current_vm->locals->vars[i].name;
            _temp = first_x_data(_ptr, strlen(part));
            if(strcmp(_temp, part) == 0 || strcmp(part, "") == 0){
                if(state > ok)
                    ok++;
                else{
                    _result = _s_strdup(_ptr);
                    free(_temp);
                    return _result;
                }
            }
            free(_temp);
        }
    }

    for(int i = 0; i < current_vm->globals.count; i++){
        _ptr = current_vm->globals.vars[i].name;
        _temp = first_x_data(_ptr, strlen(part));
        if(strcmp(_temp, part) == 0 || strcmp(part, "") == 0){
            if(state > ok)
                ok++;
            else{
                _result = _s_strdup(_ptr);
                free(_temp);
                return _result;
            }
        }
        free(_temp);
    }

    return _result;
}

#endif

bool is_empty(const char *str) {
    if (!str) {
        return false;
    }

    for (; *str != '\0'; ++str) {
        if (!isspace((unsigned char)*str)) {
            return false;
        }
    }

    return true;
}

const char* v_type(_sbValue value) {
    switch (value.type) {
        case VAL_NULL:
            return "null";
        case VAL_NUMBER:
            return "number";
        case VAL_STRING:
            return "string";
        case VAL_BOOL:
            return "bool";
        case VAL_FUNCTION:
            return "function";
        case VAL_NATIVE:
            return "native";
        case VAL_STRUCT:
            return "struct";
        case VAL_STRUCT_INSTANCE:
            return "instance";
        case VAL_LIST:
            return "list";
        case VAL_GOTO_BLOCK:
            return "block";
        default:
            return "unknown";
    }
}

/* Initialize address set */
static AddressSet* create_address_set() {
    AddressSet* set = malloc(sizeof(AddressSet));
    if (!set) return NULL;
    
    set->addresses = malloc(sizeof(void*) * 8);
    if (!set->addresses) {
        free(set);
        return NULL;
    }
    
    set->count = 0;
    set->capacity = 8;
    return set;
}

/* Free address set */
static void free_address_set(AddressSet* set) {
    if (set) {
        free(set->addresses);
        free(set);
    }
}

/* Check if address exists in set */
static bool contains_address(AddressSet* set, void* addr) {
    if (!set) return false;
    
    for (size_t i = 0; i < set->count; i++) {
        if (set->addresses[i] == addr) {
            return true;
        }
    }
    return false;
}

/* Add address to set */
static bool add_address(AddressSet* set, void* addr) {
    if (!set) return false;
    
    if (set->count >= set->capacity) {
        size_t new_capacity = set->capacity * 2;
        void** new_addresses = realloc(set->addresses, sizeof(void*) * new_capacity);
        if (!new_addresses) return false;
        
        set->addresses = new_addresses;
        set->capacity = new_capacity;
    }
    
    set->addresses[set->count++] = addr;
    return true;
}

/* Remove address from set */
static void remove_address(AddressSet* set, void* addr) {
    if (!set) return;
    
    for (size_t i = 0; i < set->count; i++) {
        if (set->addresses[i] == addr) {
            /* Move last element to current position */
            if (i < set->count - 1) {
                set->addresses[i] = set->addresses[set->count - 1];
            }
            set->count--;
            break;
        }
    }
}

char *repr(const char *s) {
    size_t len = strlen(s);

    char *buf = malloc(len * 4 + 3);
    if (!buf) return nullptr;

    char quote = '\'';
    if (strchr(s, '\'')) {
        quote = '"';
    }

    char *p = buf;
    *p++ = quote;

    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        switch (c) {
            case '\n': *p++='\\'; *p++='n'; break;
            case '\t': *p++='\\'; *p++='t'; break;
            case '\r': *p++='\\'; *p++='r'; break;
            case '\f': *p++='\\'; *p++='f'; break;
            case '\v': *p++='\\'; *p++='v'; break;
            case '\\': *p++='\\'; *p++='\\'; break;
            case '\'':
                if (quote == '\'') { *p++='\\'; *p++='\''; }
                else *p++='\'';
                break;
            case '"':
                if (quote == '"') { *p++='\\'; *p++='"'; }
                else *p++='"';
                break;
            default:
                if (isprint(c)) {
                    *p++ = c;
                } else {
                    sprintf(p, "\\x%02x", c);
                    p += 4;
                }
        }
    }

    *p++ = quote;
    *p = '\0';
    return buf;
}

/* Internal toString with cycle detection */
static _sbValue toString_internal(_sbVM* vm, _sbValue value, bool _repr, AddressSet* visited) {
    char* result;
    switch (value.type) {
        case VAL_NULL:
            result = "null";
            break;
        case VAL_NUMBER:
            char* _s = double_to_string(value.as.number);
            result = _s_strdup(_s);
            free(_s);
            break;
        case VAL_STRING:
            result = _s_strdup(value.as.string);
            break;
        case VAL_BOOL:
            result = _s_strdup(value.as.boolean ? "true" : "false");
            break;
        case VAL_FUNCTION: {
            const char* source_file = value.as.function->source_code_file ? value.as.function->source_code_file : "<unknown>";
            const char* func_name = value.as.function->name ? value.as.function->name : "<unnamed>";
            char* _s1 = calloc(13 + strlen(func_name) + strlen(source_file), sizeof(char));
            strcpy(_s1, "<function:");
            strcat(_s1, func_name);
            strcat(_s1, ":");
            strcat(_s1, source_file);
            strcat(_s1, ">");
            result = _s_strdup(_s1);
            free(_s1);
            break;
        }
        case VAL_GOTO_BLOCK: {
            const char* block_name = value.as.block.block_name ? value.as.block.block_name : "<unnamed>";
            char* _s1 = calloc(9 + strlen(block_name), sizeof(char));
            strcpy(_s1, "<block:");
            strcat(_s1, block_name);
            strcat(_s1, ">");
            result = _s_strdup(_s1);
            free(_s1);
            break;
        }
        case VAL_NATIVE:
            return create_string(vm,"<native_function>");
        case VAL_STRUCT:
            ssize_t size = 12;
            size += strlen(value.as.struct_def->name);
            for (int i = 0; i < value.as.struct_def->member_count; i++) {
                size += strlen(value.as.struct_def->members[i]);
                if (i != value.as.struct_def->member_count - 1)
                    size += 2;
            }
            size++;
            char* _s3 = calloc(size, sizeof (char));
            strcpy(_s3, "{Struct[");
            strcat(_s3, value.as.struct_def->name);
            strcat(_s3, "]: ");
            for (int i = 0; i < value.as.struct_def->member_count; i++) {
                strcat(_s3, value.as.struct_def->members[i]);
                if (i != value.as.struct_def->member_count - 1)
                    strcat(_s3, ", ");
            }
            strcat(_s3, "}");
            result = _s_strdup(_s3);
            free(_s3);
            break;
        case VAL_STRUCT_INSTANCE:
            ssize_t size1 = 13;
            size1 += strlen(value.as.instance->struct_def->name);
            char* _s4 = calloc(size1, sizeof (char));
            strcpy(_s4, "{Instance[");
            strcat(_s4, value.as.instance->struct_def->name);
            strcat(_s4, "]}");
            result = _s_strdup(_s4);
            free(_s4);
            break;
        case VAL_LIST:
            /* Check for circular reference */
            if (contains_address(visited, value.as.list)) {
                result = _s_strdup("[...]");
                break;
            }
            
            /* Add current list address to visited set */
            if (!add_address(visited, value.as.list)) {
                result = _s_strdup("[<memory error>]");
                break;
            }
            
            char* _s5 = calloc(2, sizeof (char));
            size_t len;
            assert(_s5 != nullptr);
            _s5[0] = '[';
            for (int i = 0; i < value.as.list->count; i++) {
                _sbValue _result;
                if (value.as.list->items[i].type == VAL_STRING)
                    _result = toString_internal(vm, value.as.list->items[i], true, visited);
                else
                    _result = toString_internal(vm, value.as.list->items[i], false, visited);
                if (i > 0) {
                    len = strlen(_s5) + strlen(_result.as.string) + 3;
                    _s5 = realloc(_s5, len * sizeof (char));
                    assert(_s5 != nullptr);
                    strcat(_s5, ", ");
                }
                else {
                    len = strlen(_s5) + strlen(_result.as.string) + 1;
                    _s5 = realloc(_s5, len * sizeof (char));
                    assert(_s5 != nullptr);
                }
                strcat(_s5, _result.as.string);
                _s5[len - 1] = '\0';
            }
            len = strlen(_s5) + 2;
            _s5 = realloc(_s5, len * sizeof (char));
            strcat(_s5, "]");
            result = _s_strdup(_s5);
            free(_s5);
            
            /* Remove current list address from visited set */
            remove_address(visited, value.as.list);
            break;
        default:
            result = _s_strdup("<?unknown type>");
            break;
    }

    _sbValue result_v;
    if (_repr) {
        char* repr_s = repr(result);
        free(result);
        result_v = create_string(vm, repr_s);
        free(repr_s);
    }
    else {
        result_v = create_string(vm, result);
        free(result);
    }

    return result_v;
}

_sbValue toString(_sbVM* vm, _sbValue value, bool _repr) {
    AddressSet* visited = create_address_set();
    if (!visited) {
        return create_string(vm, "<memory error>");
    }
    
    _sbValue result = toString_internal(vm, value, _repr, visited);
    free_address_set(visited);
    return result;
}

static char* c2s(char c) {
    char* s = calloc(2, sizeof(char));
    *s = c;
    return s;
}

/* Type transferation */
char* double_to_string(double value) {
    // Temporary buffer
    char temp[350];
    int len;
    if (value == (long long int)value) {
        len = snprintf(temp, 350, "%lld", (long long int)value);
    }
    else {
        len = snprintf(temp, 350, "%.6g", value);
    }

    // Real length of buffer
    char* result = (char*)malloc((len + 1) * sizeof(char));
    if (result == nullptr) {
        return nullptr;
    }

    strcpy(result, temp);
    return result;
}

/* Built-in print function */
static _sbValue builtin_print(_sbVM* vm, _sbValue* args, int arg_count) {
    for (int i = 0; i < arg_count; i++) {
        switch (args[i].type) {
            case VAL_NULL:
                printf("null");
                break;
            case VAL_NUMBER:
                /* Check if it's an integer */
                double num = args[i].as.number;
                //printf("--- PRINT DEBUG: %d, %lf, %lld\n", num == (long long int)num, num, (long long int)num);
                if (num == (long long int)num) {
                    printf("%lld", (long long int)num);
                } else {
                    printf("%.6g", num);
                }
                break;
            case VAL_STRING:
                printf("%s", args[i].as.string);
                break;
            case VAL_BOOL:
                printf("%s", args[i].as.boolean ? "true" : "false");
                break;
            default:
                printf("%s", toString(vm, args[i], false).as.string);
                break;
        }

        if (i < arg_count - 1) {
            printf("%s", sep_s);
        }
    }
    //printf("\n");

    return create_null();
}

/* Built-in input function */
static _sbValue builtin_input(_sbVM* vm, _sbValue* args, int arg_count) {
    /* Print prompt if provided */
    if (arg_count > 0 && args[0].type == VAL_STRING) {
        printf("%s", args[0].as.string);
        fflush(stdout);
    }

#ifndef ENABLE_READLINE
    char* buffer;
    buffer = malloc(sizeof(char));
    int length = 0;
    int ch;
    while (true) {
        ch = getchar();

        if(ch=='\n'||ch=='\0') break;

        if (ch == EOF) {
            putchar('\n');
            free(buffer);
            return create_null();
        }

        buffer = realloc(buffer, (length + 1) * sizeof(char));
        buffer[length++] = ch;
    }

    buffer[length] = '\0';
    _sbValue result = create_string(vm, buffer);
    free(buffer);

    return result;

#else
    char* buffer = readline("");
    if (buffer != nullptr) {
        _sbValue result = create_string(vm, buffer);
        free(buffer);
        return result;
    }

    return create_null();
#endif
}

/* Built-in setsep function */
static _sbValue builtin_setsep(_sbVM* vm, _sbValue* args, int arg_count) {
    if (arg_count != 1) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "setsep() expects exactly 1 argument");
        return create_null();
    }

    if (args[0].type != VAL_STRING) {
        vm_error(vm, VM_TYPE_ERROR, "setsep() expects a string");
        return create_null();
    }

    free(sep_s);
    sep_s = _s_strdup(args[0].as.string);
    return create_null();
}

/* Built-in len function */
static _sbValue builtin_len(_sbVM* vm, _sbValue* args, int arg_count) {
    if (arg_count != 1) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "len() expects exactly 1 argument");
        return create_null();
    }

    switch (args[0].type) {
        case VAL_STRING:
            if (args[0].as.string) {
                return create_number((double)strlen(args[0].as.string));
            }
            return create_number(0);

        case VAL_LIST:
            if (args[0].as.list) {
                return create_number((double)args[0].as.list->count);
            }
            return create_number(0);

        default:
            vm_error(vm, VM_TYPE_ERROR, "len() expects a string or list");
            return create_null();
    }
}

/* Built-in dupe function */
static _sbValue builtin_dupe(_sbVM* vm, _sbValue* args, int arg_count) {
    if (arg_count != 1) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "dupe() expects exactly 1 argument");
        return create_null();
    }

    return copy_value(vm, args[0]);
}

/* Built-in append_list function */
static _sbValue builtin_append_list(_sbVM* vm, _sbValue* args, int arg_count) {
    if (arg_count != 2) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "append_list() expects exactly 2 argument");
        return create_null();
    }

    if (args[0].type == VAL_LIST) {
        if (!append_list(vm, args[0].as.list, args[1])) {
            vm_error(vm, VM_MEMORY_ERROR, "append_list(): cannot expand size of list");
            return create_null();
        }
        return create_null();
    }
    else{
        vm_error(vm, VM_TYPE_ERROR, "append_list() expects a list");
        return create_null();
    }
}

/* Built-in insert_list function */
static _sbValue builtin_insert_list(_sbVM* vm, _sbValue* args, int arg_count) {
    if (arg_count != 3) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "insert_list() expects exactly 3 argument");
        return create_null();
    }

    if (args[0].type == VAL_LIST) {
        if (args[1].type != VAL_NUMBER) {
            vm_error(vm, VM_TYPE_ERROR, "insert_list(): invalid index");
            return create_null();
        }

        int index = (int)args[1].as.number;
        if (index < 0) {
            index = args[0].as.list->count + index;
        }

        if (index < 0 || index >= args[0].as.list->count) {
            vm_error(vm, VM_INDEX_OUT_OF_BOUNDS, "insert_list(): index out of bounds");
            return create_null();
        }

        if (!insert_list(vm, args[0].as.list, index, args[2])) {
            vm_error(vm, VM_MEMORY_ERROR, "insert_list(): cannot expand size of list");
            return create_null();
        }
        return create_null();
    }
    else{
        vm_error(vm, VM_TYPE_ERROR, "insert_list() expects a list");
        return create_null();
    }
}

/* Built-in pop_list function */
static _sbValue builtin_pop_list(_sbVM* vm, _sbValue* args, int arg_count) {
    if (arg_count != 1) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "append_list() expects exactly 1 argument");
        return create_null();
    }

    if (args[0].type == VAL_LIST) {
        pop_list(args[0].as.list);
        return create_null();
    }
    else{
        vm_error(vm, VM_TYPE_ERROR, "append_list() expects a list");
        return create_null();
    }
}

/* Built-in ord function */
static _sbValue builtin_ord(_sbVM* vm, _sbValue* args, int arg_count) {
    if (arg_count != 1) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "ord() expects exactly 1 argument");
        return create_null();
    }

    if (args[0].type == VAL_STRING) {
        if (strlen(args[0].as.string) != 1) {
            vm_error(vm, VM_TYPE_ERROR, "ord() expects single character, but string of length %d found", strlen(args[0].as.string));
            return create_null();
        }
        int result = (int)args[0].as.string[0];
        return create_number(result);
    }
    else {
        vm_error(vm, VM_TYPE_ERROR, "ord() expects a string");
        return create_null();
    }
}

/* Built-in chr function */
static _sbValue builtin_chr(_sbVM* vm, _sbValue* args, int arg_count) {
    if (arg_count != 1) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "chr() expects exactly 1 argument");
        return create_null();
    }

    if (args[0].type == VAL_NUMBER) {
        char result_str[2];
        result_str[0] = (char)args[0].as.number;
        result_str[1] = '\0';
        return create_string(vm, result_str);
    }
    else {
        vm_error(vm, VM_TYPE_ERROR, "chr() expects a number");
        return create_null();
    }
}

/* Built-in type function */
static _sbValue builtin_type(_sbVM* vm, _sbValue* args, int arg_count) {
    if (arg_count != 1) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "type() expects exactly 1 argument");
        return create_null();
    }

    _sbValue result = create_string(vm, v_type(args[0]));

    return result;
}

/* Built-in address function */
static _sbValue builtin_address(_sbVM* vm, _sbValue* args, int arg_count) {
    if (arg_count != 1) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "address() expects exactly 1 argument");
        return create_null();
    }

    uintptr_t address = 0;
    switch (args[0].type) {
        case VAL_STRING:
            address = (uintptr_t)args[0].as.string;
            break;
        case VAL_LIST:
            address = (uintptr_t)args[0].as.list;
            break;
        case VAL_STRUCT_INSTANCE:
            address = (uintptr_t)args[0].as.instance;
            break;
        case VAL_FUNCTION:
            address = (uintptr_t)args[0].as.function;
            break;
        default:
            address = (uintptr_t)&args[0].as;
            break;
    }

    return create_number((double)address);
}

/* Built-in exit function */
static _sbValue builtin_exit(_sbVM* vm, _sbValue* args, int arg_count) {
    int exit_code = 0;

    if (arg_count > 0 && args[0].type == VAL_NUMBER) {
        exit_code = (int)args[0].as.number;
    }

    /* Stop VM execution */
    vm->running = false;

    /* For now, we'll just return null and handle exit in the main loop */
    return create_null();
}

/* Built-in toString function */
static _sbValue builtin_toString(_sbVM* vm, _sbValue* args, int arg_count) {
    if (arg_count != 1) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "toString() expects exactly 1 argument");
        return create_null();
    }

    return toString(vm, args[0], false);
}

/* Built-in toNumber function */
static _sbValue builtin_toNumber(_sbVM* vm, _sbValue* args, int arg_count) {
    if (arg_count != 1) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "toNumber() expects exactly 1 argument");
        return create_null();
    }

    if (args[0].type != VAL_STRING) {
        vm_error(vm, VM_TYPE_ERROR, "toNumber(): argument should be a string");
        return create_null();
    }

    return create_number(atof(args[0].as.string));
}

/* Built-in repr function */
static _sbValue builtin_repr(_sbVM* vm, _sbValue* args, int arg_count) {
    if (arg_count != 1) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "repr() expects exactly 1 argument");
        return create_null();
    }

    return toString(vm, args[0], true);
}

/* Built-in system function */
static _sbValue builtin_system(_sbVM* vm, _sbValue* args, int arg_count) {
    if (arg_count != 1) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "system() expects exactly 1 argument");
        return create_null();
    }

    if (args[0].type != VAL_STRING) {
        vm_error(vm, VM_TYPE_ERROR, "system(): argument should be a string");
        return create_null();
    }

    return create_number(system(args[0].as.string));
}

static void register_builtin_variables(_sbVM* vm) {
    //printf("DEBUG: Starting register_builtin_variables\n");
    
    // Use create_string to properly allocate the string
    //printf("DEBUG: About to create EOL\n");
    _sbValue eol_val = create_string(vm,"\n");
    //printf("DEBUG: Created EOL, about to define global\n");
    vm_define_global(vm, "EOL", eol_val);
    //printf("DEBUG: EOL global defined\n");
    
    vm_define_global(vm, "true", create_bool(true));
    vm_define_global(vm, "false", create_bool(false));
    vm_define_global(vm, "null", create_null());
    
    //printf("DEBUG: Finished register_builtin_variables\n");
}

/* Register built-in functions */
void register_builtin_functions(_sbVM* vm) {
    current_vm = vm;

#ifdef ENABLE_READLINE
    //rl_bind_key('\t', rl_tab_event);
    rl_completion_entry_function = rl_generator;
#endif

    sep_s = _s_strdup(" ");

    vm_register_native(vm, "print", builtin_print);
    vm_register_native(vm, "input", builtin_input);
    vm_register_native(vm, "system", builtin_system);
    vm_register_native(vm, "setsep", builtin_setsep);
    vm_register_native(vm, "len", builtin_len);
    vm_register_native(vm, "dupe", builtin_dupe);
    vm_register_native(vm, "append_list", builtin_append_list);
    vm_register_native(vm, "insert_list", builtin_insert_list);
    vm_register_native(vm, "pop_list", builtin_pop_list);

    vm_register_native(vm, "ord", builtin_ord);
    vm_register_native(vm, "chr", builtin_chr);

    vm_register_native(vm, "type", builtin_type);
    vm_register_native(vm, "address", builtin_address);
    vm_register_native(vm, "toString", builtin_toString);
    vm_register_native(vm, "toNumber", builtin_toNumber);
    vm_register_native(vm, "repr", builtin_repr);
    vm_register_native(vm, "exit", builtin_exit);

    register_builtin_variables(vm);
}

