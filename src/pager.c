#include "pager.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static void ensure_file_size(int fd, size_t min_size) {
    struct stat st;
    if (fstat(fd, &st) == -1) {
        return;
    }
    
    if ((size_t)st.st_size < min_size) {
        if (ftruncate(fd, min_size) == -1) {
            return;
        }
    }
}

static MappedFile* mapped_file_open(const char* filename) {
    MappedFile* file = malloc(sizeof(MappedFile));
    if (!file) {
        return NULL;
    }
    
    file->fd = open(filename, O_RDWR | O_CREAT, 0644);
    if (file->fd == -1) {
        free(file);
        return NULL;
    }
    
    ensure_file_size(file->fd, PAGE_SIZE);
    
    struct stat st;
    if (fstat(file->fd, &st) == -1) {
        close(file->fd);
        free(file);
        return NULL;
    }
    
    file->file_size = st.st_size;
    file->mapped_size = (file->file_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    
    file->mapped_memory = mmap(NULL, file->mapped_size, 
                               PROT_READ | PROT_WRITE, 
                               MAP_SHARED, file->fd, 0);
    
    if (file->mapped_memory == MAP_FAILED) {
        close(file->fd);
        free(file);
        return NULL;
    }
    
    file->data = (uint8_t*)file->mapped_memory;
    
    return file;
}

static void mapped_file_close(MappedFile* file) {
    if (!file) {
        return;
    }
    
    if (file->mapped_memory != MAP_FAILED) {
        munmap(file->mapped_memory, file->mapped_size);
    }
    
    if (file->fd != -1) {
        close(file->fd);
    }
    
    free(file);
}

static void mapped_file_resize(MappedFile* file, size_t new_size) {
    if (new_size <= file->mapped_size) {
        file->file_size = new_size;
        return;
    }
    
    munmap(file->mapped_memory, file->mapped_size);
    
    ensure_file_size(file->fd, new_size);
    
    file->file_size = new_size;
    file->mapped_size = (new_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    
    file->mapped_memory = mmap(NULL, file->mapped_size,
                               PROT_READ | PROT_WRITE,
                               MAP_SHARED, file->fd, 0);
    
    file->data = (uint8_t*)file->mapped_memory;
}

Pager* pager_open(const char* filename) {
    Pager* pager = malloc(sizeof(Pager));
    if (!pager) {
        return NULL;
    }
    
    pager->file = mapped_file_open(filename);
    if (!pager->file) {
        free(pager);
        return NULL;
    }
    
    memset(pager->pages, 0, sizeof(pager->pages));
    
    pager->num_pages = pager->file->file_size / PAGE_SIZE;
    if (pager->num_pages == 0) {
        pager->num_pages = 1;
    }
    
    return pager;
}

void pager_close(Pager* pager) {
    if (!pager) {
        return;
    }
    
    pager_sync(pager);
    
    mapped_file_close(pager->file);
    free(pager);
}

void* pager_get_page(Pager* pager, uint32_t page_num) {
    if (page_num >= TABLE_MAX_PAGES) {
        return NULL;
    }
    
    if (pager->pages[page_num] == NULL) {
        if (page_num >= pager->num_pages) {
            size_t new_size = (page_num + 1) * PAGE_SIZE;
            mapped_file_resize(pager->file, new_size);
            pager->num_pages = page_num + 1;
        }
        
        size_t offset = page_num * PAGE_SIZE;
        pager->pages[page_num] = pager->file->data + offset;
        
        __builtin_prefetch(pager->pages[page_num], 0, 3);
    }
    
    return pager->pages[page_num];
}

void pager_flush_page(Pager* pager, uint32_t page_num) {
    if (page_num >= pager->num_pages || !pager->pages[page_num]) {
        return;
    }
    
    size_t offset = page_num * PAGE_SIZE;
    msync(pager->file->data + offset, PAGE_SIZE, MS_ASYNC);
}

void pager_sync(Pager* pager) {
    if (!pager || !pager->file) {
        return;
    }
    
    msync(pager->file->mapped_memory, pager->file->mapped_size, MS_SYNC);
}

uint32_t pager_allocate_page(Pager* pager) {
    uint32_t new_page_num = pager->num_pages;
    
    void* page = pager_get_page(pager, new_page_num);
    if (page) {
        memset(page, 0, PAGE_SIZE);
    }
    
    return new_page_num;
}
