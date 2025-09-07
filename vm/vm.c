/*
 * SB - Language
 * By Laman28
 * Virtual Machine Implementation
 * Not welcome to use /XD
 */

#ifndef _WIN32
#define _GNU_SOURCE
#endif

#include "vm.h"
#include "../error/error.h"
#include "../lexer/lexer.h"
#include "../parser/parser.h"
#include "../builtin/base_functions.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <stddef.h>

#ifdef _WIN32
#include <windows.h>
#define SHARED_LIB_EXT ".dll"
#define PATH_MAX MAX_PATH
#else
#include <dlfcn.h>
#include <unistd.h>
#include <limits.h>
#include <libgen.h>
#include <sys/stat.h>
#define SHARED_LIB_EXT ".so"
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#endif

//static bool append_module_instructions(_sbVM* vm, BytecodeGenerator* gen, size_t offset);

/* ========== No skipping repeated seperator: strtok ========== */
char* _no_skip_strtok(char* str, const char* delim){
    static char* next_token = nullptr;  // Save the starting position for the next call
    static bool finished = false;       // Flag indicating whether processing is complete
    char* token_start;

    // If a new string is passed in, reset the state
    if (str != nullptr) {
        next_token = str;
        finished = false;
    }

    // Return NULL if processing is already complete
    if (finished || next_token == nullptr) {
        return nullptr;
    }

    // Record the start position of the current token
    token_start = next_token;

    // Find the position of the next delimiter
    char* delim_pos = strpbrk(next_token, delim);

    if (delim_pos != nullptr) {
        // Replace the delimiter with '\0'
        *delim_pos = '\0';
        // Update next_token to point to the position after the delimiter
        next_token = delim_pos + 1;
    } else {
        // No delimiter found - this is the last token
        finished = true;
    }

    return token_start;
}

/* ========== Variable Table Management Functions ========== */

/* Forward declaration */
static void free_variable_table_gc(_sbVM* vm, _sbVariableTable* table);
static void free_variable_table_gc_safe(_sbVM* vm, _sbVariableTable* table);
static void gc_free_data_by_type(_sbVM* vm, _sbValueType type, void* data);
static bool gc_remove_and_free(_sbVM* vm, void* ptr);

/* Initialize variable table */
static void init_variable_table(_sbVariableTable* table) {
    table->vars = nullptr;
    table->count = 0;
    table->capacity = 0;
}

/* Free variable table and all its variables */
static void free_variable_table(_sbVariableTable* table) {
    free_variable_table_gc(nullptr, table);
}

/* Free variable table and all its variables (GC-aware version) */
static void free_variable_table_gc(_sbVM* vm, _sbVariableTable* table) {
    if (!table) return;

    /* Free each variable's name and value */
    for (size_t i = 0; i < table->count; i++) {
        if (table->vars[i].name) free(table->vars[i].name);
        if (vm) {
            free_value_gc(vm, table->vars[i].value);
        } else {
            free_value(table->vars[i].value);
        }
    }

    if (table->vars) free(table->vars);
    table->vars = nullptr;
    table->count = 0;
    table->capacity = 0;
}

/* Free variable table safely during VM destruction - don't free GC objects since they're handled separately */
static void free_variable_table_gc_safe(_sbVM* vm, _sbVariableTable* table) {
    if (!table) return;

    /* Free each variable's name but NOT the GC-managed values (handled by GC cleanup) */
    for (size_t i = 0; i < table->count; i++) {
        if (table->vars[i].name) free(table->vars[i].name);

        // Don't free GC-managed values here - they will be freed by GC cleanup
        // Only free non-GC managed types like function source file paths
        _sbValue* value = &table->vars[i].value;
        if (value->type == VAL_FUNCTION && value->as.function && value->as.function->source_code_file) {
            // Check if this function is still valid (not pointing to freed memory)
            bool is_valid_function = false;
            if (vm && vm->functions) {
                for (size_t j = 0; j < vm->function_count; j++) {
                    if (&vm->functions[j] == value->as.function) {
                        is_valid_function = true;
                        break;
                    }
                }
            }
            
            if (is_valid_function) {
                free(value->as.function->source_code_file);
                value->as.function->source_code_file = nullptr;
            }
        }
    }

    if (table->vars) free(table->vars);
    table->vars = nullptr;
    table->count = 0;
    table->capacity = 0;
}

/* Find variable in variable table */
static _sbVariable* find_variable(_sbVariableTable* table, const char* name) {
    if (!table || !name) return nullptr;

    // Linear search through variable table
    for (size_t i = 0; i < table->count; i++) {
        if (strcmp(table->vars[i].name, name) == 0) {
            return &table->vars[i];
        }
    }
    return nullptr;
}

/* Add or update variable in variable table */
static bool add_variable(_sbVM* vm, _sbVariableTable* table, const char* name, _sbValue value) {
    if (!table || !name) return false;

    /* Check if variable already exists */
    _sbVariable* existing = find_variable(table, name);
    if (existing) {
        // Update existing variable value
        if (vm) {
            free_value_gc(vm, existing->value);
        } else {
            free_value(existing->value);
        }
        existing->value = copy_value(vm, value);
        return true;
    }

    /* Check if capacity needs to be increased */
    if (table->count >= table->capacity) {
        size_t new_capacity = table->capacity == 0 ? 8 : table->capacity * 2;
        _sbVariable* new_vars = (_sbVariable*)realloc(table->vars, new_capacity * sizeof(_sbVariable));
        if (!new_vars) return false;

        table->vars = new_vars;
        table->capacity = new_capacity;
    }

    /* Add new variable */
    table->vars[table->count].name = _s_strdup(name);
    table->vars[table->count].value = copy_value(vm, value);
    table->count++;

    return true;
}

/* ========== VM Management Functions ========== */

/**
 * Create VM instance
 * Initialize all components and set default state
 */
_sbVM* create_vm() {
    //printf("DEBUG: Starting create_vm\n");
    _sbVM* vm = (_sbVM*)malloc(sizeof(_sbVM));
    if (!vm) return nullptr;

    //printf("DEBUG: VM allocated, initializing fields\n");

    /* Initialize instruction-related fields */
    vm->chunks = (_sbSubChunk*)malloc(sizeof(_sbSubChunk) * VM_INITIAL_CHUNK_SIZE);
    vm->chunk_id = -1;
    vm->chunk_count = 0;
    vm->chunk_capacity = VM_INITIAL_CHUNK_SIZE;

    vm->chunk_traceback = (size_t*)malloc(sizeof(size_t) * VM_INITIAL_CHUNK_TRACEBACK_SIZE);
    vm->chunk_tb_count = 0;
    vm->chunk_traceback_capacity = VM_INITIAL_CHUNK_TRACEBACK_SIZE;

    vm->instructions = nullptr;
    vm->instruction_count = 0;
    vm->pc = 0; // Program counter

    /* Initialize dynamic stack */
    vm->stack = (_sbValue*)malloc(VM_INITIAL_STACK_SIZE * sizeof(_sbValue));
    vm->stack_top = 0;
    vm->stack_capacity = VM_INITIAL_STACK_SIZE;

    /* Initialize dynamic call stack */
    vm->call_stack = (_sbCallFrame*)malloc(VM_INITIAL_CALL_STACK_SIZE * sizeof(_sbCallFrame));
    vm->call_depth = 0;
    vm->call_capacity = VM_INITIAL_CALL_STACK_SIZE;

    vm->debug = false;

    if (!vm->stack || !vm->call_stack) {
        if (vm->stack) free(vm->stack);
        if (vm->call_stack) free(vm->call_stack);
        free(vm);
        return nullptr;
    }

    /* Initialize variable tables */
    init_variable_table(&vm->globals);
    vm->locals = nullptr;

    /* Initialize function and struct tables */
    vm->functions = malloc(sizeof(_sbVFunction));
    vm->function_count = 0;
    vm->structs = nullptr;
    vm->struct_count = 0;

    /* Initialize loaded libraries */
    vm->loaded_libs = nullptr;
    vm->loaded_lib_count = 0;

    /* Initialize error and runtime state */
    vm->last_error = VM_OK;
    vm->error_message = nullptr;
    vm->running = false;
    vm->gc_enabled = true;

    /* Initialize garbage collection */
    vm->gc_objects = nullptr;
    vm->gc_object_count = 0;
    vm->gc_threshold = 1024 * 1024; // 1MB default threshold
    vm->gc_bytes_allocated = 0;

    /* Initialize source tracking */
    vm->source_filename = nullptr;
    vm->source_content = nullptr;
    vm->is_bytecode_execution = false;

    vm->sfp = malloc(sizeof(char*) * VM_INITIAL_SOURCE_FILE_PATH_STORAGE_SIZE);
    vm->sfp_count = 0;
    vm->sfp_capacity = VM_INITIAL_SOURCE_FILE_PATH_STORAGE_SIZE;

    vm->bc = malloc(sizeof(int) * VM_INITIAL_SOURCE_FILE_PATH_STORAGE_SIZE);
    vm->bc_count = 0;
    vm->bc_capacity = VM_INITIAL_SOURCE_FILE_PATH_STORAGE_SIZE;

    vm->sf_traceback = malloc(sizeof(int) * VM_INITIAL_SOURCE_FILE_PATH_TRACEBACK_SIZE);
    vm->sf_traceback_count = 0;
    vm->sf_traceback_capacity = VM_INITIAL_SOURCE_FILE_PATH_TRACEBACK_SIZE;

    if (vm) {
        /* Register built-in functions */
        register_builtin_functions(vm);
    }

    vm->error_from_native = false;
    vm->error = false;

    return vm;
}

/* Chunk management */
extern size_t new_chunk(_sbVM* vm) {
    if (!vm) return -1;

    if (vm->chunk_count + 1 >= vm->chunk_capacity) {
        vm->chunk_capacity = vm->chunk_capacity + VM_INITIAL_CHUNK_SIZE;
        vm->chunks = (_sbSubChunk*)realloc(vm->chunks, vm->chunk_capacity * sizeof(_sbSubChunk));
    }

    vm->chunks[vm->chunk_count].inst = nullptr;
    vm->chunks[vm->chunk_count].chunk_pc = 0;
    vm->chunks[vm->chunk_count].inst_count = 0;
    vm->chunks[vm->chunk_count].chunk_id = vm->chunk_count;

    return vm->chunk_count++;
}

extern size_t step_chunk(_sbVM* vm, size_t chunk_id){
    if (!vm) return -1;

    if (vm->chunk_id != -1) {
        if (vm->chunk_tb_count + 1 >= vm->chunk_traceback_capacity) {
            vm->chunk_traceback_capacity = vm->chunk_traceback_capacity + VM_INITIAL_CHUNK_TRACEBACK_SIZE;
            vm->chunk_traceback = (size_t*)realloc(vm->chunk_traceback, vm->chunk_traceback_capacity * sizeof(size_t));
        }

        save_chunk(vm);

        vm->chunk_traceback[vm->chunk_tb_count] = vm->chunk_id;
        vm->chunk_tb_count++;
    }

    vm->instructions = vm->chunks[chunk_id].inst;
    vm->instruction_count = vm->chunks[chunk_id].inst_count;
    vm->pc = vm->chunks[chunk_id].chunk_pc;
    vm->chunk_id = chunk_id;

    return chunk_id;
}

extern size_t back_chunk(_sbVM* vm) {
    if (!vm) return -1;
    if (vm->chunk_id == -1) return -1;

    if (vm->chunk_tb_count < 1) return -1;

    save_chunk(vm);

    size_t id = vm->chunks[vm->chunk_traceback[vm->chunk_tb_count - 1]].chunk_id;
    vm->instruction_count = vm->chunks[id].inst_count;
    vm->pc = vm->chunks[id].chunk_pc;
    vm->instructions = vm->chunks[id].inst;
    vm->chunk_id = id;

    vm->chunk_tb_count--;
    return id;
}

extern size_t save_chunk(_sbVM* vm) {
    if (!vm) return -1;
    if (vm->chunk_id == -1) return -1;

    vm->chunks[vm->chunk_id].chunk_pc = vm->pc;
    vm->chunks[vm->chunk_id].inst_count = vm->instruction_count;
    vm->chunks[vm->chunk_id].inst = vm->instructions;

    return vm->chunk_id;
}


/**
 * Destroy virtual machine instance and free all resources
 */
