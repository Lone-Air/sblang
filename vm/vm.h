/*
 * SB - Language
 * By Laman28
 * Virtual Machine
 * Not welcome to use /XD
 */

#ifndef _SBL_VM
#define _SBL_VM

#include "../bytecode/bytecode.h"
#include <stdbool.h>

#define VM_INITIAL_STACK_SIZE 512      /* Initial operand stack size */
#define VM_INITIAL_CALL_STACK_SIZE 1024  /* Initial call stack depth */
#define VM_INITIAL_SOURCE_FILE_PATH_STORAGE_SIZE 128  /* Initial sfp depth */
#define VM_INITIAL_SOURCE_FILE_PATH_TRACEBACK_SIZE 256  /* Initial sf_traceback depth */
#define VM_INITIAL_CHUNK_SIZE 16  /* Initial vm chunks depth */
#define VM_INITIAL_CHUNK_TRACEBACK_SIZE 128  /* Initial vm chunks traceback depth */

/* Value type enumeration */
typedef enum {
    VAL_NULL,               /* Null value */
    VAL_NUMBER,             /* Number type */
    VAL_STRING,             /* String type */
    VAL_BOOL,               /* Boolean type */
    VAL_FUNCTION,           /* Function type */
    VAL_NATIVE,             /* Native function type */
    VAL_STRUCT,             /* Struct definition type */
    VAL_STRUCT_INSTANCE,    /* Struct instance type */
    VAL_LIST,                /* List type */

    VAL_FREED                /* Freed value */
} _sbValueType;

/* Forward declarations */
typedef struct _sbValue _sbValue;
typedef struct _sbVM _sbVM;

/* Native function pointer type */
typedef _sbValue (*_sbNativeFunction)(_sbVM* vm, _sbValue* args, int arg_count);

/* Function structure */
typedef struct {
    char* name;             /* Function name */
    size_t start_addr;      /* Function start address */
    size_t param_count;     /* Parameter count */
    _sbValue* locals;          /* Local variables array */
    size_t local_count;     /* Local variable count */

    char* source_code_file; /* Function defined in which file (for traceback) */

    size_t chunk_id;
} _sbVFunction;

/* Struct definition */
typedef struct {
    char* name;             /* Struct name */
    char** members;         /* Member names array */
    size_t member_count;    /* Member count */
} _sbVStruct;

/* Struct instance */
typedef struct {
    _sbVStruct* struct_def;     /* Struct definition */
    _sbValue* members;         /* Member values array */
} _sbVStructInstance;

/* List structure */
typedef struct {
    _sbValue* items;           /* Elements array */
    size_t count;           /* Current element count */
    size_t capacity;        /* Capacity */
} _sbVList;

/* GC object header for tracking allocated objects */
typedef struct _sbGCObject {
    struct _sbGCObject* next;      /* Next object in linked list */
    bool marked;                /* Mark flag for GC */
    _sbValueType type;             /* Type of the object */
    void* data;                 /* Pointer to the actual data */
} _sbGCObject;

/* Value structure - supports multiple data types */
struct _sbValue {
    _sbValueType type;         /* Value type */
    union {
        double number;              /* Numeric value */
        char* string;               /* String value */
        bool boolean;               /* Boolean value */
        _sbVFunction* function;         /* Function pointer */
        _sbNativeFunction native;      /* Native function pointer */
        _sbVStruct* struct_def;         /* Struct definition */
        _sbVStructInstance* instance;   /* Struct instance */
        _sbVList* list;                 /* List pointer */
    } as;
    bool freed;
};

/* Call frame structure */
typedef struct {
    _sbVFunction* function;     /* Current function */
    size_t return_addr;     /* Return address */
    _sbValue* locals;          /* Local variables */
    size_t local_count;     /* Local variable count */
    size_t stack_base;      /* Stack base address */
} _sbCallFrame;

/* Variable structure */
typedef struct {
    char* name;             /* Variable name */
    _sbValue value;            /* Variable value */
} _sbVariable;

/* Variable table */
typedef struct {
    _sbVariable* vars;         /* Variables array */
    size_t count;           /* Variable count */
    size_t capacity;        /* Capacity */
} _sbVariableTable;

