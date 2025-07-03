#include <stdlib.h>
#include <string.h>
#include <stdint.h>

void* safe_malloc(size_t size) {
    void* ptr = malloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

void* safe_realloc(void* ptr, size_t old_size, size_t new_size) {
    void* new_ptr = realloc(ptr, new_size);
    if (new_ptr && new_size > old_size) {
        memset((char*)new_ptr + old_size, 0, new_size - old_size);
    }
    return new_ptr;
}

uint32_t hash_string(const char* str) {
    uint32_t hash = 5381;
    int c;
    
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    
    return hash;
}

char* string_duplicate(const char* str) {
    if (!str) {
        return NULL;
    }
    
    size_t len = strlen(str);
    char* copy = malloc(len + 1);
    if (copy) {
        memcpy(copy, str, len + 1);
    }
    return copy;
}

int string_compare_case_insensitive(const char* a, const char* b) {
    while (*a && *b) {
        int ca = *a;
        int cb = *b;
        
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        
        if (ca != cb) {
            return ca - cb;
        }
        
        a++;
        b++;
    }
    
    return *a - *b;
}