void destroy_vm(_sbVM* vm) {
    if (!vm) return;

    vm->running = false;

    // Disable GC during cleanup to prevent interference
    vm->gc_enabled = false;

    // Clean up all GC objects first, before freeing variable tables
    _sbGCObject* current = vm->gc_objects;
    while (current) {
        _sbGCObject* next = current->next;

        // Free the actual data based on type
        if (current->data) {
            gc_free_data_by_type(vm, current->type, current->data);
        }

        free(current);
        current = next;
    }
    vm->gc_objects = nullptr;
    vm->gc_object_count = 0;
    vm->gc_bytes_allocated = 0;

    destroy_vm_stacks(vm);

    free(vm->stack);
    vm->stack = nullptr;

    // Free dynamic call stack
    if (vm->call_stack) {
        free(vm->call_stack);
        vm->call_stack = nullptr;
    }

    // Free global and local variable tables (GC objects already freed above)
    free_variable_table_gc_safe(vm, &vm->globals);
    if (vm->locals) {
        free_variable_table_gc_safe(vm, vm->locals);
        free(vm->locals);
        vm->locals = nullptr;
    }

    // Free loaded shared libraries
    if (vm->loaded_libs) {
        for (size_t i = 0; i < vm->loaded_lib_count; i++) {
            if (vm->loaded_libs[i].handle) {
#ifdef _WIN32
                FreeLibrary((HMODULE)vm->loaded_libs[i].handle);
#else
                dlclose(vm->loaded_libs[i].handle);
#endif
            }
            if (vm->loaded_libs[i].name) {
                free(vm->loaded_libs[i].name);
                vm->loaded_libs[i].name = nullptr;
            }
        }
        free(vm->loaded_libs);
        vm->loaded_libs = nullptr;
        vm->loaded_lib_count = 0;
    }

    // Free instruction array and string operands
    if (vm->chunks) {
        for (size_t j = 0; j < vm->chunk_count; j++) {
            step_chunk(vm, j);
            if (vm->instructions) {
                for (size_t i = 0; i < vm->instruction_count; i++) {
                    Instruction* inst = &vm->instructions[i];
                    // Free all string operands
                    if (inst->opcode == OP_PUSH_STR || inst->opcode == OP_PUSH_IDENT ||
                        inst->opcode == OP_LOAD_VAR || inst->opcode == OP_STORE_VAR ||
                        inst->opcode == OP_LOAD_MODULE || inst->opcode == OP_FUNC_START ||
                        inst->opcode == OP_STRUCT_DEF || inst->opcode == OP_STRUCT_NEW ||
                        inst->opcode == OP_MEMBER_ACCESS || inst->opcode == OP_MEMBER_STORE ||
                        inst->opcode == OP_LOAD_GLOBAL || inst->opcode == OP_STORE_GLOBAL) {
                        if (inst->operand.str_value) {
                            free(inst->operand.str_value);
                            inst->operand.str_value = nullptr;
                        }
                        }
                }
                free(vm->instructions);
                vm->instructions = nullptr;
                vm->instruction_count = 0;
            }
            back_chunk(vm);
        }
    }

    free(vm->chunks);
    free(vm->chunk_traceback);

    // Free function definitions
    if (vm->functions) {
        for (size_t i = 0; i < vm->function_count; i++) {
            if (vm->functions[i].name) {
                free(vm->functions[i].name);
                vm->functions[i].name = nullptr;
            }
            if (vm->functions[i].source_code_file) {
                free(vm->functions[i].source_code_file);
                vm->functions[i].source_code_file = nullptr;
            }
            if (vm->functions[i].locals) {
                free(vm->functions[i].locals);
                vm->functions[i].locals = nullptr;
            }
        }
        free(vm->functions);
        vm->functions = nullptr;
        vm->function_count = 0;
    }

    // Free struct definitions
    if (vm->structs) {
        for (size_t i = 0; i < vm->struct_count; i++) {
            if (vm->structs[i].name) {
                free(vm->structs[i].name);
                vm->structs[i].name = nullptr;
            }
            if (vm->structs[i].members) {
                for (size_t j = 0; j < vm->structs[i].member_count; j++) {
                    if (vm->structs[i].members[j]) {
                        free(vm->structs[i].members[j]);
                        vm->structs[i].members[j] = nullptr;
                    }
                }
                free(vm->structs[i].members);
                vm->structs[i].members = nullptr;
            }
            vm->structs[i].member_count = 0;
        }
        free(vm->structs);
        vm->structs = nullptr;
        vm->struct_count = 0;
    }

    // Free error message
    if (vm->error_message) {
        free(vm->error_message);
        vm->error_message = nullptr;
    }

    // Free source tracking information
    if (vm->source_filename) {
        free(vm->source_filename);
        vm->source_filename = nullptr;
    }
    if (vm->source_content) {
        free(vm->source_content);
        vm->source_content = nullptr;
    }

    // Clear remaining fields
    vm->stack_top = 0;
    vm->stack_capacity = 0;
    vm->call_depth = 0;
    vm->call_capacity = 0;
    vm->pc = 0;
    vm->last_error = VM_OK;

    vm_cleanup_static_buffers(vm);

    free(vm);

    free(sep_s);
}

void destroy_vm_stacks(_sbVM* vm) {
    // Free dynamic stack without GC (since GC is disabled and cleaned)
    if (vm->stack) {
        for (size_t i = 0; i < vm->stack_top; i++) {
            // Only free non-GC managed parts since GC objects are already freed
            _sbValue* val = &vm->stack[i];
            if (val->type == VAL_FUNCTION && val->as.function && val->as.function->source_code_file) {
                free(val->as.function->source_code_file);
                val->as.function->source_code_file = nullptr;
            }
        }
    }
}

/**
 * Push value onto VM stack
 */
void vm_push(_sbVM* vm, _sbValue value) {
    if (!vm) return;

    /* Check if stack needs expansion */
    if (vm->stack_top >= vm->stack_capacity) {
        size_t new_capacity = vm->stack_capacity + VM_INITIAL_STACK_SIZE;
        _sbValue* new_stack = (_sbValue*)realloc(vm->stack, new_capacity * sizeof(_sbValue));
        if (!new_stack) {
            vm_error(vm, VM_MEMORY_ERROR, "Failed to expand stack");
            return;
        }
        vm->stack = new_stack;
        vm->stack_capacity = new_capacity;
    }

    vm->stack[vm->stack_top++] = value;
}

/**
 * Pop value from VM stack
 */
_sbValue vm_pop(_sbVM* vm) {
    if (!vm || vm->stack_top == 0) {
        if (vm) vm_error(vm, VM_STACK_UNDERFLOW, "Stack underflow");
        return create_null();
    }

    return vm->stack[--vm->stack_top];
}

/**
 * Peek at value at distance from top of stack (without popping)
 */
_sbValue vm_peek(_sbVM* vm, int distance) {
    if (!vm || vm->stack_top <= (size_t)distance) {
        return create_null();
    }

    return vm->stack[vm->stack_top - 1 - distance];
}

/* ========== Value Creation Functions ========== */

/**
 * Create null value
 */
_sbValue create_null() {
    _sbValue val;
    val.type = VAL_NULL;
    val.freed = false;
    return val;
}

/**
 * Create number value
 */
_sbValue create_number(double num) {
    _sbValue val;
    val.type = VAL_NUMBER;
    val.as.number = num;
    val.freed = false;
    return val;
}

/**
 * Create string value (copies the string)
 */
_sbValue create_string(_sbVM* vm, const char* str) {
    _sbValue val;
    val.type = VAL_STRING;
    val.freed = false;

    if (str && vm && vm->gc_enabled) {
        // Use GC allocation for string
        size_t len = strlen(str) + 1;
        val.as.string = (char*)gc_alloc(vm, len, VAL_STRING);
        if (val.as.string) {
            strcpy(val.as.string, str);
        }
    } else {
        val.as.string = str ? _s_strdup(str) : nullptr;
    }

    if (val.as.string) {
        //printf("DEBUG: create_string('%s') allocated at %p\n", str, val.as.string);
    }
    return val;
}

/**
 * Create boolean value
 */
_sbValue create_bool(bool b) {
    _sbValue val;
    val.type = VAL_BOOL;
    val.as.boolean = b;
    val.freed = false;
    return val;
}

/**
 * Create function value
 */
_sbValue create_function(_sbVM* vm, _sbVFunction* func, size_t chunk_id) {
    _sbValue val;
    val.type = VAL_FUNCTION;
    val.freed = false;
    val.as.function = func;
    val.as.function->chunk_id = chunk_id;
    return val;
}

/**
 * Create struct value
 */
_sbValue create_struct(_sbVM* vm, _sbVStruct* _struct) {
    _sbValue val;
    val.type = VAL_STRUCT;
    val.freed = false;
    val.as.struct_def = _struct;
    return val;
}

/**
 * Create native function value
 */
_sbValue create_native(_sbNativeFunction func) {
    _sbValue val;
    val.type = VAL_NATIVE;
    val.as.native = func;
    val.freed = false;
    return val;
}

/**
 * Create empty list value
 */
_sbValue create_list(_sbVM* vm) {
    _sbValue val;
    val.type = VAL_LIST;
    val.freed = false;

    if (vm && vm->gc_enabled) {
        val.as.list = (_sbVList*)gc_alloc(vm, sizeof(_sbVList), VAL_LIST);
    } else {
        val.as.list = (_sbVList*)malloc(sizeof(_sbVList));
    }

    if (val.as.list) {
        val.as.list->items = nullptr;
        val.as.list->count = 0;
        val.as.list->capacity = 0;
    }
    return val;
}

/**
 * Append an item to list
 */
_sbVList* append_list(_sbVList* list, _sbValue value) {
    if (list->count + 1 > list->capacity) {
        list->capacity += 8;
        list->items = realloc(list->items, sizeof(_sbValue) * list->capacity);
        if (!list->items) return nullptr;
    }

    list->items[list->count++] = value;

    return list;
}

/**
 * Insert an item into list
 */
_sbVList* insert_list(_sbVList* list, int index, _sbValue value) {
    _sbValue* new_list = malloc(sizeof(_sbValue) * list->capacity);
    if (!new_list) return nullptr;

    if (list->count + 1 > list->capacity) {
        list->capacity += 8;
        new_list = realloc(new_list, sizeof(_sbValue) * list->capacity);
        if (!new_list) return nullptr;
    }

    //list->items[list->count++] = value;
    int count = 0;
    for (int i = 0; i < list->count; i++) {
        if (i == index) {
            new_list[count] = value;
            count++;
        }

        new_list[count] = list->items[i];
        count++;
    }

    free(list->items);
    list->items = new_list;

    list->count++;

    return list;
}

/**
 * Pop an item from list
 */
_sbVList* pop_list(_sbVList* list) {

    --list->count;

    return list;
}


/**
 * Check if value is truthy (truthiness evaluation)
 */
bool is_truthy(_sbValue value) {
    switch (value.type) {
        case VAL_NULL: return false;
        case VAL_BOOL: return value.as.boolean;
        case VAL_NUMBER: return value.as.number != 0;
        default: return true; // Other types (string, function, etc.) are truthy
    }
}

/**
 * Compare two values for equality
 */
bool values_equal(_sbValue a, _sbValue b) {
    if (a.type != b.type) return false;

    switch (a.type) {
        case VAL_NULL: return true;
        case VAL_BOOL: return a.as.boolean == b.as.boolean;
        case VAL_NUMBER: return a.as.number == b.as.number;
        case VAL_STRING:
            if (!a.as.string || !b.as.string) return a.as.string == b.as.string;
            return strcmp(a.as.string, b.as.string) == 0;
        default: return false; // Complex types not compared
    }
}

/**
 * Create a deep copy of a value
 */
_sbValue copy_value(_sbVM* vm, _sbValue value) {
    _sbValue result;
    result.type = value.type;
    result.freed = false;

    switch (value.type) {
        case VAL_NULL:
            break;
        case VAL_BOOL:
            result.as.boolean = value.as.boolean;
            break;
        case VAL_NUMBER:
            result.as.number = value.as.number;
            break;
        case VAL_STRING:
            if (value.as.string) {
                if (vm && vm->gc_enabled) {
                    // Create a safe copy before GC allocation to prevent use-after-free
                    // during garbage collection triggered by gc_alloc
                    size_t len = strlen(value.as.string) + 1;
                    char* temp_string = malloc(len);  // Use regular malloc for temp copy
                    if (temp_string) {
                        strcpy(temp_string, value.as.string);
                        
                        result.as.string = (char*)gc_alloc(vm, len, VAL_STRING);
                        if (result.as.string) {
                            strcpy(result.as.string, temp_string);
                        }
                        
                        free(temp_string);  // Free the temporary copy
                    } else {
                        result.as.string = nullptr;
                    }
                } else {
                    result.as.string = _s_strdup(value.as.string);
                }
            } else {
                result.as.string = nullptr;
            }
            if (result.as.string) {
                //printf("DEBUG: copy_value string '%s' from %p to %p\n", value.as.string, value.as.string, result.as.string);
            }
            break;
        case VAL_LIST:
            if (value.as.list) {
                if (vm && vm->gc_enabled) {
                    result.as.list = (_sbVList*)gc_alloc(vm, sizeof(_sbVList), VAL_LIST);
                } else {
                    result.as.list = malloc(sizeof(_sbVList));
                }
                result.as.list->capacity = value.as.list->capacity;
                result.as.list->count = value.as.list->count;
                if (value.as.list->items && value.as.list->count > 0) {
                    result.as.list->items = malloc(sizeof(_sbValue) * value.as.list->capacity);
                    for (size_t i = 0; i < value.as.list->count; i++) {
                        result.as.list->items[i] = copy_value(vm, value.as.list->items[i]);
                    }
                }
                else {
                    result.as.list->items = nullptr;
                }
            }
            else {
                result.as.list = nullptr;
            }
            break;
        case VAL_STRUCT_INSTANCE:
            if (!value.as.instance) {
                result.as.instance = nullptr;
            }
            else {
                if (vm && vm->gc_enabled) {
                    result.as.instance = (_sbVStructInstance*)gc_alloc(vm, sizeof(_sbVStructInstance), VAL_STRUCT_INSTANCE);
                } else {
                    result.as.instance = (_sbVStructInstance*)malloc(sizeof(_sbVStructInstance));
                }

                result.as.instance->members = (_sbValue*)malloc(sizeof(_sbValue) * value.as.instance->struct_def->member_count);
                for (int i = 0; i < value.as.instance->struct_def->member_count; i++) {
                    result.as.instance->members[i] = copy_value(vm, value.as.instance->members[i]);
                }

                /*
                result.as.instance->struct_def = (Struct*)malloc(sizeof(Struct));
                result.as.instance->struct_def->name = value.as.instance->struct_def->name ? _s_strdup(value.as.instance->struct_def->name) : nullptr;
                result.as.instance->struct_def->members = (char**)malloc(sizeof(char*) * value.as.instance->struct_def->member_count);
                for (int i = 0; i < value.as.instance->struct_def->member_count; i++) {
                    result.as.instance->struct_def->members[i] = value.as.instance->struct_def->members[i] ? _s_strdup(value.as.instance->struct_def->members[i]) : nullptr;
                }
                result.as.instance->struct_def->member_count = value.as.instance->struct_def->member_count;
                */

                result.as.instance->struct_def = value.as.instance->struct_def;
            }
            break;
        case VAL_NATIVE:
            result.as.native = value.as.native;
            break;
        case VAL_FUNCTION:
            result.as.function = value.as.function;
            break;
        default:
            result = value;
            break;
    }

    return result;
}

