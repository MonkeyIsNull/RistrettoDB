#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "db.h"

#define MAX_INPUT_SIZE 4096

static void print_prompt(void) {
    printf("ristretto> ");
    fflush(stdout);
}

static char* read_input(void) {
    static char buffer[MAX_INPUT_SIZE];
    
    if (!fgets(buffer, MAX_INPUT_SIZE, stdin)) {
        return NULL;
    }
    
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
    }
    
    return buffer;
}

static void query_callback(void* ctx, int n_cols, char** values, char** col_names) {
    (void)ctx;
    
    for (int i = 0; i < n_cols; i++) {
        printf("%s", col_names[i]);
        if (i < n_cols - 1) {
            printf(" | ");
        }
    }
    printf("\n");
    
    for (int i = 0; i < n_cols; i++) {
        printf("%s", values[i] ? values[i] : "NULL");
        if (i < n_cols - 1) {
            printf(" | ");
        }
    }
    printf("\n");
}

int main(int argc, char** argv) {
    const char* db_file = "ristretto.db";
    
    if (argc > 1) {
        db_file = argv[1];
    }
    
    printf("RistrettoDB v0.1.0\n");
    printf("Opening database: %s\n", db_file);
    
    RistrettoDB* db = ristretto_open(db_file);
    if (!db) {
        fprintf(stderr, "Failed to open database: %s\n", db_file);
        return 1;
    }
    
    printf("Type '.exit' to quit\n\n");
    
    while (true) {
        print_prompt();
        
        char* input = read_input();
        if (!input) {
            break;
        }
        
        if (strlen(input) == 0) {
            continue;
        }
        
        if (strcmp(input, ".exit") == 0) {
            break;
        }
        
        RistrettoResult result;
        if (strncasecmp(input, "SELECT", 6) == 0) {
            result = ristretto_query(db, input, query_callback, NULL);
        } else {
            result = ristretto_exec(db, input);
        }
        
        if (result != RISTRETTO_OK) {
            fprintf(stderr, "Error: %s\n", ristretto_error_string(result));
        } else if (strncasecmp(input, "SELECT", 6) != 0) {
            printf("OK\n");
        }
    }
    
    ristretto_close(db);
    return 0;
}
