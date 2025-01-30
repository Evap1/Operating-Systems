#ifndef MY_STDLIB_H
#define MY_STDLIB_H

#include <stddef.h>
#include <stdio.h>

void *smalloc(size_t size);
void *scalloc(size_t num, size_t size);
void sfree(void *p);
void *srealloc(void *oldp, size_t size);

size_t _num_free_blocks();
size_t _num_free_bytes();
size_t _num_allocated_blocks();
size_t _num_allocated_bytes();
size_t _num_meta_data_bytes();
size_t _size_meta_data();

    void printHeapStatistics() {
        printf("Heap Statistics:\n");
        printf("Total Allocated Blocks: %zu\n", _num_allocated_blocks());
        printf("Total Allocated Bytes: %zu\n",  _num_allocated_bytes());
        printf("Free Blocks: %zu\n",            _num_free_blocks());
        printf("Free Bytes: %zu\n",             _num_free_bytes());
        printf("Metadata Bytes: %zu\n\n",         _num_free_blocks() * 32);
    }
#endif /* MY_STDLIB_H */