/**
 * Remove and free a GC object from tracking
 * This is used when we want to immediately free an object
 */
static bool gc_remove_and_free(_sbVM* vm, void* ptr) {
    if (!vm || !ptr || !vm->gc_enabled) return false;

    _sbGCObject* prev = nullptr;
    _sbGCObject* current = vm->gc_objects;

    while (current) {
        if (current->data == ptr && current->data != nullptr) {
            // Remove from linked list
            if (prev) {
                prev->next = current->next;
            } else {
                vm->gc_objects = current->next;
            }

            // Free the actual data based on type
            gc_free_data_by_type(vm, current->type, current->data);

            // Free the GC object header
            free(current);
            vm->gc_object_count--;
            return true;
        }
        prev = current;
        current = current->next;
    }
    return false;
}

/**
 * Free data based on its type - used only during VM destruction when GC is disabled
 * NOTE: This should NOT recursively free child objects as they are managed separately in GC
 */
static void gc_free_data_by_type(_sbVM* vm, _sbValueType type, void* data) {
    if (!data) return;
    if (type == VAL_FREED) return;

    switch (type) {
        case VAL_STRING:
            free(data);
            data = nullptr;
            break;
        case VAL_LIST: {
            _sbVList* list = (_sbVList*)data;
            // Don't free list->items if it's GC-allocated, it will be freed separately
            // Only free if it's not in the GC object list (check by type)

            if (list->items) {
                /*for (size_t i = 0; i < list->count; i++) {
                    free_value(list->items[i]);
                }*/
                // This is tricky - items array might be GC or non-GC allocated
                // For now, assume items arrays are always non-GC allocated
                free(list->items);
            }
            free(list);
            break;
        }
        case VAL_STRUCT_INSTANCE: {
            _sbVStructInstance* instance = (_sbVStructInstance*)data;
            // Free members array - it's always allocated with malloc/calloc, not GC
            if (instance->members) {
                free(instance->members);
            }
            free(instance);
            break;
        }
        default:
            free(data);
            break;
    }
}

/**
 * Check if pointer is GC-managed
 */
bool is_gc_managed(_sbVM* vm, void* ptr) {
    if (!vm || !ptr) return false;

    _sbGCObject* obj = vm->gc_objects;
    while (obj) {
        if (obj->data == ptr) return true;
        obj = obj->next;
    }
    return false;
}

/**
 * Free memory occupied by value (GC-aware version)
 * This function properly handles GC managed objects
 */
void free_value_gc(_sbVM* vm, _sbValue value) {
    if (!vm || !vm->gc_enabled) {
        free_value(value);
        return;
    }

    if (value.freed) return;
    value.freed = true;

    switch (value.type) {
        case VAL_STRING:
            if (value.as.string) {
                gc_remove_and_free(vm, value.as.string);
            }
            break;
        case VAL_LIST:
            break;
        case VAL_STRUCT_INSTANCE:
            if (value.as.instance) {
                // First free all members recursively
                if (value.as.instance->members && value.as.instance->struct_def) {
                    for (size_t i = 0; i < value.as.instance->struct_def->member_count; i++) {
                        free_value_gc(vm, value.as.instance->members[i]);
                    }
                }
                gc_remove_and_free(vm, value.as.instance);
            }
            break;
        case VAL_FUNCTION:
            // Function values don't own source_code_file, only vm->functions array does
            // So we don't free source_code_file here
            break;
        default:
            // Other types don't allocate heap memory or are not GC-managed
            break;
    }

    value.type = VAL_FREED;
}

/**
 * Free memory occupied by value
 */
void free_value(_sbValue value) {
    if (value.freed) return;
    value.freed = true;

    switch (value.type) {
        case VAL_STRING:
            if (value.as.string) {
                //printf("DEBUG: Freeing string: '%s' at %p\n", value.as.string, value.as.string);
                free(value.as.string);
                value.as.string = nullptr;
            }
            break;
        case VAL_LIST:
            if (value.as.list) {
                if (value.as.list->items) {
                    // Free all elements in the list
                    for (size_t i = 0; i < value.as.list->count; i++) {
                        free_value(value.as.list->items[i]);
                    }
                    free(value.as.list->items);
                }
                free(value.as.list);
                value.as.list = nullptr;
            }
            break;
        case VAL_STRUCT_INSTANCE:
            if (value.as.instance) {
                if (value.as.instance->members && value.as.instance->struct_def) {
                    // Free all members of struct instance
                    for (size_t i = 0; i < value.as.instance->struct_def->member_count; i++) {
                        free_value(value.as.instance->members[i]);
                        //free(value.as.instance->struct_def->members[i]);
                    }
                    free(value.as.instance->members);
                    //free(value.as.instance->struct_def->name);
                    //free(value.as.instance->struct_def->members);
                    //free(value.as.instance->struct_def);
                    value.as.instance->members = nullptr;
                }
                // Do NOT free struct_def - it's shared among all instances and managed by VM
                free(value.as.instance);
            }
            break;
        case VAL_FUNCTION:
            if (value.as.function && value.as.function->source_code_file) {
                free(value.as.function->source_code_file);
                value.as.function->source_code_file = nullptr;
            }
        default:
            break; // Other types don't need special handling
    }

    value.type = VAL_FREED;
}

/* ========== Error Handling Functions ========== */

/**
 * Enable debug for VM instace
 */
void enable_debug(_sbVM* vm) {
    vm->debug = true;
}

/**
 * Print current stack state (for debugging)
 */
void vm_print_stack(_sbVM* vm) {
    if (!vm) return;
    printf("\n=== Stack ===\n");

    printf("Stack [%zx]: \n\n", vm->stack_top);
    for (size_t i = 0; i < vm->stack_top; i++) {
        printf("%06zu: ", i);
        switch (vm->stack[i].type) {
            case VAL_NULL: printf("null"); break;
            case VAL_NUMBER: printf("%.6f ", vm->stack[i].as.number); break;
            case VAL_STRING: printf("\"%s\" ", vm->stack[i].as.string); break;
            case VAL_BOOL: printf("%s ", vm->stack[i].as.boolean ? "true" : "false"); break;
            case VAL_LIST: printf("<list object>");break;
            case VAL_FUNCTION: {
                const char* source_file = vm->stack[i].as.function->source_code_file ? vm->stack[i].as.function->source_code_file : "<unknown>";
                const char* func_name = vm->stack[i].as.function->name ? vm->stack[i].as.function->name : "<unnamed>";
                char* _s1 = calloc(13 + strlen(func_name) + strlen(source_file), sizeof(char));
                strcpy(_s1, "<function:");
                strcat(_s1, func_name);
                strcat(_s1, ":");
                strcat(_s1, source_file);
                strcat(_s1, ">");
                printf("%s ", _s1);
                free(_s1);
                break;
            }
            case VAL_NATIVE: printf("<native_function>"); break;
            case VAL_STRUCT_INSTANCE:
                ssize_t size1 = 13;
                size1 += strlen(vm->stack[i].as.instance->struct_def->name);
                char* _s4 = calloc(size1, sizeof (char));
                strcpy(_s4, "{Instance[");
                strcat(_s4, vm->stack[i].as.instance->struct_def->name);
                strcat(_s4, "]}");
                printf("%s", _s4);
                free(_s4);
            case VAL_STRUCT:
                ssize_t size = 12;
                size += strlen(vm->stack[i].as.struct_def->name);
                for (int i = 0; i < vm->stack[i].as.struct_def->member_count; i++) {
                    size += strlen(vm->stack[i].as.struct_def->members[i]);
                    if (i != vm->stack[i].as.struct_def->member_count - 1)
                        size += 2;
                }
                size++;
                char* _s3 = calloc(size, sizeof (char));
                strcpy(_s3, "{Struct[");
                strcat(_s3, vm->stack[i].as.struct_def->name);
                strcat(_s3, "]: ");
                for (int i = 0; i < vm->stack[i].as.struct_def->member_count; i++) {
                    strcat(_s3, vm->stack[i].as.struct_def->members[i]);
                    if (i != vm->stack[i].as.struct_def->member_count - 1)
                        strcat(_s3, ", ");
                }
                strcat(_s3, "}");
                printf("%s", _s3);
                free(_s3);
            default: printf("<?undefined type>"); break;
        }
        printf("\n");
    }
    printf("\nNormal quit will clear stack (all item will be `null`)\n");
}

/**
 * Print VM status
 */
void vm_print_status(_sbVM* vm) {
    if (!vm) return;
    printf("=== Debugging outputs ===\n");

    printf("\n=== Bytecode ===\n");
    for (size_t c = 0; c < vm->chunk_count; c++) {
        step_chunk(vm, c);
        printf("\n--- Chunk %d instructions: %zu\n\n", c, vm->instruction_count);

        for (size_t i = 0; i < vm->instruction_count; i++) {
            Instruction* inst = &(vm->instructions[i]);
            if (!inst) continue;

            printf("0x%06zx: ", i);

            switch (inst->opcode) {
                case OP_NOP: printf("NOP"); break;
                case OP_PUSH_NUM: printf("PUSH_NUM %.6f", inst->operand.num_value); break;
                case OP_PUSH_STR: printf("PUSH_STR \"%s\"", inst->operand.str_value); break;
                case OP_PUSH_IDENT: printf("PUSH_IDENT %s", inst->operand.str_value); break;
                case OP_PUSH_TRUE: printf("PUSH_TRUE"); break;
                case OP_PUSH_FALSE: printf("PUSH_FALSE"); break;
                case OP_PUSH_NULL: printf("PUSH_NULL"); break;
                case OP_POP: printf("POP"); break;
                case OP_DUP: printf("DUP"); break;
                case OP_SWAP: printf("SWAP"); break;
                case OP_ADD: printf("ADD"); break;
                case OP_SUB: printf("SUB"); break;
                case OP_MUL: printf("MUL"); break;
                case OP_DIV: printf("DIV"); break;
                case OP_MOD: printf("MOD"); break;
                case OP_POW: printf("POW"); break;
                case OP_BIT_AND: printf("BIT_AND"); break;
                case OP_BIT_OR: printf("BIT_OR"); break;
                case OP_BIT_XOR: printf("BIT_XOR"); break;
                case OP_BIT_NOT: printf("BIT_NOT"); break;
                case OP_BIT_LSHIFT: printf("BIT_LSHIFT"); break;
                case OP_BIT_RSHIFT: printf("BIT_RSHIFT"); break;
                case OP_LOGIC_AND: printf("LOGIC_AND"); break;
                case OP_LOGIC_OR: printf("LOGIC_OR"); break;
                case OP_LOGIC_NOT: printf("LOGIC_NOT"); break;
                case OP_EQ: printf("EQ"); break;
                case OP_NEQ: printf("NEQ"); break;
                case OP_LT: printf("LT"); break;
                case OP_GT: printf("GT"); break;
                case OP_LEQ: printf("LEQ"); break;
                case OP_GEQ: printf("GEQ"); break;
                case OP_ASSIGN: printf("ASSIGN"); break;
                case OP_LOAD_VAR: printf("LOAD_VAR %s", inst->operand.str_value); break;
                case OP_STORE_VAR: printf("STORE_VAR %s", inst->operand.str_value); break;
                case OP_LOAD_GLOBAL: printf("LOAD_GLOBAL %s", inst->operand.str_value); break;
                case OP_STORE_GLOBAL: printf("STORE_GLOBAL %s", inst->operand.str_value); break;
                case OP_JUMP: printf("JUMP 0x%x", inst->operand.int_value); break;
                case OP_JUMP_IF_FALSE: printf("JUMP_IF_FALSE 0x%x", inst->operand.int_value); break;
                case OP_JUMP_IF_TRUE: printf("JUMP_IF_TRUE 0x%x", inst->operand.int_value); break;
                case OP_CALL: printf("CALL %d", inst->operand.int_value); break;
                case OP_RETURN: printf("RETURN"); break;
                case OP_FUNC_DEF: printf("FUNC_DEF %s", inst->operand.str_value); break;
                case OP_FUNC_SET_ARGS: printf("FUNC_SET_ARGS %d", inst->operand.int_value); break;
                case OP_FUNC_START: printf("FUNC_START %s", inst->operand.str_value); break;
                case OP_FUNC_END: printf("FUNC_END"); break;
                case OP_BLOCK_START: printf("BLOCK_START"); break;
                case OP_BLOCK_END: printf("BLOCK_END"); break;
                case OP_LOAD_MODULE: printf("LOAD_MODULE %s", inst->operand.str_value); break;
                case OP_STRUCT_DEF: printf("STRUCT_DEF %s", inst->operand.str_value); break;
                case OP_STRUCT_NEW: printf("STRUCT_NEW %s", inst->operand.str_value); break;
                case OP_MEMBER_ACCESS: printf("MEMBER_ACCESS %s", inst->operand.str_value); break;
                case OP_MEMBER_STORE: printf("MEMBER_STORE %s", inst->operand.str_value); break;
                case OP_LIST_NEW: printf("LIST_NEW %d", inst->operand.int_value); break;
                case OP_LIST_ACCESS: printf("LIST_ACCESS"); break;
                case OP_LIST_STORE: printf("LIST_STORE"); break;
                case OP_LIST_PUSH: printf("LIST_PUSH"); break;
                case OP_HALT: printf("HALT"); break;
                default: printf("UNKNOWN_OP %d", inst->opcode); break;
            }
            printf("\n");
        }
        back_chunk(vm);
    }

    if (vm->functions) {
        if (vm->function_count > 0) {
            printf("\n=== Functions ===\n");
            for (size_t i = 0; i < vm->function_count; i++) {
                _sbVFunction func = vm->functions[i];
                printf("  %s (params: %zu, addr: %zu, block: %lu, from: %s)\n",
                    func.name, func.param_count, func.start_addr, func.chunk_id, func.source_code_file);
            }
        }
    }

    if (vm->structs) {
        if (vm->struct_count > 0) {
            printf("\n=== Structs ===\n");
            for (size_t i = 0; i <vm->struct_count; i++) {
                _sbVStruct st = vm->structs[i];
                printf("  %s { ", st.name);
                for (size_t j = 0; j < st.member_count; j++) {
                    char* member = st.members[j];
                    if (member) printf("%s ", member);
                }
                printf("}\n");
            }
        }
    }

    if (vm->globals.count > 0) {
        printf("\n=== Globals ===\n");
        for (size_t i = 0; i < vm->globals.count; i++) {
            char* global = vm->globals.vars[i].name;
            if (global) printf("  %s\n", global);
        }
    }

    if (vm->locals) {
        if (vm->locals->count > 0) {
            printf("\n=== Locals ===\n");
            for (size_t i = 0; i < vm->locals->count; i++) {
                char* local = vm->locals->vars[i].name;
                if (local) printf("  %s\n", local);
            }
        }
    }

    if (vm->sfp) {
        if (vm->sfp_count > 0) {
            printf("\n=== File path storage ===\n");
            for (size_t i = 0; i < vm->sfp_count; i++) {
                char* fp = vm->sfp[i];
                bool current = false;
                if (vm->source_filename) {
                    current = strcmp(fp, vm->source_filename) == 0;
                }
                printf("  %s (bc: %s) %s\n", fp, vm->bc[i] ? "true" : "false",  current? "(current)" : "");
            }
        }
    }

    if (vm->loaded_libs) {
        if (vm->loaded_lib_count > 0) {
            printf("\n=== Loaded libraries ===\n");
            for (size_t i = 0; i < vm->loaded_lib_count; i++) {
                _sbLoadedLibrary _lib = vm->loaded_libs[i];
                printf("  %s\n", _lib.name);
            }
        }
    }

    printf("\n=== VM Information ===\n");
    printf("Enabled GC: %s\n", vm->gc_enabled ? "true" : "false");
    if (vm->gc_enabled) {
        printf("GC Allocated: %lu\n", vm->gc_bytes_allocated);
        printf("GC Objects: %lu\n", vm->gc_object_count);
        printf("GC Threshold: %lu\n", vm->gc_threshold);
    }
    printf("\n");

    printf("Bytecode execution mode: %s\n\n", vm->is_bytecode_execution ? "true" : "false");

    printf("Loaded Chunks: %lu\n", vm->chunk_count);
    int total_instruction_count = 0;
    for (size_t i = 0; i < vm->chunk_count; i++) {
        total_instruction_count += vm->chunks[i].inst_count;
    }
    printf("Total instructions: %lu\n", total_instruction_count);
    printf("Current instruction: %lu\n", vm->pc);
    //printf("End instruction: %lu\n\n", vm->end_pc);

    printf("Stack size: %lu\n", vm->stack_capacity);
    printf("Stack top: %lu\n\n", vm->stack_top);

    printf("CallFrame Stack size: %lu\n", vm->call_capacity);
    printf("CallFrame Stack top: %lu\n\n", vm->call_depth);

    printf("Defined functions: %lu\n", vm->function_count);
    printf("Defined structures: %lu\n", vm->struct_count);
    printf("Loaded modules: %lu\n", vm->loaded_lib_count);

    vm_print_stack(vm);
}

