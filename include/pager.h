#ifndef RISTRETTO_PAGER_H
#define RISTRETTO_PAGER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define PAGE_SIZE 4096
#define TABLE_MAX_PAGES 1000

typedef struct {
    uint8_t *data;
    size_t file_size;
    int fd;
    void *mapped_memory;
    size_t mapped_size;
} MappedFile;

typedef struct {
    MappedFile *file;
    void *pages[TABLE_MAX_PAGES];
    uint32_t num_pages;
} Pager;

Pager* pager_open(const char *filename);
void pager_close(Pager *pager);

void* pager_get_page(Pager *pager, uint32_t page_num);
void pager_flush_page(Pager *pager, uint32_t page_num);
void pager_sync(Pager *pager);

uint32_t pager_allocate_page(Pager *pager);

#endif
