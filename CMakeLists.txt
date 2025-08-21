cmake_minimum_required(VERSION 4.0)

project(SBLang)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c23 -g")

# file(GLOB SRC "dynarray/*.c", "lexer/*.c", "parser/parser.c")

# Main SBL executable - Compiler frontend and runtime
add_executable(sbl 
        sbl.c
        vm/vm.c
        bytecode/bytecode.c
        parser/parser.c
        lexer/lexer.c
        error/error.c
        dynarray/dynarray.c
        builtin/base_functions.c
)
target_link_libraries(sbl m dl)

# Bytecode test executable
add_executable(test_bytecode
        test/test_bytecode.c
        vm/vm.c
        bytecode/bytecode.c
        bytecode/bytecode.c
        parser/parser.c
        lexer/lexer.c
        error/error.c
        dynarray/dynarray.c
        builtin/base_functions.c
)

target_link_libraries(test_bytecode m dl)

# VM test executable
add_executable(test_vm
        test/test_vm.c
        vm/vm.c
        bytecode/bytecode.c
        parser/parser.c
        lexer/lexer.c
        error/error.c
        dynarray/dynarray.c
        builtin/base_functions.c
)
target_link_libraries(test_vm m dl)

# VM module test executable
add_executable(test_vm_module
        test/test_vm_module.c
        vm/vm.c
        bytecode/bytecode.c
        parser/parser.c
        lexer/lexer.c
        error/error.c
        dynarray/dynarray.c
        builtin/base_functions.c
)
target_link_libraries(test_vm_module m dl)

# Demo math shared library
add_library(math SHARED 
        demo_math_lib.c
        vm/vm.c
        bytecode/bytecode.c
        parser/parser.c
        lexer/lexer.c
        error/error.c
        dynarray/dynarray.c
        builtin/base_functions.c
)
target_link_libraries(math m dl)

# Simple test for shared library loading
add_executable(test_simple_lib
        test/test_simple_lib.c
        vm/vm.c
        bytecode/bytecode.c
        parser/parser.c
        lexer/lexer.c
        error/error.c
        dynarray/dynarray.c
        builtin/base_functions.c
)
target_link_libraries(test_simple_lib m dl)