/* ========== Source Tracking Functions ========== */

/**
 * Process Source information for backtrace functionality
 */
int vm_add_source_info(_sbVM* vm, const char* filename, bool bytecode) {
    if (!vm) return -1;

    int idx = vm_sourceinfo_lookup(vm, filename);
    if (idx != -1) return idx;

    if (vm->sfp_count + 1 >= vm->sfp_capacity) {
        vm->sfp_capacity += VM_INITIAL_SOURCE_FILE_PATH_STORAGE_SIZE;
        vm->sfp = realloc(vm->sfp, vm->sfp_capacity * sizeof(char*));
    }
    vm->sfp[vm->sfp_count++] = _s_strdup(filename);

    if (vm->bc_count + 1 >= vm->bc_capacity) {
        vm->bc_capacity += VM_INITIAL_SOURCE_FILE_PATH_STORAGE_SIZE;
        vm->bc = realloc(vm->bc, vm->bc_capacity * sizeof(int));
    }
    vm->bc[vm->bc_count++] = bytecode;

    return vm->sfp_count - 1;
}

int vm_sourceinfo_lookup(_sbVM* vm, const char* filename) {
    if (!vm) return -1;
    if (!vm->sf_traceback) return -1;

    for (int i = 0; i < vm->sfp_count; i++) {
        if (strcmp(filename, vm->sfp[i]) == 0)
            return i;
    }

    return -1;
}

int vm_set_source_info(_sbVM* vm, const char* filename, bool bytecode) {
    if (!vm) return -1;

    if (vm->sf_traceback_count + 1 >= vm->sf_traceback_capacity) {
        vm->sf_traceback_capacity += VM_INITIAL_SOURCE_FILE_PATH_TRACEBACK_SIZE;
        vm->sf_traceback = realloc(vm->sf_traceback, vm->sf_traceback_capacity * sizeof(int));
    }
    vm->sf_traceback[vm->sf_traceback_count++] = vm_sourceinfo_lookup(vm, vm->source_filename);

    int idx = 0;
    idx = vm_sourceinfo_lookup(vm, filename);;

    if (idx == -1) {
        idx = vm_add_source_info(vm, filename, bytecode);
    }

    vm->is_bytecode_execution = vm->bc[idx];

    if (vm->source_filename) free(vm->source_filename);
    if (vm->source_content) free(vm->source_content);

    vm->source_filename = _s_strdup(vm->sfp[idx]);

    if (!bytecode) {
        vm->source_content = read_file(vm->source_filename);
    }
    else {
        vm->source_content = nullptr;
        vm->is_bytecode_execution = true;
    }

    return idx;
}

int vm_back_source_info(_sbVM* vm) {
    if (!vm) return -1;

    if (vm->sf_traceback_count < 1) return -1;

    if (vm->source_filename) free(vm->source_filename);
    if (vm->source_content) free(vm->source_content);

    int idx = vm->sf_traceback[vm->sf_traceback_count - 1];

    vm->source_filename = _s_strdup(vm->sfp[idx]);
    vm->is_bytecode_execution = vm->bc[idx];

    if (!vm->is_bytecode_execution)
        vm->source_content = read_file(vm->source_filename);
    else
        vm->source_content = nullptr;

    vm->sf_traceback_count--;

    return idx;
}

/* Read entire file into memory */
char* read_file(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        return nullptr;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* content = (char*)malloc(size + 1);
    if (!content) {
        fclose(file);
        return nullptr;
    }

    fread(content, 1, size, file);
    content[size] = '\0';
    fclose(file);

    return content;
}

/**
 * Mark VM as executing bytecode only
 */
void vm_set_bytecode_execution(_sbVM* vm, bool is_bytecode) {
    if (!vm) return;
    vm->is_bytecode_execution = is_bytecode;
}

/* ========== Variable Operation Functions ========== */

/**
 * Get variable value (search in local variables first, then global variables)
 */
_sbValue* vm_get_variable(_sbVM* vm, const char* name) {
    if (!vm || !name) return nullptr;

    // First search local variables
    if (vm->locals) {
        _sbVariable* var = find_variable(vm->locals, name);
        if (var) return &var->value;
    }

    // Then search global variables
    _sbVariable* var = find_variable(&vm->globals, name);
    if (var) return &var->value;

    return nullptr;
}

/**
 * Set variable value (set in local variables first, then global variables)
 */
bool vm_set_variable(_sbVM* vm, const char* name, _sbValue value) {
    if (!vm || !name) return false;

    // First try to set local variable
    if (vm->locals) {
        _sbVariable* var = find_variable(vm->locals, name);
        if (var) {
            if (vm) {
                free_value_gc(vm, var->value);
            } else {
                free_value(var->value);
            }
            var->value = copy_value(vm, value);
            // Check for global
            _sbVariable* var_g = find_variable(&vm->globals, name);
            if (var_g) { // Is global
                if (vm) {
                    free_value_gc(vm, var_g->value);
                } else {
                    free_value(var_g->value);
                }
                var_g->value = copy_value(vm, value);
            }
            return true;
        }
        return add_variable(vm, vm->locals, name, value);
    }

    // Then try to set global variable
    _sbVariable* var = find_variable(&vm->globals, name);
    if (var) {
        if (vm) {
            free_value_gc(vm, var->value);
        } else {
            free_value(var->value);
        }
        var->value = copy_value(vm, value);
        return true;
    }

    return add_variable(vm, &vm->globals, name, value);
}

/**
 * Define global variable
 */
bool vm_define_global(_sbVM* vm, const char* name, _sbValue value) {
    if (!vm || !name) return false;
    return add_variable(vm, &vm->globals, name, value);
}

/**
 * Register native function to global variables
 */
void vm_register_native(_sbVM* vm, const char* name, _sbNativeFunction func) {
    if (!vm || !name || !func) return;

    _sbValue native_val = create_native(func);
    vm_define_global(vm, name, native_val);
}

/**
 * Push value to VM stack from external source
 */
void vm_push_external(_sbVM* vm, _sbValue value) {
    if (!vm) return;
    vm_push(vm, value);
}

/* ========== Module Loading Functions ========== */

/**
 * Check if file exists
 */
static bool file_exists(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (file) {
        fclose(file);
        return true;
    }
    return false;
}

/**
 * Check if path is a directory
 */
static bool is_directory(const char* path) {
    struct stat statbuf;
    if (stat(path, &statbuf) != 0) {
        return false;
    }
    return S_ISDIR(statbuf.st_mode);
}

/**
 * Get executable directory path
 */
static bool get_executable_dir(char* dir_path, size_t size) {
#ifdef _WIN32
    char exe_path[MAX_PATH];
    if (GetModuleFileName(NULL, exe_path, MAX_PATH) == 0) {
        return false;
    }
    char* last_slash = strrchr(exe_path, '\\');
    if (last_slash) {
        *last_slash = '\0';
    }
    strncpy(dir_path, exe_path, size - 1);
    dir_path[size - 1] = '\0';
    return true;
#else
    char exe_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len == -1) {
        return false;
    }
    exe_path[len] = '\0';

    char* dir = dirname(exe_path);
    strncpy(dir_path, dir, size - 1);
    dir_path[size - 1] = '\0';
    return true;
#endif
}

/**
 * Load shared library and execute _sbLibInit
 */
static bool load_shared_library(_sbVM* vm, const char* lib_path, const char* module_name) {
#ifdef _WIN32
    HMODULE handle = LoadLibrary(lib_path);
    if (!handle) {
        return false;
    }

    typedef int (*LibInitFunc)(_sbVM*);
    LibInitFunc init_func = (LibInitFunc)GetProcAddress(handle, "_sbLibInit");
    if (!init_func) {
        FreeLibrary(handle);
        return false;
    }
#else
    void* handle = dlopen(lib_path, RTLD_LAZY);
    if (!handle) {
        return false;
    }

    typedef int (*LibInitFunc)(_sbVM*);
    LibInitFunc init_func = (LibInitFunc)dlsym(handle, "_sbLibInit");
    if (!init_func) {
        dlclose(handle);
        return false;
    }
#endif

    /* Store library handle for cleanup */
    _sbLoadedLibrary* new_libs = (_sbLoadedLibrary*)realloc(vm->loaded_libs,
                                                      (vm->loaded_lib_count + 1) * sizeof(_sbLoadedLibrary));
    if (!new_libs) {
#ifdef _WIN32
        FreeLibrary(handle);
#else
        dlclose(handle);
#endif
        vm_error(vm, VM_MEMORY_ERROR, "Failed to allocate memory for library tracking");
        return false;
    }

    vm->loaded_libs = new_libs;
    vm->loaded_libs[vm->loaded_lib_count].handle = handle;
    vm->loaded_libs[vm->loaded_lib_count].name = _s_strdup(module_name);
    vm->loaded_lib_count++;

    /* Call library initialization function */
    int init_result = init_func(vm);

    /* Check initialization result */
    if (init_result != 0) {
        /* Initialization failed, cleanup */
        vm->loaded_lib_count--;
        free(vm->loaded_libs[vm->loaded_lib_count].name);
#ifdef _WIN32
        FreeLibrary(handle);
#else
        dlclose(handle);
#endif
        vm_error(vm, VM_LOAD_ERROR, "Library initialization failed: _sbLibInit returned %d", init_result);
        vm->running = false;
        return false;
    }

    //printf("---DEBUG Loaded shared library: %s\n", lib_path);
    return true;
}

/**
 * Load bytecode from .sbc file
 */
bool load_bytecode_file(_sbVM* vm, const char* filename, const char* module_name) {

    vm_set_source_info(vm, filename, true);

    BytecodeGenerator* gen = load_bytecode(filename);
    if (!gen) {
        vm_back_source_info(vm);
        vm_error(vm, VM_MEMORY_ERROR, "Failed to create bytecode generator for the module `%s`", module_name);
        return false;
    }

    //printf("Loading bytecode module: %s\n", module_name);

    vm_load_bytecode(vm, gen);
    VMError result = vm_execute(vm);

    if (result != VM_OK) {
        return result;
        //vm_print_error(vm);
        // Error report had moved into vm_error
    }

    //printf("DEBUG: Module execution finished, restoring PC from %zu to %zu\n", vm->pc, saved_pc);

    /* Restore PC to continue main program execution */

    back_chunk(vm);
    vm_back_source_info(vm);

    /* Clean up resources */
    destroy_bytecode_generator(gen);

    return result == VM_OK;
}

