#include "ctxt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "error.h"

#define INITIAL_REGION_SIZE 4096
#define REGION_SIZE_MULTIPLIER 2

#define MAX(a, b) ((a) < (b) ? (b) : (a))

static MemoryRegion *new_region(size_t size) {
    ASSERT(size > 0);

    MemoryRegion *region = (MemoryRegion *) malloc(sizeof(MemoryRegion) + size);
    if (region == NULL) OUT_OF_MEMORY();

    memset(region, 0, sizeof(MemoryRegion));
    region->capacity = size;

    return region;
}

static MemoryRegion *ctxt_new_region(MemoryContext *ctxt, size_t min_size) {
    ASSERT(ctxt != NULL);

    size_t size = MAX(ctxt->head->capacity, min_size) * REGION_SIZE_MULTIPLIER;
    MemoryRegion *region = new_region(size);

    ctxt->head->next = region;
    ctxt->head = region;
    return region;
}

void ctxt_init(MemoryContext *ctxt) {
    ASSERT(ctxt != NULL);

    MemoryRegion *region = new_region(INITIAL_REGION_SIZE);
    ctxt->tail = region;
    ctxt->head = region;
}

void ctxt_reset(MemoryContext *ctxt) {
    ASSERT(ctxt != NULL);

    for (MemoryRegion *current = ctxt->tail, *next = NULL; current != NULL; current = next) {
        next = current->next;
        current->used = 0;
    }
}

void ctxt_free(MemoryContext *ctxt) {
    ASSERT(ctxt != NULL);

    for (MemoryRegion *current = ctxt->tail, *next = NULL; current != NULL; current = next) {
        next = current->next;
        free(current);
    }
}

void *ctxt_alloc(MemoryContext *ctxt, size_t requested_size) {
    ASSERT(ctxt != NULL && requested_size != 0);

    // size_t is used to store allocation size and void* for alignment
    size_t size = requested_size + sizeof(size_t) + sizeof(void *);

    MemoryRegion *current = ctxt->tail;
    for (MemoryRegion *next = NULL; current != NULL; current = next) {
        next = current->next;
        if (current->used + size <= current->capacity) break;
    }
    if (current == NULL) current = ctxt_new_region(ctxt, size);

    size_t alignment = sizeof(void *) - (current->used % sizeof(void *));
    size_t *ptr = (size_t *) (current->buffer + current->used + alignment);
    current->used += size;
    *ptr = requested_size;
    return (void *) (ptr + 1);
}

void *ctxt_realloc(MemoryContext *ctxt, void *old_ptr, size_t new_size) {
    ASSERT(ctxt != NULL && new_size != 0 && old_ptr != NULL);

    size_t old_size = *(((size_t *) old_ptr) - 1);
    ASSERT(new_size > old_size);

    void *new_ptr = ctxt_alloc(ctxt, new_size);
    memcpy(new_ptr, old_ptr, old_size);

    return new_ptr;
}