/* VM error type enumeration */
typedef enum {
    VM_OK,                  /* No error */
    VM_RUNTIME_ERROR,       /* Runtime error */
    VM_STACK_OVERFLOW,      /* Stack overflow */
    VM_STACK_UNDERFLOW,     /* Stack underflow */
    VM_UNDEFINED_VARIABLE,  /* Undefined variable */
    VM_TYPE_ERROR,          /* Type error */
    VM_DIVISION_BY_ZERO,    /* Division by zero */
    VM_INDEX_OUT_OF_BOUNDS, /* Index out of bounds */
    VM_UNDEFINED_FUNCTION,  /* Undefined function */
    VM_ARGUMENT_MISMATCH,   /* Argument mismatch */
    VM_LOAD_ERROR,          /* Load error */
    VM_MEMORY_ERROR,        /* Memory error */
    VM_INVALID_OPCODE,      /* Invalid opcode */
    VM_UNDEFINED_MEMBER,    /* Undefined member */
    VM_NOT_A_STRUCT,        /* Not a struct type */
} VMError;

/* Loaded shared library information */
typedef struct {
    void* handle;                   /* Dynamic library handle */
    char* name;                     /* Library name */
} _sbLoadedLibrary;

/* Instruction chunks */
typedef struct {
    size_t chunk_id;

    Instruction* inst;
    size_t chunk_pc;
    size_t inst_count;
} _sbSubChunk;

/* Main VM structure */
struct _sbVM {
    /* Instruction related */

    _sbSubChunk* chunks;
    size_t chunk_id;
    size_t chunk_count;
    size_t chunk_capacity;

    size_t* chunk_traceback;
    size_t chunk_tb_count;
    size_t chunk_traceback_capacity;

    Instruction* instructions;      /* Instructions array (From chunks) */
    size_t instruction_count;       /* Instruction count */
    size_t pc;                      /* Program counter */

    /* Operand stack */
    _sbValue* stack;                   /* Dynamic operand stack */
    size_t stack_top;               /* Stack top pointer */
    size_t stack_capacity;          /* Operand stack capacity */

    /* Call stack */
    _sbCallFrame* call_stack;          /* Dynamic call stack */
    size_t call_depth;              /* Call depth */
    size_t call_capacity;           /* Call stack capacity */

    /* Variable table */
    _sbVariableTable globals;          /* Global variable table */
    _sbVariableTable* locals;          /* Current local variable table */

    /* Functions and structs */
    _sbVFunction* functions;            /* Function table */
    size_t function_count;          /* Function count */
    _sbVStruct* structs;                /* Struct table */
    size_t struct_count;            /* Struct count */

    /* Loaded shared libraries */
    _sbLoadedLibrary* loaded_libs;    /* Loaded shared libraries array */
    size_t loaded_lib_count;        /* Loaded shared libraries count */

    /* Error handling */
    VMError last_error;             /* Last error type */
    char* error_message;            /* Error message */

    /* Runtime status */
    bool running;                   /* Running flag */
    bool gc_enabled;                /* Garbage collection flag */
    
    /* Garbage collection */
    _sbGCObject* gc_objects;           /* Linked list of all allocated objects */
    size_t gc_object_count;         /* Number of allocated objects */
    size_t gc_threshold;            /* GC trigger threshold */
    size_t gc_bytes_allocated;      /* Total bytes allocated */

    /* Source tracking for backtrace */
    char* source_filename;          /* Source filename for error reporting */
    char* source_content;           /* Source content for error snippets */
    bool is_bytecode_execution;     /* Flag to indicate bytecode-only execution */

    /* File name storage */
    char** sfp;
    int* bc;
    int* sf_traceback;

    size_t sf_traceback_count;
    size_t sf_traceback_capacity;
    size_t sfp_count;
    size_t sfp_capacity;
    size_t bc_count;
    size_t bc_capacity;

    bool error;
    bool error_from_native;
    bool debug;
};

/* Native function binding structure */
typedef struct {
    char* name;             /* Function name */
    _sbNativeFunction func;    /* Function pointer */
} _sbNativeBinding;

/* ========== VM Management Functions ========== */

/* Create VM instance */
extern _sbVM* create_vm();

/* Chunk management */
extern size_t new_chunk(_sbVM* vm);
extern size_t step_chunk(_sbVM* vm, size_t chunk_id);
extern size_t back_chunk(_sbVM* vm);
extern size_t save_chunk(_sbVM* vm);

/* enable debug for VM instance */
extern void enable_debug(_sbVM* vm);

/* Destroy VM instance, free all resources */
extern void destroy_vm(_sbVM* vm);
extern void destroy_vm_stacks(_sbVM* vm);

/* Extra files operation */
extern bool load_source_file(_sbVM* vm, const char* filename, const char* module_name);
extern bool load_bytecode_file(_sbVM* vm, const char* filename, const char* module_name);
extern bool load_module(_sbVM* vm, const char* module_name);

/* ========== Stack Operation Functions ========== */

/* Push value onto stack */
extern void vm_push(_sbVM* vm, _sbValue value);

/* Pop value from stack top */
extern _sbValue vm_pop(_sbVM* vm);

