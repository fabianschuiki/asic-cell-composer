/* Copyright (c) 2016 Fabian Schuiki */
#include "common.h"
#include "util.h"


static void *
locate(ptrset_t *set, void *key, size_t *pos) {
	size_t start = 0, end = set->size;

	while (start < end) {
		size_t mid = start + (end - start) / 2;
		void *ptr = set->items[mid];
		if (key < ptr) {
			end = mid;
		} else if (key > ptr) {
			start = mid + 1;
		} else {
			if (pos) *pos = mid;
			return ptr;
		}
	}

	if (pos) *pos = start;
	return NULL;
}


void
ptrset_init(ptrset_t *set) {
	assert(set);
	memset(set, 0, sizeof(*set));
}


void
ptrset_dispose(ptrset_t *set) {
	assert(set);
	if (set->items)
		free(set->items);
	memset(set, 0, sizeof(*set));
}


/**
 * Add a pointer to a pointer set.
 *
 * @return `true` if the pointer did not exist in the set and was added, `false`
 * otherwise.
 */
bool
ptrset_add(ptrset_t *set, void *ptr) {
	assert(set && ptr);

	// Lookup the position where the item would have to be inserted. If the item
	// already exists, we're done.
	size_t pos;
	if (locate(set, ptr, &pos)) {
		return false;
	}

	// Increase the size of the items array if need be.
	if (set->size == set->capacity) {
		if (set->capacity == 0) {
			set->capacity = 4;
			set->items = malloc(set->capacity * sizeof(void*));
		} else {
			set->capacity *= 2;
			set->items = realloc(set->items, set->capacity * sizeof(void*));
		}
	}

	// Push everything after the position towards the back.
	if (pos != set->size) {
		memmove(set->items + pos + 1, set->items + pos, (set->size - pos) * sizeof(void*));
	}
	set->items[pos] = ptr;
	++set->size;

	return true;
}


/**
 * Remove a pointer from a pointer set.
 *
 * @return `true` if the pointer was found and removed, `false` otherwise.
 */
bool
ptrset_remove(ptrset_t *set, void *ptr) {
	assert(set && ptr);

	// Lookup the position of the item in the array. If the item does not exist,
	// we're done.
	size_t pos;
	if (!locate(set, ptr, &pos)) {
		return false;
	}

	// Move everything after the item towards towards the front.
	if (pos != set->size) {
		memmove(set->items + pos, set->items + pos + 1, (set->size - pos - 1) * sizeof(void*));
	}
	--set->size;

	return true;
}


/**
 * Check whether a pointer set contains a specific pointer.
 */
bool
ptrset_contains(ptrset_t *set, void *ptr) {
	assert(set && ptr);
	return locate(set, ptr, NULL) != NULL;
}
