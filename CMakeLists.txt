cmake_minimum_required(VERSION 4.0)

project(SBLang)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c23 -g")

set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

set(CMAKE_BUILD_RPATH
        ${CMAKE_CURRENT_BINARY_DIR}
)

option(ENABLE_GNU_READLINE "Enable GNU Readline support" OFF)

list(APPEND CMAKE_INSTALL_RPATH
        "$ORIGIN"                              # 可执行文件所在目录
        "$ORIGIN/lib"                          # 可执行文件目录下的lib
        "$ORIGIN/../lib/sblang"                # 相对路径
        "${CMAKE_INSTALL_PREFIX}/lib/sblang"   # 绝对安装路径
)

# file(GLOB SRC "dynarray/*.c", "lexer/*.c", "parser/parser.c")

# Main SBL executable - Compiler frontend and runtime

add_library(sblang SHARED # runtime
        vm/vm.c
        bytecode/bytecode.c
        parser/parser.c
        lexer/lexer.c
        error/error.c
        dynarray/dynarray.c
        builtin/base_functions.c
)

add_executable(sbl  # compiler
        sbl.c
)
target_link_libraries(sbl m dl sblang)

add_executable(bytecode_read  # compiler
        test/test_bytecode.c
)
target_link_libraries(bytecode_read m dl sblang)

# Native Math library
add_library(math SHARED
        builtin/libraries/math.c
)
target_link_libraries(math m sblang)

set_target_properties(math PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/sblang
)

set_target_properties(sblang PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

set_target_properties(sbl PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

install(TARGETS math
        LIBRARY DESTINATION lib/sblang
        RUNTIME DESTINATION bin
)

install(TARGETS sblang
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
)

install(TARGETS sbl
        RUNTIME DESTINATION bin
)

install(FILES
        builtin/base_functions.h
        bytecode/bytecode.h
        lexer/lexer.h
        parser/parser.h
        vm/vm.h
        information.h
        DESTINATION include/sblang
)

if(ENABLE_GNU_READLINE)
    #find_package(readline)
    find_library(READLINE_FOUND readline)
    if(READLINE_FOUND)
        message(STATUS "GNU Readline support enabled")
        add_compile_definitions(ENABLE_READLINE)
        target_link_libraries(sblang readline)
        target_link_libraries(sbl m dl sblang readline)
    else()
        message(FATAL_ERROR "GNU Readline requested but not found")
    endif()
else()
    message(STATUS "Readline support disabled")
endif()