/**
 * Load source file and compile
 */
bool load_source_file(_sbVM* vm, const char* filename, const char* module_name) {
    // For traceback
    vm_set_source_info(vm, filename, false);

    char* source = _s_strdup(vm->source_content);

    /* Lexical analysis */
    _sbToken* tokens = _sbLexer(source);
    if (!tokens) {
        free(source);
        freeTkList(tokens);
        vm_back_source_info(vm);
        vm_error(vm, VM_LOAD_ERROR, "Failed to tokenize module '%s'", module_name);
        return false;
    }

    /* Syntax analysis */
    Parser* parser = create_tkstate(tokens);
    if (!parser) {
        freeTkList(tokens);
        free(source);
        vm_back_source_info(vm);
        vm_error(vm, VM_LOAD_ERROR, "Failed to create parser for module '%s'", module_name);
        return false;
    }

    reset_error();
    ASTNode* ast = parse_program(parser);

    if (!ast || syntaxErrorDetector) {
        destroy_tkstate(parser);
        freeTkList(tokens);
        free(source);
        vm_back_source_info(vm);
        vm_error(vm, VM_LOAD_ERROR, "Failed to parse module '%s'", module_name);
        return false;
    }

    /* Generate bytecode */
    BytecodeGenerator* gen = create_bytecode_generator();
    if (!gen) {
        free_ast(ast);
        destroy_tkstate(parser);
        freeTkList(tokens);
        free(source);
        vm_back_source_info(vm);
        vm_error(vm, VM_MEMORY_ERROR, "Failed to create bytecode generator for module");
        return false;
    }

    if (!generate_bytecode(gen, ast)) {
        destroy_bytecode_generator(gen);
        free_ast(ast);
        destroy_tkstate(parser);
        freeTkList(tokens);
        free(source);
        vm_back_source_info(vm);
        vm_error(vm, VM_LOAD_ERROR, "Failed to generate bytecode for module '%s'", module_name);
        return false;
    }

    vm_load_bytecode(vm, gen);
    VMError result = vm_execute(vm);

    if (result != VM_OK) {
        return result;
        //vm_print_error(vm);
        // Error report had moved into vm_error
    }

    //printf("DEBUG: Module execution finished, restoring PC from %zu to %zu\n", vm->pc, saved_pc);

    /* Restore PC to continue main program execution */

    back_chunk(vm);
    vm_back_source_info(vm);

    /* Clean up resources */
    destroy_bytecode_generator(gen);
    free_ast(ast);
    destroy_tkstate(parser);
    freeTkList(tokens);
    free(source);

    return result == VM_OK;
}

/**
 * Load and execute module
 * Priority order:
 * 1. Check ../lib/sblang/[module_name]/ directory for init.sb -> [module_name].sb -> [module_name].sbc
 * 2. Original loading logic: .so/.dll -> .sbc -> .sb in current directory
 */
bool load_module(_sbVM* vm, const char* module_name) {
    char filename[512];
    char exe_dir[PATH_MAX];

    /* First, try to check in ../lib/sblang directory relative to executable */
    if (get_executable_dir(exe_dir, sizeof(exe_dir))) {
        char lib_path[PATH_MAX];
        snprintf(lib_path, sizeof(lib_path), "%s/../lib/sblang/%s", exe_dir, module_name);

        /* Check if the target path (without extension) is a directory */
        if (is_directory(lib_path)) {
            /* Target is a directory - try to load files in order */

            /* 1. Try init.sb first */
            snprintf(filename, sizeof(filename), "%s/init.sb", lib_path);
            if (file_exists(filename)) {
                if (load_source_file(vm, filename, module_name)) {
                    return true;
                }
            }

            /* 2. Try [module_name].sb */
            snprintf(filename, sizeof(filename), "%s/%s.sb", lib_path, module_name);
            if (file_exists(filename)) {
                if (load_source_file(vm, filename, module_name)) {
                    return true;
                }
            }

            /* 3. Try [module_name].sbc */
            snprintf(filename, sizeof(filename), "%s/%s.sbc", lib_path, module_name);
            if (file_exists(filename)) {
                if (load_bytecode_file(vm, filename, module_name)) {
                    return true;
                }
            }

            /* Directory exists but no valid files found */
            vm_error(vm, VM_LOAD_ERROR, "Module directory '%s' exists but contains no valid module files (init.sb, %s.sb, or %s.sbc)", lib_path, module_name, module_name);
            return false;
        }
        else {
            /* Target is not a directory - try to load files directly in lib_path */

            /* Try .so/.dll */
            char lib_path_dy[PATH_MAX];
            snprintf(lib_path_dy, sizeof(lib_path_dy), "%s/../lib/sblang/lib%s", exe_dir, module_name);
            snprintf(filename, sizeof(filename), "%s%s", lib_path_dy, SHARED_LIB_EXT);
            //printf("--- DEBUG: so library: %s", filename);
            if (file_exists(filename)) {
                if (load_shared_library(vm, filename, module_name)) {
                    return true;
                }
            }

            /* Try .sbc */
            snprintf(filename, sizeof(filename), "%s.sbc", lib_path);
            if (file_exists(filename)) {
                if (load_bytecode_file(vm, filename, module_name)) {
                    return true;
                }
            }

            /* Try .sb */
            snprintf(filename, sizeof(filename), "%s.sb", lib_path);
            if (file_exists(filename)) {
                if (load_source_file(vm, filename, module_name)) {
                    return true;
                }
            }
        }
    }

    /* If not found in lib directory, fall back to original loading logic */

    /* First try to load shared library (.so or .dll) */
    char script_path[PATH_MAX];
    char buffer[PATH_MAX];
    realpath(vm->source_filename, buffer);
    strcpy(script_path, buffer);
    dirname(script_path);
    snprintf(filename, sizeof(filename), "%s/lib%s%s", script_path, module_name, SHARED_LIB_EXT);
    //printf("--- DEBUG: so library: %s", filename);
    if (file_exists(filename)) {
        if (load_shared_library(vm, filename, module_name)) {
            return true;
        }
        /* If shared library exists but failed to load, continue trying other formats */
        vm_error(vm, VM_LOAD_ERROR, "Failed to load shared library '%s'", filename);
    }

    /* Then try to load bytecode file (.sbc) */
    snprintf(filename, sizeof(filename), "%s/%s.sbc", script_path, module_name);
    if (file_exists(filename)) {
        if (load_bytecode_file(vm, filename, module_name)) {
            return true;
        }
        /* If bytecode file exists but failed to load, continue trying source file */
        vm_error(vm, VM_LOAD_ERROR, "Failed to load bytecode file '%s'", filename);
    }

    /* Finally try to load source file (.sb) */
    snprintf(filename, sizeof(filename), "%s/%s.sb", script_path, module_name);
    if (file_exists(filename)) {
        if (load_source_file(vm, filename, module_name)) {
            return true;
        }
        vm_error(vm, VM_LOAD_ERROR, "Failed to load source file '%s'", filename);
        return false;
    }

    /* No module file found */
    vm_error(vm, VM_LOAD_ERROR, "Cannot load module '%s': no compatible file found in ../lib/sblang/ or current directory", module_name);
    return false;
}

/* ========== Instruction Execution Functions ========== */

/**
 * Clean up static buffers used for function call tracing
 */
void vm_cleanup_static_buffers(_sbVM* vm) {
    // Clean up any remaining source filename copies
    for (int i = 0; i < vm->sfp_count; i++) { // Clean entire array
        if (vm->sfp[i]) {
            free(vm->sfp[i]);
            vm->sfp[i] = nullptr;
        }
    }

    vm->sfp_count = 0;
    vm->bc_count = 0;
    vm->sf_traceback_count = 0;
    vm->sf_traceback_capacity = 0;

    free(vm->sfp);
    vm->sfp = nullptr;

    free(vm->sf_traceback);
    vm->sf_traceback = nullptr;

    free(vm->bc);
    vm->bc = nullptr;

    // printf("DEBUG: Freed %d static buffers\n", freed_count);
}

/**
 * Execute single instruction
 */
