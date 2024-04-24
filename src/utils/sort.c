#include "sort.h"
#include <stdlib.h>
#include <string.h>
#include "git/state.h"
#include "utils/vector.h"

static void swap(size_t *arr, size_t i, size_t j) {
    size_t temp = arr[i];
    arr[i] = arr[j];
    arr[j] = temp;
}

static int compare_files(const File *files, size_t i, size_t j) { return strcmp(files[i].dst + 2, files[j].dst + 2); }

static void quicksort(const File *files, size_t *arr, size_t l, size_t r) {
    if (l >= r) return;

    size_t pivot = arr[r];
    size_t i = l, j = r;
    while (i < j) {
        while (compare_files(files, arr[i], pivot) < 0 && i < j) i++;
        while (compare_files(files, arr[j], pivot) >= 0 && i < j) j--;
        if (i < j) swap(arr, i, j);
    }
    swap(arr, i, r);

    if (i > 0) quicksort(files, arr, l, i - 1);
    quicksort(files, arr, i + 1, r);
}

size_vec sort_files(const FileVec *files) {
    size_vec indexes = {0};

    for (size_t i = 0; i < files->length; i++) VECTOR_PUSH(&indexes, i);
    quicksort(files->data, indexes.data, 0, files->length - 1);

    return indexes;
}
