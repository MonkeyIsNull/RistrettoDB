#include "../embed/ristretto.h"
#include <stdio.h>

int main() {
    printf("RistrettoDB Version: %s\n", ristretto_version());
    RistrettoDB* db = ristretto_open("example.db");
    if (db) {
        printf("Database opened successfully!\n");
        ristretto_close(db);
    }
    return 0;
}
