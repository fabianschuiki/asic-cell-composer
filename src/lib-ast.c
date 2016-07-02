/* Copyright (c) 2016 Fabian Schuiki */
#include "lib-internal.h"
#include "util.h"


static void cell_free(lib_cell_t*);
static void pin_free(lib_pin_t*);


lib_t *
lib_new(const char *name) {
	assert(name);
	lib_t *lib = calloc(1, sizeof(*lib));
	lib->name = dupstr(name);
	array_init(&lib->cells, sizeof(cell_t*));
	return lib;
}

void
lib_free(lib_t *lib) {
	assert(lib);
	free(lib->name);
	for (size_t z = 0; z < lib->cells.size; ++z) {
		cell_free(array_at(lib->cells, lib_cell_t*, z));
	}
	array_dispose(&lib->cells);
	free(lib);
}

static int
cmp_name_and_cell(const char *name, lib_cell_t **cell) {
	return strcmp(name, (*cell)->name);
}

static int
cmp_name_and_pin(const char *name, lib_pin_t **pin) {
	return strcmp(name, (*pin)->name);
}

static void *
array_find(array_t *self, const void *key, int (*compare)(const void*, const void*), size_t *pos) {
	size_t start = 0, end = self->size;
	int result;

	while (start < end) {
		size_t mid = start + (end - start) / 2;
		result = compare(key, self->items + mid * self->item_size);
		if (result < 0) {
			end = mid;
		} else if (result > 0) {
			start = mid + 1;
		} else {
			if (pos) *pos = mid;
			return self->items + mid * self->item_size;
		}
	}

	if (pos) *pos = start;
	return NULL;
}

int
lib_add_cell(lib_t *lib, const char *name, lib_cell_t **out) {
	size_t pos;
	assert(lib && name && out);

	// Find the location where the cell should be inserted. If the cell already
	// exists, return an error.
	if (array_find(&lib->cells, name, (void*)cmp_name_and_cell, &pos))
		return LIB_ERR_CELL_EXISTS;

	// Create the new cell and insert it at the location found above.
	lib_cell_t *cell = calloc(1, sizeof(*cell));
	cell->lib = lib;
	cell->name = dupstr(name);
	array_init(&cell->pins, sizeof(lib_pin_t*));
	array_insert(&lib->cells, pos, &cell);
	*out = cell;
	return LIB_OK;
}

lib_cell_t *
lib_find_cell(lib_t *lib, const char *name) {
	assert(lib && name);
	return *(lib_cell_t**)array_find(&lib->cells, name, (void*)cmp_name_and_cell, NULL);
}

static void
cell_free(lib_cell_t *cell) {
	assert(cell);
	free(cell->name);
	for (size_t z = 0; z < cell->pins.size; ++z) {
		pin_free(array_at(cell->pins, lib_pin_t*, z));
	}
	array_dispose(&cell->pins);
	free(cell);
}

int
lib_cell_add_pin(lib_cell_t *cell, const char *name, lib_pin_t **out) {
	size_t pos;
	assert(cell && name && out);

	// Find the location where the pin should be inserted. If the pin already
	// exists, return an error.
	if (array_find(&cell->pins, name, (void*)cmp_name_and_pin, &pos))
		return LIB_ERR_PIN_EXISTS;

	// Create the new pin and insert it at the location found above.
	lib_pin_t *pin = calloc(1, sizeof(*pin));
	pin->cell = cell;
	pin->name = dupstr(name);
	array_insert(&cell->pins, pos, &pin);
	*out = pin;
	return LIB_OK;
}

lib_pin_t *
lib_cell_find_pin(lib_cell_t *cell, const char *name) {
	assert(cell && name);
	return *(lib_pin_t**)array_find(&cell->pins, name, (void*)cmp_name_and_pin, NULL);
}

static void
pin_free(lib_pin_t *pin) {
	assert(pin);
	free(pin->name);
	free(pin);
}