VMError vm_execute_instruction(_sbVM* vm) {

    if (!vm || vm->pc >= vm->instruction_count) {
        return VM_RUNTIME_ERROR;
    }

    Instruction* inst = &vm->instructions[vm->pc++];

    save_chunk(vm);

    switch (inst->opcode) {
        case OP_NOP:
            // No operation
            break;

        case OP_PUSH_NUM:
            // Push numeric constant
            vm_push(vm, create_number(inst->operand.num_value));
            break;

        case OP_PUSH_STR:
            // Push string constant
            vm_push(vm, create_string(vm, inst->operand.str_value));
            break;

        case OP_PUSH_IDENT:
            // Push identifier (as string)
            vm_push(vm, create_string(vm, inst->operand.str_value));
            break;

        case OP_PUSH_TRUE:
            // Push true
            vm_push(vm, create_bool(true));
            break;

        case OP_PUSH_FALSE:
            // Push false
            vm_push(vm, create_bool(false));
            break;

        case OP_PUSH_NULL:
            // Push null
            vm_push(vm, create_null());
            break;

        case OP_POP:
            // Pop value from stack
            vm_pop(vm);
            break;

        case OP_DUP: {
            // Duplicate top value
            if (vm->stack_top == 0) {
                vm_error(vm, VM_STACK_UNDERFLOW, "Cannot duplicate empty stack");
                return VM_STACK_UNDERFLOW;
            }
            _sbValue val = vm_peek(vm, 0);
            vm_push(vm, val);
            break;
        }

        case OP_ADD: {
            // Addition operation (supports numbers and strings)
            _sbValue b = vm_pop(vm);
            _sbValue a = vm_pop(vm);

            if (a.type == VAL_NUMBER && b.type == VAL_NUMBER) {
                vm_push(vm, create_number(a.as.number + b.as.number));
            }
            else if (a.type == VAL_STRING && b.type == VAL_STRING) {
                // String concatenation
                size_t len = strlen(a.as.string) + strlen(b.as.string) + 1;
                char* result = (char*)calloc(len, sizeof(char));
                if (result) {
                    strcpy(result, a.as.string);
                    strcat(result, b.as.string);
                    _sbValue str_val = create_string(vm, result);
                    free(result);
                    vm_push(vm, str_val);
                }
            }
            else {
                vm_error(vm, VM_TYPE_ERROR, "Invalid operands for addition (should be `string + string` or `number + number`)");
                //printf("---- DEBUG: a: %d, b: %d \n", a.type, b.type);
                return VM_TYPE_ERROR;
            }

            free_value_gc(vm, a);
            free_value_gc(vm, b);
            break;
        }

        case OP_SUB: {
            // Subtraction operation
            _sbValue b = vm_pop(vm);
            _sbValue a = vm_pop(vm);

            if (a.type != VAL_NUMBER || b.type != VAL_NUMBER) {
                vm_error(vm, VM_TYPE_ERROR, "Subtraction requires numbers");
                return VM_TYPE_ERROR;
            }

            vm_push(vm, create_number(a.as.number - b.as.number));
            break;
        }

        case OP_MUL: {
            // Multiplication operation
            _sbValue b = vm_pop(vm);
            _sbValue a = vm_pop(vm);

            if (a.type != VAL_NUMBER || b.type != VAL_NUMBER) {
                vm_error(vm, VM_TYPE_ERROR, "Multiplication requires numbers");
                return VM_TYPE_ERROR;
            }

            vm_push(vm, create_number(a.as.number * b.as.number));
            break;
        }

        case OP_DIV: {
            // Division operation
            _sbValue b = vm_pop(vm);
            _sbValue a = vm_pop(vm);

            if (a.type != VAL_NUMBER || b.type != VAL_NUMBER) {
                vm_error(vm, VM_TYPE_ERROR, "Division requires numbers");
                return VM_TYPE_ERROR;
            }

            if (b.as.number == 0) {
                vm_error(vm, VM_DIVISION_BY_ZERO, "Division by zero");
                return VM_DIVISION_BY_ZERO;
            }

            vm_push(vm, create_number(a.as.number / b.as.number));
            break;
        }

        case OP_MOD: {
            // Modulo operation
            _sbValue b = vm_pop(vm);
            _sbValue a = vm_pop(vm);

            if (a.type != VAL_NUMBER || b.type != VAL_NUMBER) {
                vm_error(vm, VM_TYPE_ERROR, "Modulo requires numbers");
                return VM_TYPE_ERROR;
            }

            if (b.as.number == 0) {
                vm_error(vm, VM_DIVISION_BY_ZERO, "Modulo by zero");
                return VM_DIVISION_BY_ZERO;
            }

            vm_push(vm, create_number(fmod(a.as.number, b.as.number)));
            break;
        }

        case OP_POW: {
            // Power operation
            _sbValue b = vm_pop(vm);
            _sbValue a = vm_pop(vm);

            if (a.type != VAL_NUMBER || b.type != VAL_NUMBER) {
                vm_error(vm, VM_TYPE_ERROR, "Power requires numbers");
                return VM_TYPE_ERROR;
            }

            vm_push(vm, create_number(pow(a.as.number, b.as.number)));
            break;
        }

        case OP_BIT_AND: {
            // Bitwise AND operation
            _sbValue b = vm_pop(vm);
            _sbValue a = vm_pop(vm);

            if (a.type != VAL_NUMBER || b.type != VAL_NUMBER) {
                vm_error(vm, VM_TYPE_ERROR, "Bitwise AND requires numbers");
                return VM_TYPE_ERROR;
            }

            vm_push(vm, create_number((double)((int)a.as.number & (int)b.as.number)));
            break;
        }

        case OP_BIT_OR: {
            // Bitwise OR operation
            _sbValue b = vm_pop(vm);
            _sbValue a = vm_pop(vm);

            if (a.type != VAL_NUMBER || b.type != VAL_NUMBER) {
                vm_error(vm, VM_TYPE_ERROR, "Bitwise OR requires numbers");
                return VM_TYPE_ERROR;
            }

            vm_push(vm, create_number((double)((int)a.as.number | (int)b.as.number)));
            break;
        }

        case OP_BIT_XOR: {
            // Bitwise XOR operation
            _sbValue b = vm_pop(vm);
            _sbValue a = vm_pop(vm);

            if (a.type != VAL_NUMBER || b.type != VAL_NUMBER) {
                vm_error(vm, VM_TYPE_ERROR, "Bitwise XOR requires numbers");
                return VM_TYPE_ERROR;
            }

            vm_push(vm, create_number((double)((int)a.as.number ^ (int)b.as.number)));
            break;
        }

        case OP_BIT_NOT: {
            // Bitwise NOT operation
            _sbValue a = vm_pop(vm);

            if (a.type != VAL_NUMBER) {
                vm_error(vm, VM_TYPE_ERROR, "Bitwise NOT requires number");
                return VM_TYPE_ERROR;
            }

            vm_push(vm, create_number((double)(~(int)a.as.number)));
            break;
        }

        case OP_BIT_LSHIFT: {
            // Left shift operation
            _sbValue b = vm_pop(vm);
            _sbValue a = vm_pop(vm);

            if (a.type != VAL_NUMBER || b.type != VAL_NUMBER) {
                vm_error(vm, VM_TYPE_ERROR, "Left shift requires numbers");
                return VM_TYPE_ERROR;
            }

            vm_push(vm, create_number((double)((int)a.as.number << (int)b.as.number)));
            break;
        }

        case OP_BIT_RSHIFT: {
            // Right shift operation
            _sbValue b = vm_pop(vm);
            _sbValue a = vm_pop(vm);

            if (a.type != VAL_NUMBER || b.type != VAL_NUMBER) {
                vm_error(vm, VM_TYPE_ERROR, "Right shift requires numbers");
                return VM_TYPE_ERROR;
            }

            vm_push(vm, create_number((double)((int)a.as.number >> (int)b.as.number)));
            break;
        }

        case OP_LOGIC_AND: {
            // Logical AND operation
            _sbValue b = vm_pop(vm);
            _sbValue a = vm_pop(vm);

            vm_push(vm, create_bool(is_truthy(a) && is_truthy(b)));
            free_value_gc(vm, a);
            free_value_gc(vm, b);
            break;
        }

        case OP_LOGIC_OR: {
            // Logical OR operation
            _sbValue b = vm_pop(vm);
            _sbValue a = vm_pop(vm);

            vm_push(vm, create_bool(is_truthy(a) || is_truthy(b)));
            free_value_gc(vm, a);
            free_value_gc(vm, b);
            break;
        }

        case OP_LOGIC_NOT: {
            // Logical NOT operation
            _sbValue a = vm_pop(vm);
            vm_push(vm, create_bool(!is_truthy(a)));
            free_value(a);
            break;
        }

        case OP_EQ: {
            // Equality comparison
            _sbValue b = vm_pop(vm);
            _sbValue a = vm_pop(vm);

            vm_push(vm, create_bool(values_equal(a, b)));
            free_value_gc(vm, a);
            free_value_gc(vm, b);
            break;
        }

        case OP_NEQ: {
            // Inequality comparison
            _sbValue b = vm_pop(vm);
            _sbValue a = vm_pop(vm);

            vm_push(vm, create_bool(!values_equal(a, b)));
            free_value_gc(vm, a);
            free_value_gc(vm, b);
            break;
        }

        case OP_LT: {
            // Less than comparison
            _sbValue b = vm_pop(vm);
            _sbValue a = vm_pop(vm);

            if (a.type != VAL_NUMBER || b.type != VAL_NUMBER) {
                vm_error(vm, VM_TYPE_ERROR, "Comparison requires numbers");
                return VM_TYPE_ERROR;
            }

            vm_push(vm, create_bool(a.as.number < b.as.number));
            break;
        }

        case OP_GT: {
            // Greater than comparison
            _sbValue b = vm_pop(vm);
            _sbValue a = vm_pop(vm);

            if (a.type != VAL_NUMBER || b.type != VAL_NUMBER) {
                vm_error(vm, VM_TYPE_ERROR, "Comparison requires numbers");
                return VM_TYPE_ERROR;
            }

            vm_push(vm, create_bool(a.as.number > b.as.number));
            break;
        }

        case OP_LEQ: {
            // Less than or equal comparison
            _sbValue b = vm_pop(vm);
            _sbValue a = vm_pop(vm);

            if (a.type != VAL_NUMBER || b.type != VAL_NUMBER) {
                vm_error(vm, VM_TYPE_ERROR, "Comparison requires numbers");
                return VM_TYPE_ERROR;
            }

            vm_push(vm, create_bool(a.as.number <= b.as.number));
            break;
        }

        case OP_GEQ: {
            // Greater than or equal comparison
            _sbValue b = vm_pop(vm);
            _sbValue a = vm_pop(vm);

            if (a.type != VAL_NUMBER || b.type != VAL_NUMBER) {
                vm_error(vm, VM_TYPE_ERROR, "Comparison requires numbers");
                return VM_TYPE_ERROR;
            }

            vm_push(vm, create_bool(a.as.number >= b.as.number));
            break;
        }

        case OP_LOAD_GLOBAL:
        case OP_LOAD_VAR: {
            // Load variable value onto stack
            const char* name = inst->operand.str_value;
            _sbValue* var = vm_get_variable(vm, name);

            if (!var) {
                _sbValue* native_var = vm_get_variable(vm, name);
                if (native_var && native_var->type == VAL_NATIVE) {
                    native_var->freed = false;
                    vm_push(vm, *native_var);
                }
                else {
                    vm_error(vm, VM_UNDEFINED_VARIABLE, "Undefined variable '%s'", name);
                    return VM_UNDEFINED_VARIABLE;
                }
            }
            else {
                // Push the value directly for reference types like structs
                // For function arguments, copy_value will be called separately
                switch (var->type) {
                    case VAL_LIST:
                    case VAL_STRUCT_INSTANCE: {
                        // Special types, push reference to allow member modification
                        vm_push(vm, *var);
                        break;
                    }
                    default: {
                        // For other types, make a copy
                        vm_push(vm, copy_value(vm, *var));
                        break;
                    }
                }
            }
            break;
        }

        case OP_STORE_GLOBAL:
        case OP_STORE_VAR: {
            // Store top value to variable
            _sbValue value = vm_pop(vm);
            const char* name = inst->operand.str_value;

            bool success = false;
            if (inst->opcode == OP_STORE_GLOBAL) {
                success = add_variable(vm, &vm->globals, name, value);
            } else {
                success = vm_set_variable(vm, name, value);
            }

            if (!success) {
                vm_error(vm, VM_RUNTIME_ERROR, "Failed to store variable '%s'", name);
                free_value_gc(vm, value);
                return VM_RUNTIME_ERROR;
            }
            // Free the original value since add_variable/vm_set_variable makes a copy
            free_value_gc(vm, value);
            break;
        }

        case OP_JUMP: {
            // Unconditional jump
            vm->pc = inst->operand.int_value;
            break;
        }

        case OP_JUMP_IF_FALSE: {
            // Jump if condition is false
            _sbValue cond = vm_pop(vm);
            if (!is_truthy(cond)) {
                vm->pc = inst->operand.int_value;
            }
            free_value_gc(vm, cond);
            break;
        }

        case OP_JUMP_IF_TRUE: {
            // Jump if condition is true
            _sbValue cond = vm_pop(vm);
            if (is_truthy(cond)) {
                vm->pc = inst->operand.int_value;
            }
            free_value_gc(vm, cond);
            break;
        }

        case OP_CALL: {
            // Function call
            int arg_count = inst->operand.int_value;

            // Get arguments
            _sbValue args[arg_count];
            for (int i = arg_count - 1; i >= 0; i--) {
                _sbValue original = vm_pop(vm);
                // Always make a copy to avoid memory management issues
                if (original.type == VAL_LIST || original.type == VAL_STRUCT_INSTANCE) {
                    args[i] = original;
                }
                else {
                    args[i] = copy_value(vm, original);
                    // Free the original value since we have a copy
                    free_value_gc(vm, original);
                }
            }

            // Get function name
            _sbValue func_name = vm_pop(vm);

            if (func_name.type == VAL_STRING) {
                _sbValue* func_val = vm_get_variable(vm, func_name.as.string);

                if (!func_val) {
                    vm_error(vm, VM_UNDEFINED_FUNCTION, "Undefined function '%s'", func_name.as.string);
                    free_value_gc(vm, func_name);
                    for (int i = 0; i < arg_count; i++) {
                        free_value_gc(vm, args[i]);
                    }
                    return VM_UNDEFINED_FUNCTION;
                }

                if (func_val->type == VAL_NATIVE) {
                    // Call native function
                    vm->error_from_native = true;

                    _sbValue result = func_val->as.native(vm, args, arg_count);
                    if (vm->error) {
                        return vm->last_error;
                    }
                    result.freed = false;
                    vm_push(vm, result);

                    for (int i = 0; i < arg_count; i++) {
                        free_value_gc(vm, args[i]);
                    }

                    vm->error_from_native = false;
                }
                else if (func_val->type == VAL_FUNCTION) {
                    // Call user-defined function
                    _sbVFunction* func = func_val->as.function;

                    // Check argument count
                    if (func->param_count != (size_t)arg_count) {
                        vm_error(vm, VM_ARGUMENT_MISMATCH,
                                "Function '%s' expects %zu arguments, got %d",
                                func_name.as.string, func->param_count, arg_count);
                        free_value_gc(vm, func_name);
                        for (int i = 0; i < arg_count; i++) {
                            free_value_gc(vm, args[i]);
                        }
                        return VM_ARGUMENT_MISMATCH;
                    }

                    // Check if call stack needs expansion
                    if (vm->call_depth >= vm->call_capacity) {
                        size_t new_capacity = vm->call_capacity + VM_INITIAL_CALL_STACK_SIZE;
                        _sbCallFrame* new_call_stack = (_sbCallFrame*)realloc(vm->call_stack, new_capacity * sizeof(_sbCallFrame));
                        if (!new_call_stack) {
                            vm_error(vm, VM_MEMORY_ERROR, "Failed to expand call stack");
                            free_value_gc(vm, func_name);
                            for (int i = 0; i < arg_count; i++) {
                                free_value_gc(vm, args[i]);
                            }
                            return VM_MEMORY_ERROR;
                        }
                        vm->call_stack = new_call_stack;
                        vm->call_capacity = new_capacity;
                    }

                    // Create call frame
                    _sbCallFrame* frame = &vm->call_stack[vm->call_depth++];
                    frame->function = func;
                    frame->return_addr = vm->pc;
                    frame->stack_base = vm->stack_top;  // Save current stack top before pushing args back

                    // Save current local scope and create new one
                    _sbVariableTable* old_locals = vm->locals;
                    vm->locals = (_sbVariableTable*)malloc(sizeof(_sbVariableTable));
                    init_variable_table(vm->locals);

                    // Store old locals in frame for restoration on return
                    frame->locals = (_sbValue*)old_locals;  // Temporarily store pointer
                    frame->local_count = 0;  // Flag to indicate it's a VariableTable pointer

                    // Push arguments back onto stack for the function to use
                    for (int i = 0; i < arg_count; i++) {
                        vm_push(vm, args[i]);
                    }

                    // Jump to function body
                    if (func->source_code_file) {
                        vm_set_source_info(vm, func->source_code_file, false);
                    }

                    step_chunk(vm, func->chunk_id);
                    vm->pc = func->start_addr;  // Set PC to function start
                }
                else {
                    vm_error(vm, VM_TYPE_ERROR, "'%s' is not a function", func_name.as.string);
                    free_value_gc(vm, func_name);
                    for (int i = 0; i < arg_count; i++) {
                        free_value_gc(vm, args[i]);
                    }
                    return VM_TYPE_ERROR;
                }
            }

            free_value_gc(vm, func_name);
            break;
        }

        case OP_RETURN: {
            // Function return
            if (vm->call_depth > 0) {
                // Get return value from stack (if any)
                _sbValue return_value = create_null();
                if (vm->stack_top > 0) {
                    return_value = vm_pop(vm);
                }

                _sbCallFrame* frame = &vm->call_stack[--vm->call_depth];

                // Clean up function's stack frame (remove local variables and parameters)
                vm->stack_top = frame->stack_base;

                // Restore PC
                vm->pc = frame->return_addr;

                // Restore previous chunk context
                back_chunk(vm);

                // Clean up current local variables and restore previous scope
                if (vm->locals) {
                    free_variable_table_gc(vm, vm->locals);
                    free(vm->locals);
                }

                // Restore previous local scope (stored in frame->locals)
                vm->locals = (_sbVariableTable*)frame->locals;

                // Push return value back onto stack
                return_value.freed = false;
                vm_push(vm, return_value);

                // Traceback file level
                if (vm->source_filename) {
                    free(vm->source_filename);
                    vm->source_filename = nullptr;
                }

                vm_back_source_info(vm);
            }
            else {
                return VM_OK; // Main program return
            }
            break;
        }

        case OP_FUNC_START:
        case OP_FUNC_END:
        case OP_FUNC_SET_ARGS:
        case OP_FUNC_DEF:
            // Function definition start/end (skip function body execution)
            /*while (vm->pc < vm->instruction_count &&
                   vm->instructions[vm->pc].opcode != OP_FUNC_END) {
                vm->pc++;
            }
            if (vm->pc < vm->instruction_count) vm->pc++;*/
            break;

        case OP_BLOCK_START:
        case OP_BLOCK_END:
            // Code block start/end (no operation)
            break;

        case OP_LOAD_MODULE: {
            // Load module
            const char* module_name = inst->operand.str_value;
            //printf("DEBUG: OP_LOAD_MODULE at PC=%zu\n", vm->pc - 1);
            if (!load_module(vm, module_name)) {
                return VM_LOAD_ERROR;
            }
            //printf("DEBUG: OP_LOAD_MODULE completed, continuing at PC=%zu\n", vm->pc);
            break;
        }

        case OP_STRUCT_DEF:
            // Register struct
            const char* struct_name = inst->operand.str_value;

            _sbValue member_count = vm_pop(vm);
            if (member_count.type != VAL_NUMBER) {
                vm_error(vm, VM_TYPE_ERROR, "Invalid instruction during running when creating a struct (invalid member count)");
                return VM_TYPE_ERROR;
            }

            char** members = malloc((int)member_count.as.number * sizeof(char*));
            _sbValue _s;

            for (int i = 0; i < member_count.as.number; i++) {
                _s = vm_pop(vm);
                if (_s.type != VAL_STRING) {
                    vm_error(vm, VM_TYPE_ERROR, "Invalid instruction during running when creating a struct (invalid identifier type)");
                    return VM_TYPE_ERROR;
                }
                members[i] = _s_strdup(_s.as.string);
            }

            for (int i = 0; i < vm->struct_count; i++) {
                if (strcmp(struct_name, vm->structs[i].name) == 0) {
                    // Overloaded structures
                    free(vm->structs[i].name);
                    vm->structs[i].name = _s_strdup(struct_name);

                    for (int j = 0; j < vm->structs[i].member_count; j++) {
                        free(vm->structs[i].members[j]);
                    }
                    free(vm->structs[i].members);

                    vm->structs[i].member_count = (size_t)member_count.as.number;
                    vm->structs[i].members = members;
                    goto end_of_create_struct;
                }
            }

            // New struct
            vm->struct_count++;
            vm->structs = realloc(vm->structs, (vm->struct_count + 1) * sizeof(_sbVStruct));
            vm->structs[vm->struct_count - 1].name = _s_strdup(struct_name);
            vm->structs[vm->struct_count - 1].member_count = (size_t)member_count.as.number;
            vm->structs[vm->struct_count - 1].members = members;

            _sbValue _struct = create_struct(vm, &vm->structs[vm->struct_count - 1]);
            vm_define_global(vm, vm->structs[vm->struct_count - 1].name, _struct);

end_of_create_struct:

            break;

        case OP_STRUCT_NEW: {
            // Create struct instance
            const char* struct_name = inst->operand.str_value;

            // Find struct definition
            _sbVStruct* struct_def = nullptr;
            for (size_t i = 0; i < vm->struct_count; i++) {
                if (strcmp(vm->structs[i].name, struct_name) == 0) {
                    struct_def = &vm->structs[i];
                    break;
                }
            }

            // If definition not found, create new struct definition
            if (!struct_def) {
                vm_error(vm, VM_UNDEFINED_FUNCTION, "Struct '%s' not defined", struct_name);
                return VM_UNDEFINED_FUNCTION;
            }

            // Debug: print struct info
            //printf("DEBUG: Creating instance of struct '%s' with %zu members\n", struct_name, struct_def->member_count);

            // Create struct instance using GC
            _sbVStructInstance* instance = (_sbVStructInstance*)gc_alloc(vm, sizeof(_sbVStructInstance), VAL_STRUCT_INSTANCE);
            if (!instance) {
                vm_error(vm, VM_MEMORY_ERROR, "Failed to create struct instance");
                return VM_MEMORY_ERROR;
            }

            instance->struct_def = struct_def;
            instance->members = (_sbValue*)calloc(struct_def->member_count, sizeof(_sbValue));
            if (!instance->members) {
                vm_error(vm, VM_MEMORY_ERROR, "Failed to allocate struct members");
                return VM_MEMORY_ERROR;
            }

            // Initialize member values to null
            for (size_t i = 0; i < struct_def->member_count; i++) {
                instance->members[i] = create_null();
            }

            _sbValue val;
            val.type = VAL_STRUCT_INSTANCE;
            val.as.instance = instance;
            val.freed = false;
            vm_push(vm, val);
            break;
        }

        case OP_MEMBER_ACCESS: {
            // Access struct member
            _sbValue obj = vm_pop(vm);
            const char* member_name = inst->operand.str_value;

            //printf("DEBUG: Accessing member '%s'\n", member_name);

            if (obj.type != VAL_STRUCT_INSTANCE) {
                vm_error(vm, VM_NOT_A_STRUCT, "Cannot access member of non-struct");
                free_value_gc(vm, obj);
                return VM_NOT_A_STRUCT;
            }

            _sbVStructInstance* instance = obj.as.instance;
            size_t member_idx = (size_t)-1;

            // Find member index
            for (size_t i = 0; i < instance->struct_def->member_count; i++) {
                if (instance->struct_def->members && instance->struct_def->members[i] &&
                    strcmp(instance->struct_def->members[i], member_name) == 0) {
                    member_idx = i;
                    break;
                }
            }

            if (member_idx == (size_t)-1) {
                vm_error(vm, VM_UNDEFINED_MEMBER, "Undefined member '%s'", member_name);
                free_value_gc(vm, obj);
                return VM_UNDEFINED_MEMBER;
            }

            //printf("DEBUG: Found member[%zu], type=%d\n", member_idx, instance->members[member_idx].type);
            vm_push(vm, copy_value(vm, instance->members[member_idx]));
            //vm_push(vm, instance->members[member_idx]);
            // Don't free obj since it's a reference type
            break;
        }

        case OP_MEMBER_STORE: {
            // Set struct member value
            _sbValue obj = vm_pop(vm);
            _sbValue value = vm_pop(vm);
            const char* member_name = inst->operand.str_value;

            //printf("DEBUG: Storing member '%s'\n", member_name);
            //printf("DEBUG: Value type=%d\n", value.type);
            //if (value.type == VAL_STRING) {
            //    printf("DEBUG: Value string='%s'\n", value.as.string ? value.as.string : "null");
            //}

            if (obj.type != VAL_STRUCT_INSTANCE) {
                vm_error(vm, VM_NOT_A_STRUCT, "Cannot store member of non-struct");
                free_value_gc(vm, obj);
                free_value_gc(vm, value);
                return VM_NOT_A_STRUCT;
            }

            _sbVStructInstance* instance = obj.as.instance;
            size_t member_idx = (size_t)-1;

            //printf("DEBUG: Struct has %zu members\n", instance->struct_def->member_count);
            //for (size_t i = 0; i < instance->struct_def->member_count; i++) {
            //    printf("DEBUG: Member %zu: '%s'\n", i, instance->struct_def->members[i]);
            //}

            // Find member index
            for (size_t i = 0; i < instance->struct_def->member_count; i++) {
                if (instance->struct_def->members &&
                    strcmp(instance->struct_def->members[i], member_name) == 0) {
                    member_idx = i;
                    break;
                }
            }

            if (member_idx == (size_t)-1) {
                vm_error(vm, VM_UNDEFINED_MEMBER, "Undefined member '%s'", member_name);
                // Don't free obj here since it's a reference type
                free_value_gc(vm, value);
                return VM_UNDEFINED_MEMBER;
            }
            else {
                // Update existing member value
                //printf("DEBUG: Setting member[%zu] to value\n", member_idx);
                free_value_gc(vm, instance->members[member_idx]);
                instance->members[member_idx] = copy_value(vm, value);
                //printf("DEBUG: After copy, member[%zu] type=%d\n", member_idx, instance->members[member_idx].type);
                free_value_gc(vm, value);
                //printf("--- DEBUG\n");
            }
            // Don't free obj since it's a reference type that's still stored in the variable
            break;
        }

        case OP_LIST_NEW: {
            // Create new list
            int count = inst->operand.int_value;
            _sbValue list = create_list(vm);

            if (count > 0 && list.as.list) {
                list.as.list->items = (_sbValue*)malloc(count * sizeof(_sbValue));
                list.as.list->capacity = count;
                list.as.list->count = 0;
            }

            vm_push(vm, list);
            break;
        }

        case OP_LIST_PUSH: {
            // Add element to list
            _sbValue item = vm_pop(vm);
            _sbValue list = vm_pop(vm);

            if (list.type != VAL_LIST) {
                vm_error(vm, VM_TYPE_ERROR, "Cannot push to non-list");
                free_value_gc(vm, item);
                free_value_gc(vm, list);
                return VM_TYPE_ERROR;
            }

            _sbVList* l = list.as.list;
            if (l->count >= l->capacity) {
                // Expand list capacity
                size_t new_capacity = l->capacity == 0 ? 4 : l->capacity * 2;
                _sbValue* new_items = (_sbValue*)realloc(l->items, new_capacity * sizeof(_sbValue));
                if (!new_items) {
                    vm_error(vm, VM_MEMORY_ERROR, "Failed to resize list");
                    free_value(item);
                    free_value(list);
                    return VM_MEMORY_ERROR;
                }
                l->items = new_items;
                l->capacity = new_capacity;
            }

            l->items[l->count++] = item;
            vm_push(vm, list);
            break;
        }

        case OP_LIST_ACCESS: {
            // Access list element
            _sbValue index = vm_pop(vm);
            _sbValue list = vm_pop(vm);

            if (list.type != VAL_LIST) {
                vm_error(vm, VM_TYPE_ERROR, "Cannot index non-list");
                free_value_gc(vm, index);
                free_value_gc(vm, list);
                return VM_TYPE_ERROR;
            }

            if (index.type != VAL_NUMBER) {
                vm_error(vm, VM_TYPE_ERROR, "List index must be number");
                free_value_gc(vm, index);
                free_value_gc(vm, list);
                return VM_TYPE_ERROR;
            }

            int idx = (int)index.as.number;
            if (idx < 0) {
                idx = list.as.list->count + idx;
            }

            if (idx < 0 || idx >= (int)list.as.list->count) {
                vm_error(vm, VM_INDEX_OUT_OF_BOUNDS, "List index out of bounds");
                free_value_gc(vm, index);
                free_value_gc(vm, list);
                return VM_INDEX_OUT_OF_BOUNDS;
            }

            switch (list.as.list->items[idx].type) {
                case VAL_LIST:
                case VAL_STRUCT_INSTANCE: {
                    vm_push(vm, list.as.list->items[idx]);
                }

                default: {
                    vm_push(vm, copy_value(vm, list.as.list->items[idx]));
                }
            }
            break;
        }

        case OP_HALT:
            // Stop execution
            //if (vm->pc == vm->end_pc)
            //    vm->running = false; // End program
            return VM_OK;

        default:
            vm_error(vm, VM_INVALID_OPCODE, "Invalid opcode: %d", inst->opcode);
            return VM_INVALID_OPCODE;
    }

    return VM_OK;
}

