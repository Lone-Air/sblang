#include <stdio.h>
#include <string.h>

int main() {
    char filename[256];
    const char* module_name = "math";
    
    // Test what filenames are being generated
    snprintf(filename, sizeof(filename), "%s.so", module_name);
    printf("Checking for: %s\n", filename);
    
    FILE* f = fopen(filename, "r");
    if (f) {
        printf("  -> File exists!\n");
        fclose(f);
    } else {
        printf("  -> File NOT found\n");
    }
    
    return 0;
}