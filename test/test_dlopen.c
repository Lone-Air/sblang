// Simple test to check library loading
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

int main() {
    printf("Testing library loading...\n");
    
    // Check if file exists
    FILE* f = fopen("math.so", "r");
    if (f) {
        printf("math.so file exists\n");
        fclose(f);
    } else {
        printf("math.so file NOT found\n");
    }
    
    // Try to load library
    void* handle = dlopen("./math.so", RTLD_LAZY);
    if (handle) {
        printf("Library loaded successfully\n");
        
        // Try to find _sbLibInit
        void* func = dlsym(handle, "_sbLibInit");
        if (func) {
            printf("_sbLibInit function found\n");
        } else {
            printf("_sbLibInit function NOT found: %s\n", dlerror());
        }
        
        dlclose(handle);
    } else {
        printf("Failed to load library: %s\n", dlerror());
    }
    
    return 0;
}