/**
 * Execute VM bytecode
 */
VMError vm_execute(_sbVM* vm) {
    if (!vm || !vm->instructions) {
        return VM_RUNTIME_ERROR;
    }

    vm->running = true;
    vm->error = false;
    vm->last_error = VM_OK;

    // Main execution loop
    while (vm->running && vm->pc < vm->instruction_count) {
        VMError error = vm_execute_instruction(vm);
        if (error != VM_OK) {
            vm->running = false;
            //vm_print_error(vm);
            // Error report had moved into vm_error
            return error;
        }
    }

    return VM_OK;
}

/**
 * Load bytecode from bytecode generator to VM
 */
bool vm_load_bytecode(_sbVM* vm, BytecodeGenerator* gen) {
    if (!vm || !gen) return false;

    //print_bytecode(gen);

    size_t jmp_offset = 0;

    size_t id = new_chunk(vm);
    step_chunk(vm, id);

    //vm->instruction_count = gen->instructions->count;
    vm->instructions = (Instruction*)calloc(1, sizeof(Instruction));
    if (!vm->instructions) {
        vm_error(vm, VM_MEMORY_ERROR, "Failed to allocate instruction memory");
        back_chunk(vm);
        return false;
    }

    // Copy instructions
    for (size_t i = 0; i < gen->instructions->count; i++) {
        vm->instruction_count ++;
        vm->instructions = realloc(vm->instructions, (vm->instruction_count) * sizeof(Instruction));
        //memset(&vm->instructions[vm->instruction_count - 1], 0, sizeof(Instruction));
        Instruction* src = (Instruction*)gen->instructions->items[i];
        if (!src) break;

        if (src->opcode == OP_HALT) {
            vm->instructions[vm->instruction_count - 1] = *src;
            break;
        }

        // Copy string operands
        if (src->opcode == OP_PUSH_STR || src->opcode == OP_PUSH_IDENT ||
            src->opcode == OP_LOAD_VAR || src->opcode == OP_STORE_VAR ||
            src->opcode == OP_LOAD_MODULE || src->opcode == OP_FUNC_START ||
            src->opcode == OP_STRUCT_DEF || src->opcode == OP_STRUCT_NEW ||
            src->opcode == OP_MEMBER_ACCESS || src->opcode == OP_MEMBER_STORE ||
            src->opcode == OP_LOAD_GLOBAL || src->opcode == OP_STORE_GLOBAL) {
            if (src->operand.str_value) {
                vm->instructions[vm->instruction_count - 1] = *src;
                vm->instructions[vm->instruction_count - 1].operand.str_value = _s_strdup(src->operand.str_value);
                continue;
            }
        }

        if (src->opcode == OP_JUMP || src->opcode == OP_JUMP_IF_FALSE ||
            src->opcode == OP_JUMP_IF_TRUE ) {
            vm->instructions[vm->instruction_count - 1] = *src;
            vm->instructions[vm->instruction_count - 1].operand.int_value = src->operand.int_value - jmp_offset;
            continue;
        }

        if (src->opcode == OP_FUNC_DEF) {
            vm->instruction_count --;
            jmp_offset += 2;
            size_t jmp_offset_f = jmp_offset;

            size_t function_chunk = new_chunk(vm);
            step_chunk(vm, function_chunk);

            vm->instructions = (Instruction*)calloc(1, sizeof(Instruction));
            size_t inst_count = 0;
            size_t param_count = 0;

            while (true) { // Save function in a new chunk
                Instruction* _src = (Instruction*)gen->instructions->items[i];
                jmp_offset++;
                i++;

                if (_src->opcode == OP_FUNC_DEF) continue;

                if (_src->opcode == OP_FUNC_SET_ARGS) {
                    param_count = _src->operand.int_value;
                    continue;
                }

                vm->instructions = realloc(vm->instructions, (vm->instruction_count + 1) * sizeof(Instruction));

                // Copy string operands
                if (_src->opcode == OP_PUSH_STR || _src->opcode == OP_PUSH_IDENT ||
                    _src->opcode == OP_LOAD_VAR || _src->opcode == OP_STORE_VAR ||
                    _src->opcode == OP_LOAD_MODULE || _src->opcode == OP_FUNC_START ||
                    _src->opcode == OP_STRUCT_DEF || _src->opcode == OP_STRUCT_NEW ||
                    _src->opcode == OP_MEMBER_ACCESS || _src->opcode == OP_MEMBER_STORE ||
                    _src->opcode == OP_LOAD_GLOBAL || _src->opcode == OP_STORE_GLOBAL) {
                    if (src->operand.str_value) {
                        vm->instructions[vm->instruction_count] = *_src;
                        vm->instructions[vm->instruction_count].operand.str_value = _s_strdup(_src->operand.str_value);
                    }
                }
                else if (_src->opcode == OP_JUMP || _src->opcode == OP_JUMP_IF_FALSE ||
                    _src->opcode == OP_JUMP_IF_TRUE ) {
                    vm->instructions[vm->instruction_count] = *_src;
                    vm->instructions[vm->instruction_count].operand.int_value = _src->operand.int_value - jmp_offset_f;
                }
                else
                    vm->instructions[vm->instruction_count] = *_src;

                vm->instruction_count ++;

                if (_src->opcode == OP_FUNC_END)
                    break;
            }
            back_chunk(vm);

            vm->function_count++;
            _sbVFunction* old_functions = vm->functions;
            vm->functions = (_sbVFunction*)realloc(vm->functions, vm->function_count * sizeof(_sbVFunction));
            
            if (!vm->functions) {
                vm_error(vm, VM_RUNTIME_ERROR, "Failed to allocate memory for functions");
                vm->functions = old_functions;
                vm->function_count--;
                return false;
            }
            
            // If realloc moved the array, we need to update all function pointers in global variables
            if (vm->functions != old_functions) {
                ptrdiff_t offset = (char*)vm->functions - (char*)old_functions;
                for (size_t j = 0; j < vm->globals.count; j++) {
                    if (vm->globals.vars[j].value.type == VAL_FUNCTION && 
                        vm->globals.vars[j].value.as.function >= old_functions && 
                        vm->globals.vars[j].value.as.function < old_functions + (vm->function_count - 1)) {
                        vm->globals.vars[j].value.as.function = 
                            (_sbVFunction*)((char*)vm->globals.vars[j].value.as.function + offset);
                    }
                }
            }

            vm->functions[vm->function_count - 1].name = _s_strdup(src->operand.str_value);
            vm->functions[vm->function_count - 1].param_count = param_count;
            vm->functions[vm->function_count - 1].locals = nullptr;
            vm->functions[vm->function_count - 1].start_addr = 0;
            vm->functions[vm->function_count - 1].source_code_file = vm->source_filename ? _s_strdup(vm->source_filename) : _s_strdup("unknown");

            _sbValue func_val = create_function(vm, &vm->functions[vm->function_count - 1], function_chunk);
            vm_define_global(vm, vm->functions[vm->function_count - 1].name, func_val);

            i--;
            continue;
        }

        vm->instructions[vm->instruction_count - 1] = *src;
    }

    // Load function definitions and register them as global variables
    /*if (gen->functions && gen->functions->count > 0) {
        vm->function_count = gen->functions->count;
        vm->functions = (_sbVFunction*)malloc(vm->function_count * sizeof(_sbVFunction));

        for (size_t i = 0; i < vm->function_count; i++) {
            FunctionInfo* src = (FunctionInfo*)gen->functions->items[i];
            vm->functions[i].name = _s_strdup(src->name);
            vm->functions[i].start_addr = src->start_addr;
            vm->functions[i].param_count = src->param_count;
            vm->functions[i].locals = nullptr;

            // Register function as a global variable
            _sbValue func_val = create_function(vm, &vm->functions[i]);
            vm_define_global(vm, vm->functions[i].name, func_val);
        }
    }*/

    // Load struct definitions
    /*
    if (gen->structs && gen->structs->count > 0) {
        vm->struct_count = gen->structs->count;
        vm->structs = (_sbVStruct*)malloc(vm->struct_count * sizeof(_sbVStruct));

        for (size_t i = 0; i < vm->struct_count; i++) {
            StructInfo* src = (StructInfo*)gen->structs->items[i];
            vm->structs[i].name = _s_strdup(src->name);
            vm->structs[i].member_count = src->members ? src->members->count : 0;

            if (vm->structs[i].member_count > 0) {
                vm->structs[i].members = (char**)malloc(vm->structs[i].member_count * sizeof(char*));
                for (size_t j = 0; j < vm->structs[i].member_count; j++) {
                    vm->structs[i].members[j] = _s_strdup((char*)src->members->items[j]);
                }
            }
            else {
                vm->structs[i].members = nullptr;
            }
        }
    }*/

    // Initialize global variables
    /*if (gen->globals) {
        for (size_t i = 0; i < gen->globals->count; i++) {
            char* global_name = (char*)gen->globals->items[i];
            vm_define_global(vm, global_name, create_null());
        }
    }*/

    //vm->pc = 0;

    /*if (vm->running == false)
        vm->end_pc = vm->instruction_count;*/

    save_chunk(vm);
    if (vm->debug) {
        printf("\n=== Debug output while loading bytecodes into new chunks ===\n");
        vm_print_status(vm);
    }
    return true;
}

