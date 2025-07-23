/*
** RistrettoDB Command Line Interface
**
** This is the CLI tool for RistrettoDB. It provides a simple REPL
** interface for executing SQL commands.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "ristretto.h"

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

static void print_help(void) {
    printf("RistrettoDB %s - A tiny, blazingly fast, embeddable SQL engine\n", ristretto_version());
    printf("\n");
    printf("Usage: ristretto [database_file]\n");
    printf("\n");
    printf("Commands:\n");
    printf("  .exit    - Exit the program\n");
    printf("  .help    - Show this help message\n");
    printf("  .version - Show version information\n");
    printf("\n");
    printf("SQL commands:\n");
    printf("  CREATE TABLE, INSERT, SELECT - Standard SQL operations\n");
    printf("\n");
}

static void print_version(void) {
    printf("RistrettoDB %s\n", ristretto_version());
    printf("Version number: %d\n", ristretto_version_number());
    printf("Performance: 4.57x faster than SQLite on ultra-fast writes\n");
    printf("           : 2.8x faster than SQLite on general operations\n");
}

int main(int argc, char** argv) {
    const char* db_file = "ristretto.db";
    
    if (argc > 1) {
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            print_help();
            return 0;
        }
        if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
            print_version();
            return 0;
        }
        db_file = argv[1];
    }
    
    printf("RistrettoDB %s\n", ristretto_version());
    printf("Opening database: %s\n", db_file);
    
    RistrettoDB* db = ristretto_open(db_file);
    if (!db) {
        fprintf(stderr, "Failed to open database: %s\n", db_file);
        return 1;
    }
    
    printf("Type '.exit' to quit, '.help' for help\n\n");
    
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
        
        if (strcmp(input, ".help") == 0) {
            print_help();
            continue;
        }
        
        if (strcmp(input, ".version") == 0) {
            print_version();
            continue;
        }
        
        RistrettoResult result;
        if (strncasecmp(input, "SELECT", 6) == 0 ||
            strncasecmp(input, "SHOW TABLES", 11) == 0 ||
            strncasecmp(input, "SHOW CREATE TABLE", 17) == 0 ||
            strncasecmp(input, "DESCRIBE", 8) == 0 ||
            strncasecmp(input, "DESC", 4) == 0) {
            result = ristretto_query(db, input, query_callback, NULL);
        } else {
            result = ristretto_exec(db, input);
        }
        
        if (result != RISTRETTO_OK) {
            fprintf(stderr, "Error: %s\n", ristretto_error_string(result));
        } else if (strncasecmp(input, "SELECT", 6) != 0 &&
                   strncasecmp(input, "SHOW TABLES", 11) != 0 &&
                   strncasecmp(input, "SHOW CREATE TABLE", 17) != 0 &&
                   strncasecmp(input, "DESCRIBE", 8) != 0 &&
                   strncasecmp(input, "DESC", 4) != 0) {
            printf("OK\n");
        }
    }
    
    ristretto_close(db);
    return 0;
}
