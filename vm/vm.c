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
static void free_variable_table_gc(VM* vm, VariableTable* table);

/* Initialize variable table */
static void init_variable_table(VariableTable* table) {
    table->vars = nullptr;
    table->count = 0;
    table->capacity = 0;
}

/* Free variable table and all its variables */
static void free_variable_table(VariableTable* table) {
    free_variable_table_gc(nullptr, table);
}

/* Free variable table and all its variables (GC-aware version) */
static void free_variable_table_gc(VM* vm, VariableTable* table) {
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

/* Find variable in variable table */
static Variable* find_variable(VariableTable* table, const char* name) {
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
static bool add_variable(VM* vm, VariableTable* table, const char* name, Value value) {
    if (!table || !name) return false;

    /* Check if variable already exists */
    Variable* existing = find_variable(table, name);
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
        Variable* new_vars = (Variable*)realloc(table->vars, new_capacity * sizeof(Variable));
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
VM* create_vm() {
    //printf("DEBUG: Starting create_vm\n");
    VM* vm = (VM*)malloc(sizeof(VM));
    if (!vm) return nullptr;

    //printf("DEBUG: VM allocated, initializing fields\n");

    /* Initialize instruction-related fields */
    vm->instructions = nullptr;
    vm->instruction_count = 0;
    vm->pc = 0; // Program counter

    /* Initialize dynamic stack */
    vm->stack = (Value*)malloc(VM_INITIAL_STACK_SIZE * sizeof(Value));
    vm->stack_top = 0;
    vm->stack_capacity = VM_INITIAL_STACK_SIZE;
    
    /* Initialize dynamic call stack */
    vm->call_stack = (CallFrame*)malloc(VM_INITIAL_CALL_STACK_SIZE * sizeof(CallFrame));
    vm->call_depth = 0;
    vm->call_capacity = VM_INITIAL_CALL_STACK_SIZE;
    
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
    vm->functions = nullptr;
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

    if (vm) {
        /* Register built-in functions */
        register_builtin_functions(vm);
    }

    return vm;
}

/**
 * Destroy virtual machine instance and free all resources
 */
void destroy_vm(VM* vm) {
    if (!vm) return;

    vm->running = false;
    
    // Disable GC during cleanup to prevent interference
    vm->gc_enabled = false;
    
    // Free dynamic stack and all values on it with GC awareness
    if (vm->stack) {
        for (size_t i = 0; i < vm->stack_top; i++) {
            free_value_gc(vm, vm->stack[i]);
        }
        free(vm->stack);
        vm->stack = nullptr;
    }

    // Free dynamic call stack
    if (vm->call_stack) {
        free(vm->call_stack);
        vm->call_stack = nullptr;
    }

    // Free global and local variable tables with GC awareness
    free_variable_table_gc(vm, &vm->globals);
    if (vm->locals) {
        free_variable_table_gc(vm, vm->locals);
        free(vm->locals);
        vm->locals = nullptr;
    }

    // Now clean up any remaining GC objects that weren't freed by above operations
    GCObject* current = vm->gc_objects;
    while (current) {
        GCObject* next = current->next;
        
        // Only free if not already marked as freed (data != nullptr)
        if (current->data) {
            switch (current->type) {
                case VAL_STRING:
                    free(current->data);
                    break;
                case VAL_LIST: {
                    List* list = (List*)current->data;
                    if (list->items) {
                        free(list->items);
                    }
                    free(list);
                    break;
                }
                case VAL_STRUCT_INSTANCE: {
                    StructInstance* instance = (StructInstance*)current->data;
                    if (instance->members) {
                        free(instance->members);
                    }
                    free(instance);
                    break;
                }
                default:
                    free(current->data);
                    break;
            }
        }
        
        free(current);
        current = next;
    }
    vm->gc_objects = nullptr;
    vm->gc_object_count = 0;
    vm->gc_bytes_allocated = 0;

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

    // Free function definitions
    if (vm->functions) {
        for (size_t i = 0; i < vm->function_count; i++) {
            if (vm->functions[i].name) {
                free(vm->functions[i].name);
                vm->functions[i].name = nullptr;
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

    free(vm);
}

/**
 * Push value onto VM stack
 */
void vm_push(VM* vm, Value value) {
    if (!vm) return;

    /* Check if stack needs expansion */
    if (vm->stack_top >= vm->stack_capacity) {
        size_t new_capacity = vm->stack_capacity * 2;
        Value* new_stack = (Value*)realloc(vm->stack, new_capacity * sizeof(Value));
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
Value vm_pop(VM* vm) {
    if (!vm || vm->stack_top == 0) {
        if (vm) vm_error(vm, VM_STACK_UNDERFLOW, "Stack underflow");
        return create_null();
    }

    return vm->stack[--vm->stack_top];
}

/**
 * Peek at value at distance from top of stack (without popping)
 */
Value vm_peek(VM* vm, int distance) {
    if (!vm || vm->stack_top <= (size_t)distance) {
        return create_null();
    }

    return vm->stack[vm->stack_top - 1 - distance];
}

/* ========== Value Creation Functions ========== */

/**
 * Create null value
 */
Value create_null() {
    Value val;
    val.type = VAL_NULL;
    return val;
}

/**
 * Create number value
 */
Value create_number(double num) {
    Value val;
    val.type = VAL_NUMBER;
    val.as.number = num;
    return val;
}

/**
 * Create string value (copies the string)
 */
Value create_string(VM* vm, const char* str) {
    Value val;
    val.type = VAL_STRING;
    
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
Value create_bool(bool b) {
    Value val;
    val.type = VAL_BOOL;
    val.as.boolean = b;
    return val;
}

/**
 * Create function value
 */
Value create_function(Function* func) {
    Value val;
    val.type = VAL_FUNCTION;
    val.as.function = func;
    return val;
}

/**
 * Create native function value
 */
Value create_native(NativeFunction func) {
    Value val;
    val.type = VAL_NATIVE;
    val.as.native = func;
    return val;
}

/**
 * Create empty list value
 */
Value create_list(VM* vm) {
    Value val;
    val.type = VAL_LIST;
    
    if (vm && vm->gc_enabled) {
        val.as.list = (List*)gc_alloc(vm, sizeof(List), VAL_LIST);
    } else {
        val.as.list = (List*)malloc(sizeof(List));
    }
    
    if (val.as.list) {
        val.as.list->items = nullptr;
        val.as.list->count = 0;
        val.as.list->capacity = 0;
    }
    return val;
}

/**
 * Check if value is truthy (truthiness evaluation)
 */
bool is_truthy(Value value) {
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
bool values_equal(Value a, Value b) {
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
Value copy_value(VM* vm, Value value) {
    Value result;
    result.type = value.type;
    
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
                    // Use GC allocation
                    size_t len = strlen(value.as.string) + 1;
                    result.as.string = (char*)gc_alloc(vm, len, VAL_STRING);
                    if (result.as.string) {
                        strcpy(result.as.string, value.as.string);
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
                    result.as.list = (List*)gc_alloc(vm, sizeof(List), VAL_LIST);
                } else {
                    result.as.list = malloc(sizeof(List));
                }
                result.as.list->capacity = value.as.list->capacity;
                result.as.list->count = value.as.list->count;
                if (value.as.list->items && value.as.list->count > 0) {
                    result.as.list->items = malloc(sizeof(Value) * value.as.list->capacity);
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
                    result.as.instance = (StructInstance*)gc_alloc(vm, sizeof(StructInstance), VAL_STRUCT_INSTANCE);
                } else {
                    result.as.instance = (StructInstance*)malloc(sizeof(StructInstance));
                }
                result.as.instance->members = (Value*)malloc(sizeof(Value) * value.as.instance->struct_def->member_count);
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
 * Check if pointer is GC-managed and remove it from GC tracking if found
 * Returns true if the object was found and removed, false otherwise
 */
static bool remove_from_gc_if_managed(VM* vm, void* ptr) {
    if (!vm || !ptr) return false;
    
    GCObject* prev = nullptr;
    GCObject* current = vm->gc_objects;
    
    while (current) {
        if (current->data == ptr) {
            // Remove from linked list
            if (prev) {
                prev->next = current->next;
            } else {
                vm->gc_objects = current->next;
            }
            
            // Free the GC object header but not the data (caller will handle data)
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
 * Mark object as freed by nulling its data pointer in GC tracking
 * This prevents double-free while keeping the GC object in the list for cleanup
 */
static bool mark_gc_object_as_freed(VM* vm, void* ptr) {
    if (!vm || !ptr) return false;
    
    GCObject* current = vm->gc_objects;
    while (current) {
        if (current->data == ptr) {
            current->data = nullptr;  // Mark as already freed
            return true;
        }
        current = current->next;
    }
    return false;
}

/**
 * Check if pointer is GC-managed
 */
bool is_gc_managed(VM* vm, void* ptr) {
    if (!vm || !ptr) return false;
    
    GCObject* obj = vm->gc_objects;
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
void free_value_gc(VM* vm, Value value) {
    if (!vm) {
        free_value(value);
        return;
    }
    
    switch (value.type) {
        case VAL_STRING:
            // Mark as freed in GC tracking and free the string
            if (value.as.string && mark_gc_object_as_freed(vm, value.as.string)) {
                free(value.as.string);
            } else if (value.as.string) {
                // Not GC managed, free directly
                free(value.as.string);
            }
            break;
        case VAL_LIST:
            if (value.as.list && mark_gc_object_as_freed(vm, value.as.list)) {
                if (value.as.list->items) {
                    // Free all elements in the list
                    for (size_t i = 0; i < value.as.list->count; i++) {
                        free_value_gc(vm, value.as.list->items[i]);
                    }
                    free(value.as.list->items);
                }
                free(value.as.list);
            } else if (value.as.list) {
                if (value.as.list->items) {
                    for (size_t i = 0; i < value.as.list->count; i++) {
                        free_value_gc(vm, value.as.list->items[i]);
                    }
                    free(value.as.list->items);
                }
                free(value.as.list);
            }
            break;
        case VAL_STRUCT_INSTANCE:
            if (value.as.instance && mark_gc_object_as_freed(vm, value.as.instance)) {
                if (value.as.instance->members && value.as.instance->struct_def) {
                    // Free all members of struct instance
                    for (size_t i = 0; i < value.as.instance->struct_def->member_count; i++) {
                        free_value_gc(vm, value.as.instance->members[i]);
                    }
                    free(value.as.instance->members);
                }
                free(value.as.instance);
            } else if (value.as.instance) {
                if (value.as.instance->members && value.as.instance->struct_def) {
                    for (size_t i = 0; i < value.as.instance->struct_def->member_count; i++) {
                        free_value_gc(vm, value.as.instance->members[i]);
                    }
                    free(value.as.instance->members);
                }
                free(value.as.instance);
            }
            break;
        default:
            // Other types don't allocate heap memory
            break;
    }
}

/**
 * Free memory occupied by value
 */
void free_value(Value value) {
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
        default:
            break; // Other types don't need special handling
    }
}

/* ========== Error Handling Functions ========== */

/**
 * Set VM error state and error message
 */
void vm_error(VM* vm, VMError error, const char* format, ...) {
    if (!vm) return;

    vm->last_error = error;
    vm->running = false;

    if (vm->error_message) {
        free(vm->error_message);
        vm->error_message = nullptr;
    }

    char buffer[512];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    vm->error_message = _s_strdup(buffer);
}

/**
 * Get error string for error code
 */
const char* vm_error_string(VMError error) {
    switch (error) {
        case VM_OK: return "OK";
        case VM_RUNTIME_ERROR: return "RuntimeError";
        case VM_STACK_OVERFLOW: return "StackOverflowError";
        case VM_STACK_UNDERFLOW: return "StackUnderflowError";
        case VM_UNDEFINED_VARIABLE: return "UndefinedVariableError";
        case VM_TYPE_ERROR: return "TypeError";
        case VM_DIVISION_BY_ZERO: return "DivisionByZeroError";
        case VM_INDEX_OUT_OF_BOUNDS: return "IndexOutOfBoundsError";
        case VM_UNDEFINED_FUNCTION: return "UndefinedFunctionError";
        case VM_ARGUMENT_MISMATCH: return "ArgumentMismatchError";
        case VM_LOAD_ERROR: return "LoadError";
        case VM_MEMORY_ERROR: return "MemoryError";
        case VM_INVALID_OPCODE: return "InvalidOpcodeError";
        case VM_UNDEFINED_MEMBER: return "UndefinedMemberError";
        case VM_NOT_A_STRUCT: return "NotAStructError";
        default: return "UnknownError";
    }
}

/**
 * Print error information to stderr
 */
void vm_print_error(VM* vm) {
    if (!vm) return;

    vm->pc--;

    fprintf(stderr, "An error occurred during running\n");

    fprintf(stderr, "%s", vm_error_string(vm->last_error));
    if (vm->error_message) {
        fprintf(stderr, ": %s", vm->error_message);
    }
    fprintf(stderr, "\n");

    if (vm->pc < vm->instruction_count) {
        Instruction* current_inst = &vm->instructions[vm->pc];
        
        // Show location information
        if (vm->is_bytecode_execution) {
            fprintf(stderr, "  at instruction %zu in <bytecode>\n", vm->pc);
        } else if (vm->source_filename && current_inst->source_line >= 0) {
            fprintf(stderr, "  at %s:%d:%d (instruction %zu)\n", 
                    vm->source_filename, current_inst->source_line + 1, 
                    current_inst->source_column + 1, vm->pc);
            
            // Show source code snippet if available
            if (vm->source_content) {
                fprintf(stderr, "  Traceback (most recent call last):\n");
                
                // Split source content into lines
                char* content_copy = _s_strdup(vm->source_content);
                if (content_copy) {
                    char* line = _no_skip_strtok(content_copy, "\n");
                    int line_num = 1;
                    int error_line = current_inst->source_line + 1; // Convert from 0-based to 1-based
                    
                    // Show context: 2 lines before and after the error line
                    int start_line = (error_line - 2) > 1 ? (error_line - 2) : 1;
                    int end_line = error_line + 2;
                    
                    while (line != nullptr && line_num <= end_line) {
                        if (line_num >= start_line) {
                            if (line_num == error_line) {
                                fprintf(stderr, " -> %4d | %s\n", line_num, line);
                            } else {
                                fprintf(stderr, "    %4d | %s\n", line_num, line);
                            }
                        }
                        line = _no_skip_strtok(nullptr, "\n");
                        line_num++;
                    }
                    free(content_copy);
                } else {
                    fprintf(stderr, "    <bytecode>\n");
                }
            }
        } else {
            fprintf(stderr, "  at instruction %zu\n", vm->pc);
        }
    }
    fprintf(stderr, "\n");
    vm->pc++;
}

/**
 * Print current stack state (for debugging)
 */
void vm_print_stack(VM* vm) {
    if (!vm) return;

    printf("Stack [%zu]: ", vm->stack_top);
    for (size_t i = 0; i < vm->stack_top; i++) {
        switch (vm->stack[i].type) {
            case VAL_NULL: printf("null "); break;
            case VAL_NUMBER: printf("%.6f ", vm->stack[i].as.number); break;
            case VAL_STRING: printf("\"%s\" ", vm->stack[i].as.string); break;
            case VAL_BOOL: printf("%s ", vm->stack[i].as.boolean ? "true" : "false"); break;
            default: printf("<object> "); break; // Complex types only show type
        }
    }
    printf("\n");
}

/* ========== Source Tracking Functions ========== */

/**
 * Set source information for backtrace functionality
 */
void vm_set_source_info(VM* vm, const char* filename, const char* source_content) {
    if (!vm) return;

    // Free existing source information
    if (vm->source_filename) {
        free(vm->source_filename);
        vm->source_filename = nullptr;
    }
    if (vm->source_content) {
        free(vm->source_content);
        vm->source_content = nullptr;
    }

    // Set new source information
    if (filename) {
        vm->source_filename = _s_strdup(filename);
    }
    if (source_content) {
        vm->source_content = _s_strdup(source_content);
    }
    vm->is_bytecode_execution = false;
}

/**
 * Mark VM as executing bytecode only
 */
void vm_set_bytecode_execution(VM* vm, bool is_bytecode) {
    if (!vm) return;
    vm->is_bytecode_execution = is_bytecode;
}

/* ========== Variable Operation Functions ========== */

/**
 * Get variable value (search in local variables first, then global variables)
 */
Value* vm_get_variable(VM* vm, const char* name) {
    if (!vm || !name) return nullptr;

    // First search local variables
    if (vm->locals) {
        Variable* var = find_variable(vm->locals, name);
        if (var) return &var->value;
    }

    // Then search global variables
    Variable* var = find_variable(&vm->globals, name);
    if (var) return &var->value;

    return nullptr;
}

/**
 * Set variable value (set in local variables first, then global variables)
 */
bool vm_set_variable(VM* vm, const char* name, Value value) {
    if (!vm || !name) return false;

    // First try to set local variable
    if (vm->locals) {
        Variable* var = find_variable(vm->locals, name);
        if (var) {
            if (vm) {
                free_value_gc(vm, var->value);
            } else {
                free_value(var->value);
            }
            var->value = copy_value(vm, value);
            // Check for global
            Variable* var_g = find_variable(&vm->globals, name);
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
    Variable* var = find_variable(&vm->globals, name);
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
bool vm_define_global(VM* vm, const char* name, Value value) {
    if (!vm || !name) return false;
    return add_variable(vm, &vm->globals, name, value);
}

/**
 * Register native function to global variables
 */
void vm_register_native(VM* vm, const char* name, NativeFunction func) {
    if (!vm || !name || !func) return;

    Value native_val = create_native(func);
    vm_define_global(vm, name, native_val);
}

/**
 * Push value to VM stack from external source
 */
void vm_push_external(VM* vm, Value value) {
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
static bool load_shared_library(VM* vm, const char* lib_path, const char* module_name) {
#ifdef _WIN32
    HMODULE handle = LoadLibrary(lib_path);
    if (!handle) {
        return false;
    }
    
    typedef int (*LibInitFunc)(VM*);
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
    
    typedef int (*LibInitFunc)(VM*);
    LibInitFunc init_func = (LibInitFunc)dlsym(handle, "_sbLibInit");
    if (!init_func) {
        dlclose(handle);
        return false;
    }
#endif

    /* Store library handle for cleanup */
    LoadedLibrary* new_libs = (LoadedLibrary*)realloc(vm->loaded_libs, 
                                                      (vm->loaded_lib_count + 1) * sizeof(LoadedLibrary));
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
static bool load_bytecode_file(VM* vm, const char* filename, const char* module_name) {
    BytecodeGenerator* gen = load_bytecode(filename);
    if (!gen) {
        return false;
    }

    /* Save current VM state */
    size_t saved_pc = vm->pc;
    bool saved_running = vm->running;
    Instruction* saved_instructions = vm->instructions;
    size_t saved_instruction_count = vm->instruction_count;

    /* Load and execute module bytecode */
    if (!vm_load_bytecode(vm, gen)) {
        destroy_bytecode_generator(gen);
        return false;
    }

    //printf("Loading bytecode module: %s\n", module_name);

    VMError result = vm_execute(vm);
    if (result != VM_OK) {
        vm_print_error(vm);
    }

    /* Restore VM state */
    if (vm->instructions && vm->instructions != saved_instructions) {
        for (size_t i = 0; i < vm->instruction_count; i++) {
            Instruction* inst = &vm->instructions[i];
            if (inst->opcode == OP_PUSH_STR || inst->opcode == OP_PUSH_IDENT ||
                inst->opcode == OP_LOAD_VAR || inst->opcode == OP_STORE_VAR ||
                inst->opcode == OP_LOAD_MODULE || inst->opcode == OP_FUNC_START ||
                inst->opcode == OP_STRUCT_DEF || inst->opcode == OP_STRUCT_NEW ||
                inst->opcode == OP_MEMBER_ACCESS || inst->opcode == OP_MEMBER_STORE ||
                inst->opcode == OP_LOAD_GLOBAL || inst->opcode == OP_STORE_GLOBAL) {
                if (inst->operand.str_value) free(inst->operand.str_value);
            }
        }
        free(vm->instructions);
    }

    vm->instructions = saved_instructions;
    vm->instruction_count = saved_instruction_count;
    vm->pc = saved_pc;
    vm->running = saved_running;

    destroy_bytecode_generator(gen);
    return result == VM_OK;
}

/**
 * Append module instructions to VM
 */
static bool append_module_instructions(VM* vm, BytecodeGenerator* gen, size_t offset) {
    if (!vm || !gen) return false;
    
    // First, add a HALT instruction at the end of main program to prevent fall-through
    if (offset > 0) {
        // Expand instruction array by 1 for HALT
        Instruction* new_instructions = (Instruction*)realloc(vm->instructions, 
                                                              (offset + 1) * sizeof(Instruction));
        if (!new_instructions) {
            vm_error(vm, VM_MEMORY_ERROR, "Failed to add HALT instruction");
            return false;
        }
        vm->instructions = new_instructions;
        
        // Add HALT instruction at the end of main program
        vm->instructions[offset].opcode = OP_HALT;
        vm->instructions[offset].operand.int_value = 0;
        
        vm->instruction_count = offset + 1;
        offset = vm->instruction_count;  // Update offset for module instructions
    }
    
    // Calculate new total instruction count
    size_t new_count = vm->instruction_count + gen->instructions->count;
    
    // Reallocate instruction array
    Instruction* new_instructions = (Instruction*)realloc(vm->instructions, 
                                                          new_count * sizeof(Instruction));
    if (!new_instructions) {
        vm_error(vm, VM_MEMORY_ERROR, "Failed to expand instruction memory for module");
        return false;
    }
    
    vm->instructions = new_instructions;
    
    // Copy module instructions to the end of main instructions
    for (size_t i = 0; i < gen->instructions->count; i++) {
        Instruction* src = (Instruction*)gen->instructions->items[i];
        size_t dest_idx = vm->instruction_count + i;
        vm->instructions[dest_idx] = *src;
        
        // Copy string operands
        if (src->opcode == OP_PUSH_STR || src->opcode == OP_PUSH_IDENT ||
            src->opcode == OP_LOAD_VAR || src->opcode == OP_STORE_VAR ||
            src->opcode == OP_LOAD_MODULE || src->opcode == OP_FUNC_START ||
            src->opcode == OP_STRUCT_DEF || src->opcode == OP_STRUCT_NEW ||
            src->opcode == OP_MEMBER_ACCESS || src->opcode == OP_MEMBER_STORE ||
            src->opcode == OP_LOAD_GLOBAL || src->opcode == OP_STORE_GLOBAL) {
            if (src->operand.str_value) {
                vm->instructions[dest_idx].operand.str_value = _s_strdup(src->operand.str_value);
            }
        }
        
        // Adjust jump addresses
        if (src->opcode == OP_JUMP || src->opcode == OP_JUMP_IF_FALSE || 
            src->opcode == OP_JUMP_IF_TRUE) {
            vm->instructions[dest_idx].operand.int_value += offset;
        }
    }
    
    // Load and adjust function definitions
    if (gen->functions && gen->functions->count > 0) {
        size_t old_func_count = vm->function_count;
        size_t new_func_count = old_func_count + gen->functions->count;
        
        Function* new_functions = (Function*)realloc(vm->functions, 
                                                     new_func_count * sizeof(Function));
        if (!new_functions) {
            vm_error(vm, VM_MEMORY_ERROR, "Failed to expand function table for module");
            return false;
        }
        
        vm->functions = new_functions;
        
        for (size_t i = 0; i < gen->functions->count; i++) {
            FunctionInfo* src = (FunctionInfo*)gen->functions->items[i];
            size_t dest_idx = old_func_count + i;
            
            vm->functions[dest_idx].name = _s_strdup(src->name);
            vm->functions[dest_idx].start_addr = src->start_addr + offset; // Adjust function address
            vm->functions[dest_idx].param_count = src->param_count;
            vm->functions[dest_idx].locals = nullptr;
            
            // Register function as a global variable
            Value func_val = create_function(&vm->functions[dest_idx]);
            vm_define_global(vm, vm->functions[dest_idx].name, func_val);
        }
        
        vm->function_count = new_func_count;
    }
    
    // Update instruction count
    vm->instruction_count = new_count;
    
    return true;
}

/**
 * Load source file and compile
 */
static bool load_source_file(VM* vm, const char* filename, const char* module_name) {
    // For traceback
    char* old_fn = vm->source_filename;
    char* old_ct = vm->source_content;

    /* Open module file */
    FILE* file = fopen(filename, "r");
    if (!file) {
        return false;
    }

    /* Read file content */
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* source = (char*)malloc(file_size + 1);
    if (!source) {
        fclose(file);
        vm_error(vm, VM_MEMORY_ERROR, "Failed to allocate memory for module source");
        return false;
    }

    fread(source, 1, file_size, file);
    source[file_size] = '\0';
    fclose(file);

    vm->source_filename = _s_strdup(filename);
    vm->source_content = _s_strdup(source);

    /* Lexical analysis */
    _sbToken* tokens = _sbLexer(source);
    if (!tokens) {
        free(source);
        vm_error(vm, VM_LOAD_ERROR, "Failed to tokenize module '%s'", module_name);
        return false;
    }

    /* Syntax analysis */
    Parser* parser = create_tkstate(tokens);
    if (!parser) {
        free(tokens);
        free(source);
        vm_error(vm, VM_LOAD_ERROR, "Failed to create parser for module '%s'", module_name);
        return false;
    }

    reset_error();
    ASTNode* ast = parse_program(parser);

    if (!ast || syntaxErrorDetector) {
        destroy_tkstate(parser);
        free(tokens);
        free(source);
        vm_error(vm, VM_LOAD_ERROR, "Failed to parse module '%s'", module_name);
        return false;
    }

    /* Generate bytecode */
    BytecodeGenerator* gen = create_bytecode_generator();
    if (!gen) {
        free_ast(ast);
        destroy_tkstate(parser);
        free(tokens);
        free(source);
        vm_error(vm, VM_MEMORY_ERROR, "Failed to create bytecode generator for module");
        return false;
    }

    if (!generate_bytecode(gen, ast)) {
        destroy_bytecode_generator(gen);
        free_ast(ast);
        destroy_tkstate(parser);
        free(tokens);
        free(source);
        vm_error(vm, VM_LOAD_ERROR, "Failed to generate bytecode for module '%s'", module_name);
        return false;
    }

    /* Append module instructions to main program */
    size_t offset = vm->instruction_count;  // Save current instruction count as offset
    if (!append_module_instructions(vm, gen, offset)) {
        destroy_bytecode_generator(gen);
        free_ast(ast);
        destroy_tkstate(parser);
        free(tokens);
        free(source);
        return false;
    }
    
    //printf("Loading module: %s\n", module_name);
    
    /* Execute module initialization code (from offset to end) */
    size_t saved_pc = vm->pc;
    
    //printf("DEBUG: Executing module from PC=%zu to PC=%zu\n", offset, vm->instruction_count - 1);
    
    // Skip the HALT instruction that separates main program from module
    if (offset > 0 && vm->instructions[offset].opcode == OP_HALT) {
        offset++;  // Skip the HALT to start at actual module code
    }
    
    vm->pc = offset;  // Start executing from the module's first instruction
    VMError result = vm_execute(vm);

    if (result != VM_OK) {
        vm_print_error(vm);
    }

    //printf("DEBUG: Module execution finished, restoring PC from %zu to %zu\n", vm->pc, saved_pc);
    
    /* Restore PC to continue main program execution */

    free(vm->source_content);
    free(vm->source_filename);

    vm->pc = saved_pc;
    vm->source_filename = old_fn;
    vm->source_content = old_ct;

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
static bool load_module(VM* vm, const char* module_name) {
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
    snprintf(filename, sizeof(filename), "%s.sbc", module_name);
    if (file_exists(filename)) {
        if (load_bytecode_file(vm, filename, module_name)) {
            return true;
        }
        /* If bytecode file exists but failed to load, continue trying source file */
        vm_error(vm, VM_LOAD_ERROR, "Failed to load bytecode file '%s'", filename);
    }
    
    /* Finally try to load source file (.sb) */
    snprintf(filename, sizeof(filename), "%s.sb", module_name);
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
 * Execute single instruction
 */
VMError vm_execute_instruction(VM* vm) {
    if (!vm || vm->pc >= vm->instruction_count) {
        return VM_RUNTIME_ERROR;
    }

    Instruction* inst = &vm->instructions[vm->pc++];

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
            Value val = vm_peek(vm, 0);
            vm_push(vm, val);
            break;
        }

        case OP_ADD: {
            // Addition operation (supports numbers and strings)
            Value b = vm_pop(vm);
            Value a = vm_pop(vm);

            if (a.type == VAL_NUMBER && b.type == VAL_NUMBER) {
                vm_push(vm, create_number(a.as.number + b.as.number));
            }
            else if (a.type == VAL_STRING && b.type == VAL_STRING) {
                // String concatenation
                size_t len = strlen(a.as.string) + strlen(b.as.string) + 1;
                char* result = (char*)malloc(len);
                if (result) {
                    strcpy(result, a.as.string);
                    strcat(result, b.as.string);
                    Value str_val = create_string(vm, result);
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
            Value b = vm_pop(vm);
            Value a = vm_pop(vm);

            if (a.type != VAL_NUMBER || b.type != VAL_NUMBER) {
                vm_error(vm, VM_TYPE_ERROR, "Subtraction requires numbers");
                return VM_TYPE_ERROR;
            }

            vm_push(vm, create_number(a.as.number - b.as.number));
            break;
        }

        case OP_MUL: {
            // Multiplication operation
            Value b = vm_pop(vm);
            Value a = vm_pop(vm);

            if (a.type != VAL_NUMBER || b.type != VAL_NUMBER) {
                vm_error(vm, VM_TYPE_ERROR, "Multiplication requires numbers");
                return VM_TYPE_ERROR;
            }

            vm_push(vm, create_number(a.as.number * b.as.number));
            break;
        }

        case OP_DIV: {
            // Division operation
            Value b = vm_pop(vm);
            Value a = vm_pop(vm);

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
            Value b = vm_pop(vm);
            Value a = vm_pop(vm);

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
            Value b = vm_pop(vm);
            Value a = vm_pop(vm);

            if (a.type != VAL_NUMBER || b.type != VAL_NUMBER) {
                vm_error(vm, VM_TYPE_ERROR, "Power requires numbers");
                return VM_TYPE_ERROR;
            }

            vm_push(vm, create_number(pow(a.as.number, b.as.number)));
            break;
        }

        case OP_BIT_AND: {
            // Bitwise AND operation
            Value b = vm_pop(vm);
            Value a = vm_pop(vm);

            if (a.type != VAL_NUMBER || b.type != VAL_NUMBER) {
                vm_error(vm, VM_TYPE_ERROR, "Bitwise AND requires numbers");
                return VM_TYPE_ERROR;
            }

            vm_push(vm, create_number((double)((int)a.as.number & (int)b.as.number)));
            break;
        }

        case OP_BIT_OR: {
            // Bitwise OR operation
            Value b = vm_pop(vm);
            Value a = vm_pop(vm);

            if (a.type != VAL_NUMBER || b.type != VAL_NUMBER) {
                vm_error(vm, VM_TYPE_ERROR, "Bitwise OR requires numbers");
                return VM_TYPE_ERROR;
            }

            vm_push(vm, create_number((double)((int)a.as.number | (int)b.as.number)));
            break;
        }

        case OP_BIT_XOR: {
            // Bitwise XOR operation
            Value b = vm_pop(vm);
            Value a = vm_pop(vm);

            if (a.type != VAL_NUMBER || b.type != VAL_NUMBER) {
                vm_error(vm, VM_TYPE_ERROR, "Bitwise XOR requires numbers");
                return VM_TYPE_ERROR;
            }

            vm_push(vm, create_number((double)((int)a.as.number ^ (int)b.as.number)));
            break;
        }

        case OP_BIT_NOT: {
            // Bitwise NOT operation
            Value a = vm_pop(vm);

            if (a.type != VAL_NUMBER) {
                vm_error(vm, VM_TYPE_ERROR, "Bitwise NOT requires number");
                return VM_TYPE_ERROR;
            }

            vm_push(vm, create_number((double)(~(int)a.as.number)));
            break;
        }

        case OP_BIT_LSHIFT: {
            // Left shift operation
            Value b = vm_pop(vm);
            Value a = vm_pop(vm);

            if (a.type != VAL_NUMBER || b.type != VAL_NUMBER) {
                vm_error(vm, VM_TYPE_ERROR, "Left shift requires numbers");
                return VM_TYPE_ERROR;
            }

            vm_push(vm, create_number((double)((int)a.as.number << (int)b.as.number)));
            break;
        }

        case OP_BIT_RSHIFT: {
            // Right shift operation
            Value b = vm_pop(vm);
            Value a = vm_pop(vm);

            if (a.type != VAL_NUMBER || b.type != VAL_NUMBER) {
                vm_error(vm, VM_TYPE_ERROR, "Right shift requires numbers");
                return VM_TYPE_ERROR;
            }

            vm_push(vm, create_number((double)((int)a.as.number >> (int)b.as.number)));
            break;
        }

        case OP_LOGIC_AND: {
            // Logical AND operation
            Value b = vm_pop(vm);
            Value a = vm_pop(vm);

            vm_push(vm, create_bool(is_truthy(a) && is_truthy(b)));
            free_value_gc(vm, a);
            free_value_gc(vm, b);
            break;
        }

        case OP_LOGIC_OR: {
            // Logical OR operation
            Value b = vm_pop(vm);
            Value a = vm_pop(vm);

            vm_push(vm, create_bool(is_truthy(a) || is_truthy(b)));
            free_value_gc(vm, a);
            free_value_gc(vm, b);
            break;
        }

        case OP_LOGIC_NOT: {
            // Logical NOT operation
            Value a = vm_pop(vm);
            vm_push(vm, create_bool(!is_truthy(a)));
            free_value(a);
            break;
        }

        case OP_EQ: {
            // Equality comparison
            Value b = vm_pop(vm);
            Value a = vm_pop(vm);

            vm_push(vm, create_bool(values_equal(a, b)));
            free_value_gc(vm, a);
            free_value_gc(vm, b);
            break;
        }

        case OP_NEQ: {
            // Inequality comparison
            Value b = vm_pop(vm);
            Value a = vm_pop(vm);

            vm_push(vm, create_bool(!values_equal(a, b)));
            free_value_gc(vm, a);
            free_value_gc(vm, b);
            break;
        }

        case OP_LT: {
            // Less than comparison
            Value b = vm_pop(vm);
            Value a = vm_pop(vm);

            if (a.type != VAL_NUMBER || b.type != VAL_NUMBER) {
                vm_error(vm, VM_TYPE_ERROR, "Comparison requires numbers");
                return VM_TYPE_ERROR;
            }

            vm_push(vm, create_bool(a.as.number < b.as.number));
            break;
        }

        case OP_GT: {
            // Greater than comparison
            Value b = vm_pop(vm);
            Value a = vm_pop(vm);

            if (a.type != VAL_NUMBER || b.type != VAL_NUMBER) {
                vm_error(vm, VM_TYPE_ERROR, "Comparison requires numbers");
                return VM_TYPE_ERROR;
            }

            vm_push(vm, create_bool(a.as.number > b.as.number));
            break;
        }

        case OP_LEQ: {
            // Less than or equal comparison
            Value b = vm_pop(vm);
            Value a = vm_pop(vm);

            if (a.type != VAL_NUMBER || b.type != VAL_NUMBER) {
                vm_error(vm, VM_TYPE_ERROR, "Comparison requires numbers");
                return VM_TYPE_ERROR;
            }

            vm_push(vm, create_bool(a.as.number <= b.as.number));
            break;
        }

        case OP_GEQ: {
            // Greater than or equal comparison
            Value b = vm_pop(vm);
            Value a = vm_pop(vm);

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
            Value* var = vm_get_variable(vm, name);

            if (!var) {
                Value* native_var = vm_get_variable(vm, name);
                if (native_var && native_var->type == VAL_NATIVE) {
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
                    case VAL_STRUCT_INSTANCE: {
                        // For struct instances, push reference to allow member modification
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
            Value value = vm_pop(vm);
            const char* name = inst->operand.str_value;

            if (inst->opcode == OP_STORE_GLOBAL)
                add_variable(vm, &vm->globals, name, value);

            if (!vm_set_variable(vm, name, value)) {
                vm_error(vm, VM_RUNTIME_ERROR, "Failed to store variable '%s'", name);
                free_value_gc(vm, value);
                return VM_RUNTIME_ERROR;
            }
            // Free the original value since vm_set_variable makes a copy
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
            Value cond = vm_pop(vm);
            if (!is_truthy(cond)) {
                vm->pc = inst->operand.int_value;
            }
            free_value_gc(vm, cond);
            break;
        }

        case OP_JUMP_IF_TRUE: {
            // Jump if condition is true
            Value cond = vm_pop(vm);
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
            Value args[arg_count];
            for (int i = arg_count - 1; i >= 0; i--) {
                Value original = vm_pop(vm);
                // Deep copy struct instances to avoid double-free when passed as function arguments
                if (original.type == VAL_STRUCT_INSTANCE) {
                    args[i] = copy_value(vm, original);
                } else {
                    args[i] = original;
                }
            }

            // Get function name
            Value func_name = vm_pop(vm);

            if (func_name.type == VAL_STRING) {
                Value* func_val = vm_get_variable(vm, func_name.as.string);

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
                    Value result = func_val->as.native(vm, args, arg_count);
                    vm_push(vm, result);

                    for (int i = 0; i < arg_count; i++) {
                        free_value_gc(vm, args[i]);
                    }
                }
                else if (func_val->type == VAL_FUNCTION) {
                    // Call user-defined function
                    Function* func = func_val->as.function;
                    
                    // Check argument count
                    if (func->param_count != (size_t)arg_count) {
                        vm_error(vm, VM_ARGUMENT_MISMATCH, 
                                "Function '%s' expects %zu arguments, got %d", 
                                func_name.as.string, func->param_count, arg_count);
                        free_value_gc(vm, func_name);
                        for (int i = 0; i < arg_count; i++) {
                            free_value(args[i]);
                        }
                        return VM_ARGUMENT_MISMATCH;
                    }
                    
                    // Check if call stack needs expansion
                    if (vm->call_depth >= vm->call_capacity) {
                        size_t new_capacity = vm->call_capacity * 2;
                        CallFrame* new_call_stack = (CallFrame*)realloc(vm->call_stack, new_capacity * sizeof(CallFrame));
                        if (!new_call_stack) {
                            vm_error(vm, VM_MEMORY_ERROR, "Failed to expand call stack");
                            free_value_gc(vm, func_name);
                            for (int i = 0; i < arg_count; i++) {
                                free_value(args[i]);
                            }
                            return VM_MEMORY_ERROR;
                        }
                        vm->call_stack = new_call_stack;
                        vm->call_capacity = new_capacity;
                    }
                    
                    // Create call frame
                    CallFrame* frame = &vm->call_stack[vm->call_depth++];
                    frame->function = func;
                    frame->return_addr = vm->pc;
                    frame->stack_base = vm->stack_top;  // Save current stack top before pushing args back
                    
                    // Save current local scope and create new one
                    VariableTable* old_locals = vm->locals;
                    vm->locals = (VariableTable*)malloc(sizeof(VariableTable));
                    init_variable_table(vm->locals);
                    
                    // Store old locals in frame for restoration on return
                    frame->locals = (Value*)old_locals;  // Temporarily store pointer
                    frame->local_count = 0;  // Flag to indicate it's a VariableTable pointer
                    
                    // Push arguments back onto stack for the function to use
                    for (int i = 0; i < arg_count; i++) {
                        vm_push(vm, args[i]);
                    }
                    
                    // Jump to function body
                    vm->pc = func->start_addr;
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
                Value return_value = create_null();
                if (vm->stack_top > 0) {
                    return_value = vm_pop(vm);
                }
                
                CallFrame* frame = &vm->call_stack[--vm->call_depth];
                
                // Clean up function's stack frame (remove local variables and parameters)
                vm->stack_top = frame->stack_base;
                
                // Restore PC
                vm->pc = frame->return_addr;

                // Clean up current local variables and restore previous scope
                if (vm->locals) {
                    free_variable_table(vm->locals);
                    free(vm->locals);
                }
                
                // Restore previous local scope (stored in frame->locals)
                vm->locals = (VariableTable*)frame->locals;
                
                // Push return value back onto stack
                vm_push(vm, return_value);
            }
            else {
                return VM_OK; // Main program return
            }
            break;
        }

        case OP_FUNC_START:
        case OP_FUNC_END:
            // Function definition start/end (skip function body execution)
            while (vm->pc < vm->instruction_count &&
                   vm->instructions[vm->pc].opcode != OP_FUNC_END) {
                vm->pc++;
            }
            if (vm->pc < vm->instruction_count) vm->pc++;
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

        case OP_STRUCT_DEF: {
            // Struct definition (skip)
            break;
        }

        case OP_STRUCT_NEW: {
            // Create struct instance
            const char* struct_name = inst->operand.str_value;

            // Find struct definition
            Struct* struct_def = nullptr;
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

            // Create struct instance
            StructInstance* instance = (StructInstance*)malloc(sizeof(StructInstance));
            if (!instance) {
                vm_error(vm, VM_MEMORY_ERROR, "Failed to create struct instance");
                return VM_MEMORY_ERROR;
            }

            instance->struct_def = struct_def;
            instance->members = (Value*)calloc(struct_def->member_count, sizeof(Value));

            // Initialize member values to null
            for (size_t i = 0; i < struct_def->member_count; i++) {
                instance->members[i] = create_null();
            }

            Value val;
            val.type = VAL_STRUCT_INSTANCE;
            val.as.instance = instance;
            vm_push(vm, val);
            break;
        }

        case OP_MEMBER_ACCESS: {
            // Access struct member
            Value obj = vm_pop(vm);
            const char* member_name = inst->operand.str_value;

            //printf("DEBUG: Accessing member '%s'\n", member_name);

            if (obj.type != VAL_STRUCT_INSTANCE) {
                vm_error(vm, VM_NOT_A_STRUCT, "Cannot access member of non-struct");
                free_value_gc(vm, obj);
                return VM_NOT_A_STRUCT;
            }

            StructInstance* instance = obj.as.instance;
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
            Value obj = vm_pop(vm);
            Value value = vm_pop(vm);
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

            StructInstance* instance = obj.as.instance;
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
            Value list = create_list(vm);

            if (count > 0 && list.as.list) {
                list.as.list->items = (Value*)malloc(count * sizeof(Value));
                list.as.list->capacity = count;
                list.as.list->count = 0;
            }

            vm_push(vm, list);
            break;
        }

        case OP_LIST_PUSH: {
            // Add element to list
            Value item = vm_pop(vm);
            Value list = vm_pop(vm);

            if (list.type != VAL_LIST) {
                vm_error(vm, VM_TYPE_ERROR, "Cannot push to non-list");
                free_value_gc(vm, item);
                free_value_gc(vm, list);
                return VM_TYPE_ERROR;
            }

            List* l = list.as.list;
            if (l->count >= l->capacity) {
                // Expand list capacity
                size_t new_capacity = l->capacity == 0 ? 4 : l->capacity * 2;
                Value* new_items = (Value*)realloc(l->items, new_capacity * sizeof(Value));
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
            Value index = vm_pop(vm);
            Value list = vm_pop(vm);

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
            if (idx < 0 || idx >= (int)list.as.list->count) {
                vm_error(vm, VM_INDEX_OUT_OF_BOUNDS, "List index out of bounds");
                free_value_gc(vm, index);
                free_value_gc(vm, list);
                return VM_INDEX_OUT_OF_BOUNDS;
            }

            vm_push(vm, copy_value(vm, list.as.list->items[list.as.list->count - idx - 1]));
            break;
        }

        case OP_HALT:
            // Stop execution
            if (vm->pc == vm->end_pc)
                vm->running = false; // End program
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
VMError vm_execute(VM* vm) {
    if (!vm || !vm->instructions) {
        return VM_RUNTIME_ERROR;
    }

    vm->running = true;
    vm->last_error = VM_OK;

    // Main execution loop
    while (vm->running && vm->pc < vm->instruction_count) {
        VMError error = vm_execute_instruction(vm);
        if (error != VM_OK) {
            vm->running = false;
            return error;
        }
    }

    return VM_OK;
}

/**
 * Load bytecode from bytecode generator to VM
 */
bool vm_load_bytecode(VM* vm, BytecodeGenerator* gen) {
    if (!vm || !gen) return false;

    vm->instruction_count = gen->instructions->count;
    vm->instructions = (Instruction*)malloc(vm->instruction_count * sizeof(Instruction));
    if (!vm->instructions) {
        vm_error(vm, VM_MEMORY_ERROR, "Failed to allocate instruction memory");
        return false;
    }

    // Copy instructions
    for (size_t i = 0; i < vm->instruction_count; i++) {
        Instruction* src = (Instruction*)gen->instructions->items[i];
        vm->instructions[i] = *src;

        // Copy string operands
        if (src->opcode == OP_PUSH_STR || src->opcode == OP_PUSH_IDENT ||
            src->opcode == OP_LOAD_VAR || src->opcode == OP_STORE_VAR ||
            src->opcode == OP_LOAD_MODULE || src->opcode == OP_FUNC_START ||
            src->opcode == OP_STRUCT_DEF || src->opcode == OP_STRUCT_NEW ||
            src->opcode == OP_MEMBER_ACCESS || src->opcode == OP_MEMBER_STORE ||
            src->opcode == OP_LOAD_GLOBAL || src->opcode == OP_STORE_GLOBAL) {
            if (src->operand.str_value) {
                vm->instructions[i].operand.str_value = _s_strdup(src->operand.str_value);
            }
        }
    }

    // Load function definitions and register them as global variables
    if (gen->functions && gen->functions->count > 0) {
        vm->function_count = gen->functions->count;
        vm->functions = (Function*)malloc(vm->function_count * sizeof(Function));

        for (size_t i = 0; i < vm->function_count; i++) {
            FunctionInfo* src = (FunctionInfo*)gen->functions->items[i];
            vm->functions[i].name = _s_strdup(src->name);
            vm->functions[i].start_addr = src->start_addr;
            vm->functions[i].param_count = src->param_count;
            vm->functions[i].locals = nullptr;
            
            // Register function as a global variable
            Value func_val = create_function(&vm->functions[i]);
            vm_define_global(vm, vm->functions[i].name, func_val);
        }
    }

    // Load struct definitions
    if (gen->structs && gen->structs->count > 0) {
        vm->struct_count = gen->structs->count;
        vm->structs = (Struct*)malloc(vm->struct_count * sizeof(Struct));

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
    }

    // Initialize global variables
    if (gen->globals) {
        for (size_t i = 0; i < gen->globals->count; i++) {
            char* global_name = (char*)gen->globals->items[i];
            vm_define_global(vm, global_name, create_null());
        }
    }

    vm->pc = 0;

    if (vm->running == false)
        vm->end_pc = vm->instruction_count;

    return true;
}

/**
 * Load bytecode from file to VM
 */
bool vm_load_from_file(VM* vm, const char* filename) {
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
 * Garbage collection (mark-and-sweep algorithm)
 */
void vm_gc_collect(VM* vm) {
    if (!vm || !vm->gc_enabled) return;
    
    // Phase 1: Mark all reachable objects
    vm_gc_mark_roots(vm);
    
    // Phase 2: Sweep unmarked objects
    vm_gc_sweep(vm);
    
    // Update threshold for next GC
    vm->gc_threshold = vm->gc_bytes_allocated * 2;
    if (vm->gc_threshold < 1024 * 1024) {
        vm->gc_threshold = 1024 * 1024; // Minimum 1MB
    }
}

/**
 * Mark live objects
 */
void vm_gc_mark(VM* vm, Value value) {
    if (!vm) return;
    
    // Find the GC object for this value and mark it
    GCObject* obj = vm->gc_objects;
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
            
            // Recursively mark referenced objects
            if (value.type == VAL_LIST && value.as.list) {
                for (size_t i = 0; i < value.as.list->count; i++) {
                    vm_gc_mark(vm, value.as.list->items[i]);
                }
            } else if (value.type == VAL_STRUCT_INSTANCE && value.as.instance) {
                for (size_t i = 0; i < value.as.instance->struct_def->member_count; i++) {
                    vm_gc_mark(vm, value.as.instance->members[i]);
                }
            }
        }
        
        obj = obj->next;
    }
}

/**
 * Sweep unmarked objects
 */
void vm_gc_sweep(VM* vm) {
    if (!vm) return;
    
    GCObject** current = &vm->gc_objects;
    
    while (*current) {
        if (!(*current)->marked) {
            // Object is not marked, free it
            GCObject* to_free = *current;
            *current = to_free->next;
            
            // Free the actual data based on type
            switch (to_free->type) {
                case VAL_STRING:
                    free(to_free->data);
                    break;
                case VAL_LIST: {
                    List* list = (List*)to_free->data;
                    if (list->items) {
                        free(list->items);
                    }
                    free(list);
                    break;
                }
                case VAL_STRUCT_INSTANCE: {
                    StructInstance* instance = (StructInstance*)to_free->data;
                    if (instance->members) {
                        free(instance->members);
                    }
                    free(instance);
                    break;
                }
                default:
                    break;
            }
            
            free(to_free);
            vm->gc_object_count--;
        } else {
            // Object is marked, unmark it for next collection
            (*current)->marked = false;
            current = &(*current)->next;
        }
    }
}

/**
 * Allocate memory with GC tracking
 */
void* gc_alloc(VM* vm, size_t size, ValueType type) {
    if (!vm) return nullptr;
    
    // Check if we should trigger GC
    if (vm->gc_enabled && vm->gc_bytes_allocated >= vm->gc_threshold) {
        vm_gc_collect(vm);
    }
    
    // Allocate GC object header
    GCObject* obj = (GCObject*)malloc(sizeof(GCObject));
    if (!obj) return nullptr;
    
    // Allocate the actual data
    void* data = malloc(size);
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
    vm->gc_bytes_allocated += size + sizeof(GCObject);
    
    return data;
}

/**
 * Free GC object manually
 */
void gc_free_object(VM* vm, GCObject* obj) {
    if (!vm || !obj) return;
    
    // Remove from linked list
    if (vm->gc_objects == obj) {
        vm->gc_objects = obj->next;
    } else {
        GCObject* current = vm->gc_objects;
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
            List* list = (List*)obj->data;
            if (list->items) {
                free(list->items);
            }
            free(list);
            break;
        }
        case VAL_STRUCT_INSTANCE: {
            StructInstance* instance = (StructInstance*)obj->data;
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
void vm_gc_mark_roots(VM* vm) {
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