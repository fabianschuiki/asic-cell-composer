/* Copyright (c) 2016 Fabian Schuiki */
#pragma once
#include "common.h"


typedef struct array array_t;


/**
 * A dynamic array.
 */
struct array {
	/// Number of items in the array.
	unsigned size;
	/// How many items may be stored in the memory region before it needs to
	/// be reallocated.
	unsigned capacity;
	/// Memory region that holds the items.
	/// Size of one array item.
	unsigned item_size;
	void* items;
};

void array_init(array_t *self, unsigned item_size);
void array_dispose(array_t *self);

void array_reserve(array_t *self, unsigned capacity);
void array_resize(array_t *self, unsigned items);
void array_shrink(array_t *self);

void* array_get(const array_t *self, unsigned index);
void array_get_many(const array_t *self, unsigned index, void *items, unsigned num_items);
void array_set(array_t *self, unsigned index, const void *item);
void array_set_many(array_t *self, unsigned index, const void *items, unsigned num_items);

void* array_insert(array_t *self, unsigned index, const void *item);
void* array_insert_many(array_t *self, unsigned index, const void *items, unsigned num_items);

void array_erase(array_t *self, unsigned index);
void array_erase_range(array_t *self, unsigned first, unsigned last);

void* array_add(array_t *self, const void *item);
void* array_add_many(array_t *self, const void *items, unsigned num_items);
void array_remove(array_t *self);
void array_remove_many(array_t *self, unsigned num_items);

void array_clear(array_t *self);
#define array_at(arr, type, index) (((type*)(arr).items)[index])

void *array_bsearch(array_t *self, const void *key, int (*compare)(const void*, const void*), unsigned *pos);


/* String and memory duplication */
char *dupstr(const char *src);
char *dupstrn(const char *src, size_t len);
void *dupmem(const void *src, size_t len);


/* Reference counting */
void ref(void*);
void unref(void*);