/* Peek at stack top element (without popping) */
extern _sbValue vm_peek(_sbVM* vm, int distance);

/* ========== Bytecode Loading Functions ========== */

/* Load bytecode from bytecode generator */
extern bool vm_load_bytecode(_sbVM* vm, BytecodeGenerator* gen);

/* Load bytecode from file */
extern bool vm_load_from_file(_sbVM* vm, const char* filename);

/* ========== Execution Functions ========== */

/* Execute bytecode */
extern VMError vm_execute(_sbVM* vm);

/* Execute single instruction */
extern VMError vm_execute_instruction(_sbVM* vm);

/* Call function */
extern VMError vm_call_function(_sbVM* vm, _sbVFunction* func, int arg_count);

/* ========== Native Function Management ========== */

/* Register native function */
extern void vm_register_native(_sbVM* vm, const char* name, _sbNativeFunction func);

/* Push value to VM stack from external source */
extern void vm_push_external(_sbVM* vm, _sbValue value);

/* ========== Variable Management Functions ========== */

/* Get variable value */
extern _sbValue* vm_get_variable(_sbVM* vm, const char* name);

/* Set variable value */
extern bool vm_set_variable(_sbVM* vm, const char* name, _sbValue value);

/* Define global variable */
extern bool vm_define_global(_sbVM* vm, const char* name, _sbValue value);

/* ========== Value Creation Functions ========== */

/* Create null value */
extern _sbValue create_null();

/* Create numeric value */
extern _sbValue create_number(double num);

/* Create string value */
extern _sbValue create_string(_sbVM* vm, const char* str);

/* Create boolean value */
extern _sbValue create_bool(bool b);

/* Create function value */
extern _sbValue create_function(_sbVM* vm, _sbVFunction* func, size_t chunk_id);

/* Create native function value */
extern _sbValue create_native(_sbNativeFunction func);

/* Create list */
extern _sbValue create_list(_sbVM* vm);

/* ========== Value Operation Functions ========== */

/* Append an item to list */
extern _sbVList* append_list(_sbVList* list, _sbValue value);

/* Insert an item into list */
extern _sbVList* insert_list(_sbVList* list, int index, _sbValue value);

/* Pop an item from list */
extern _sbVList* pop_list(_sbVList* list);

/* Check if value is truthy */
extern bool is_truthy(_sbValue value);

/* Check if two values are equal */
extern bool values_equal(_sbValue a, _sbValue b);

/* Copy value (deep copy) */
extern _sbValue copy_value(_sbVM* vm, _sbValue value);

/* Free memory occupied by value (GC-aware) */
extern void free_value_gc(_sbVM* vm, _sbValue value);

/* Free memory occupied by value */
extern void free_value(_sbValue value);

/* ========== No skipping repeated seperator: strtok ========== */
extern char* _no_skip_strtok(char* str, const char* delim);

/* ========== Error Handling Functions ========== */

/* Print stack contents (for debugging) */
extern void vm_print_stack(_sbVM* vm);

/* Print vm information */
extern void vm_print_status(_sbVM* vm);

/* ========== Source Tracking Functions ========== */

/* Set source information for backtrace */
extern int vm_set_source_info(_sbVM* vm, const char* filename, bool bytecode);

/* traceback source information for backtrace */
extern int vm_back_source_info(_sbVM* vm);

/* lookup source information for backtrace */
extern int vm_sourceinfo_lookup(_sbVM* vm, const char* filename);

/* Add source information for backtrace */
extern int vm_add_source_info(_sbVM* vm, const char* filename, bool bytecode);

/* Mark VM as executing bytecode only */
extern void vm_set_bytecode_execution(_sbVM* vm, bool is_bytecode);

/* Read entire file into memory */
extern char* read_file(const char* filename);

/* ========== Garbage Collection Functions ========== */

/* Perform garbage collection */
extern void vm_gc_collect(_sbVM* vm);

/* Mark value (GC phase one) */
extern void vm_gc_mark(_sbVM* vm, _sbValue value);

/* Sweep unmarked objects (GC phase two) */
extern void vm_gc_sweep(_sbVM* vm);

/* Allocate memory with GC tracking */
extern void* gc_alloc(_sbVM* vm, size_t size, _sbValueType type);

/* Free GC object */
extern void gc_free_object(_sbVM* vm, _sbGCObject* obj);

/* Mark all reachable objects from roots */
extern void vm_gc_mark_roots(_sbVM* vm);

/* Check if pointer is GC-managed */
extern bool is_gc_managed(_sbVM* vm, void* ptr);

/* Clean up static buffers */
extern void vm_cleanup_static_buffers(_sbVM* vm);

#endif