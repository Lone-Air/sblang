# Semantic Bridge Language - SBLang

---
### SB-Lang
A simple & easy program language <br>
### Build
```bash
git clone https://github.com/Lone-Air/sblang
cd sblang && mkdir build-dir && cd build-dir
cmake ..
make -j
```
### Command Line Compiler
```aiignore
SB Language Compiler & Runtime v1.0.0
Create by Laman28 - Release under LGPL License
Usage: sbl [options] file1 [file2 ...]

Options:
  -c              Compile only (generate .sbc files)
  -o <file>       Specify output file (only with single input file)
  -v, --version   Show version information
  -h, --help      Show this help message

Examples:
  sbl program.sb              Execute a source file
  sbl program.sbc             Execute a bytecode file
  sbl -c program.sb           Compile to bytecode
  sbl -c -o out.sbc prog.sb   Compile with custom output
```
### Demo / Syntax rule
```sblang
function f1(){ ... } // Define a function
struct s1{ member1, member2, ... } // Define a structure

// ';' after segment is required

var1 = 10; // Create a local global and set it to number: 10
var2 = "String"; // Create a local global and set it to string: "String"

global var1; // Make var1 become a global variable

Instance1 -> s1; // Create a structure instance (store as a variable)
Instance1.member1 = 20; // Set member1 of the instanace of structure s1 "Instance1" to number: 20
Instance1.member2 = f1();
// Set member2 of the instance of structure s1 "Instance1" to the return value of function f1()

/*
 * This is a multiple line note
 */

print(toString(Instance1.member1) + toString(Instance1.member2), '\n');
// First let number 20 transfer to string '20'
// Second let the content of Instance1.member2 transfer to string type
// Combine them with '\n' then print them into the terminal
// print is a built-in native function

load math; // Load the built-in module math
// It supplies native functions like: sin, log, abs, etc.

// This is a stmt_for, its syntax like other known languages
// stmt_for: for(statment_1; statment_2; statment_3){ loop_body }
for(i = 0; i < 10; i = i + 1){ // syntax like `i++` is unsupported
    print(i, '\n');
}

j = 0; // Set local variable j to 0

// stmt_while: while(condition){ loop_body; }
while(j < 100){
    j = j + 1;
}

// stmt_if: if(condition){ then_branch } else { else_branch }
if(500 > 20){
    print("I should be printed\n");
}
else{
    print("I should not be printed\n");
}

/* The following is a recursive program demonstration */

function f2(arg1){
    if (arg1 > 1024){
        return arg1;
    }
    else{
        return f1(arg1 + 1);
    }
}

print("result: ", f2(0), '\n'); // It should output: 1025

/* Run it in shell
$ sbl demo.sb
--- Running 'demo.sb'...
result:  1025 
*/

```
### Thanks for using
This programming language is just for fun and may contain unknown bugs and memory errors.