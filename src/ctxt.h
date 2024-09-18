#ifndef CTXT_H
#define CTXT_H

#include <stdlib.h>

typedef struct _MemoryRegion {
    struct _MemoryRegion *next;
    size_t capacity;
    size_t used;
    char buffer[];
} MemoryRegion;

typedef struct {
    MemoryRegion *tail;
    MemoryRegion *head;
} MemoryContext;

void ctxt_init(MemoryContext *ctxt);
void ctxt_reset(MemoryContext *ctxt);
void ctxt_free(MemoryContext *ctxt);

void *ctxt_alloc(MemoryContext *ctxt, size_t size);

#endif  // CTXT_H
