#ifndef MEMORY_H
#define MEMORY_H

#include <stdlib.h>

typedef struct _MemoryRegion {
    struct _MemoryRegion *next;
    size_t capacity;
    size_t used;
    char buffer[];  // NOTE: must be last
} MemoryRegion;

typedef struct {
    MemoryRegion *begin;
    MemoryRegion *end;
} MemoryContext;

void ctxt_init(MemoryContext *ctxt);
void ctxt_reset(MemoryContext *ctxt);
void ctxt_free(MemoryContext *ctxt);

void *ctxt_alloc(MemoryContext *ctxt, size_t size);
// NOTE: doesn't free the `ptr` with only difference from alloc being memcpy of old data
void *ctxt_realloc(MemoryContext *ctxt, void *ptr, size_t size);

#endif  // MEMORY_H