/**
 * Load bytecode from file to VM
 */
bool vm_load_from_file(_sbVM* vm, const char* filename) {
    if (!vm || !filename) return false;

    BytecodeGenerator* gen = load_bytecode(filename);
    if (!gen) {
        vm_error(vm, VM_LOAD_ERROR, "Failed to load bytecode from file");
        return false;
    }

    bool result = vm_load_bytecode(vm, gen);
    destroy_bytecode_generator(gen);

    return result;
}

/* ========== Garbage Collection Functions ========== */

/**
 * Improved garbage collection with better cycle detection
 */
void vm_gc_collect(_sbVM* vm) {
    if (!vm || !vm->gc_enabled || vm->gc_object_count == 0) return;

    // Phase 1: Reset all mark flags
    _sbGCObject* obj = vm->gc_objects;
    while (obj) {
        obj->marked = false;
        obj = obj->next;
    }

    // Phase 2: Mark all reachable objects from roots
    vm_gc_mark_roots(vm);

    // Phase 3: Sweep unmarked objects
    vm_gc_sweep(vm);

    // Update threshold for next GC
    vm->gc_threshold = vm->gc_bytes_allocated * 2;
    if (vm->gc_threshold < 1024 * 1024) {
        vm->gc_threshold = 1024 * 1024; // Minimum 1MB
    }
}

/**
 * Improved mark function with cycle detection
 */
void vm_gc_mark(_sbVM* vm, _sbValue value) {
    if (!vm || !vm->gc_objects) return;

    // Find the GC object for this value and mark it
    _sbGCObject* obj = vm->gc_objects;
    while (obj) {
        bool should_mark = false;

        switch (value.type) {
            case VAL_STRING:
                if (obj->type == VAL_STRING && obj->data == value.as.string) {
                    should_mark = true;
                }
                break;
            case VAL_LIST:
                if (obj->type == VAL_LIST && obj->data == value.as.list) {
                    should_mark = true;
                }
                break;
            case VAL_STRUCT_INSTANCE:
                if (obj->type == VAL_STRUCT_INSTANCE && obj->data == value.as.instance) {
                    should_mark = true;
                }
                break;
            default:
                break;
        }

        if (should_mark && !obj->marked) {
            obj->marked = true;

            // Mark referenced objects (with cycle protection via marked flag)
            if (value.type == VAL_LIST && value.as.list && value.as.list->items) {
                for (size_t i = 0; i < value.as.list->count; i++) {
                    vm_gc_mark(vm, value.as.list->items[i]);
                }
            } else if (value.type == VAL_STRUCT_INSTANCE && value.as.instance && value.as.instance->members) {
                for (size_t i = 0; i < value.as.instance->struct_def->member_count; i++) {
                    vm_gc_mark(vm, value.as.instance->members[i]);
                }
            }
            break; // Object found and marked, exit search loop
        }

        obj = obj->next;
    }
}

/**
 * Improved sweep function with proper memory management
 */
void vm_gc_sweep(_sbVM* vm) {
    if (!vm) return;

    _sbGCObject** current = &vm->gc_objects;
    size_t freed_count = 0;
    size_t freed_bytes = 0;

    while (*current) {
        if (!(*current)->marked) {
            // Object is not marked, free it
            _sbGCObject* to_free = *current;
            *current = to_free->next;

            // Calculate size before freeing data
            if (to_free->data) {
                freed_bytes += sizeof(_sbGCObject);
                switch (to_free->type) {
                    case VAL_STRING:
                        freed_bytes += strlen(to_free->data) + 1;
                        break;
                    case VAL_LIST:
                        freed_bytes += sizeof(_sbVList);
                        break;
                    case VAL_STRUCT_INSTANCE:
                        freed_bytes += sizeof(_sbVStructInstance);
                        break;
                    default:
                        break;
                }
                
                // Now free the actual data after size calculation
                gc_free_data_by_type(vm, to_free->type, to_free->data);
            }

            free(to_free);
            freed_count++;
        } else {
            // Object is marked, unmark it for next collection and move to next
            (*current)->marked = false;
            current = &(*current)->next;
        }
    }

    vm->gc_object_count -= freed_count;
    vm->gc_bytes_allocated = (vm->gc_bytes_allocated > freed_bytes) ?
                            (vm->gc_bytes_allocated - freed_bytes) : 0;
}

/**
 * Allocate memory with GC tracking
 */
void* gc_alloc(_sbVM* vm, size_t size, _sbValueType type) {
    if (!vm) return nullptr;

    // Check if we should trigger GC
    if (vm->gc_enabled && vm->gc_bytes_allocated >= vm->gc_threshold) {
        vm_gc_collect(vm);
    }

    // Allocate GC object header
    _sbGCObject* obj = (_sbGCObject*)calloc(1, sizeof(_sbGCObject));
    if (!obj) return nullptr;

    // Allocate the actual data
    void* data = malloc(size);
    //memset(data, 0, size);
    if (!data) {
        free(obj);
        return nullptr;
    }

    // Initialize GC object
    obj->next = vm->gc_objects;
    obj->marked = false;
    obj->type = type;
    obj->data = data;

    // Add to GC object list
    vm->gc_objects = obj;
    vm->gc_object_count++;
    vm->gc_bytes_allocated += size + sizeof(_sbGCObject);

    return data;
}

/**
 * Free GC object manually
 */
void gc_free_object(_sbVM* vm, _sbGCObject* obj) {
    if (!vm || !obj) return;

    // Remove from linked list
    if (vm->gc_objects == obj) {
        vm->gc_objects = obj->next;
    } else {
        _sbGCObject* current = vm->gc_objects;
        while (current && current->next != obj) {
            current = current->next;
        }
        if (current) {
            current->next = obj->next;
        }
    }

    // Free the data based on type
    switch (obj->type) {
        case VAL_STRING:
            free(obj->data);
            break;
        case VAL_LIST: {
            _sbVList* list = (_sbVList*)obj->data;
            if (list->items) {
                free(list->items);
            }
            free(list);
            break;
        }
        case VAL_STRUCT_INSTANCE: {
            _sbVStructInstance* instance = (_sbVStructInstance*)obj->data;
            if (instance->members) {
                free(instance->members);
            }
            free(instance);
            break;
        }
        default:
            free(obj->data);
            break;
    }

    free(obj);
    vm->gc_object_count--;
}

/**
 * Mark all reachable objects from roots
 */
void vm_gc_mark_roots(_sbVM* vm) {
    if (!vm) return;
    
    // Mark all objects on the operand stack
    for (size_t i = 0; i < vm->stack_top; i++) {
        vm_gc_mark(vm, vm->stack[i]);
    }
    
    // Mark all global variables
    for (size_t i = 0; i < vm->globals.count; i++) {
        vm_gc_mark(vm, vm->globals.vars[i].value);
    }
    
    // Mark all local variables if there are any
    if (vm->locals) {
        for (size_t i = 0; i < vm->locals->count; i++) {
            vm_gc_mark(vm, vm->locals->vars[i].value);
        }
    }
    
    // Mark all call frame local variables
    for (size_t frame = 0; frame < vm->call_depth; frame++) {
        if (vm->call_stack[frame].locals) {
            for (size_t i = 0; i < vm->call_stack[frame].local_count; i++) {
                vm_gc_mark(vm, vm->call_stack[frame].locals[i]);
            }
        }
    }
}
