/* Copyright (c) 2016 Fabian Schuiki */
#include "lib-internal.h"
#include "util.h"


static void cell_free(lib_cell_t*);
static void pin_free(lib_phx_pin_t*);


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
cmp_name_and_pin(const char *name, lib_phx_pin_t **pin) {
	return strcmp(name, (*pin)->name);
}

int
lib_add_cell(lib_t *lib, const char *name, lib_cell_t **out) {
	unsigned pos;
	assert(lib && name && out);

	// Find the location where the cell should be inserted. If the cell already
	// exists, return an error.
	if (array_bsearch(&lib->cells, name, (void*)cmp_name_and_cell, &pos))
		return LIB_ERR_CELL_EXISTS;

	// Create the new cell and insert it at the location found above.
	lib_cell_t *cell = calloc(1, sizeof(*cell));
	cell->lib = lib;
	cell->name = dupstr(name);
	array_init(&cell->pins, sizeof(lib_phx_pin_t*));
	array_insert(&lib->cells, pos, &cell);
	*out = cell;
	return LIB_OK;
}

lib_cell_t *
lib_find_cell(lib_t *lib, const char *name) {
	assert(lib && name);
	return *(lib_cell_t**)array_bsearch(&lib->cells, name, (void*)cmp_name_and_cell, NULL);
}

unsigned
lib_get_num_cells(lib_t *lib) {
	assert(lib);
	return lib->cells.size;
}

lib_cell_t *
lib_get_cell(lib_t *lib, unsigned idx) {
	assert(lib && idx < lib->cells.size);
	return array_at(lib->cells, lib_cell_t*, idx);
}

double
lib_get_capacitance_unit(lib_t *lib) {
	return 1e-12;
}



static void
cell_free(lib_cell_t *cell) {
	assert(cell);
	free(cell->name);
	for (size_t z = 0; z < cell->pins.size; ++z) {
		pin_free(array_at(cell->pins, lib_phx_pin_t*, z));
	}
	array_dispose(&cell->pins);
	free(cell);
}

int
lib_cell_add_pin(lib_cell_t *cell, const char *name, lib_phx_pin_t **out) {
	unsigned pos;
	assert(cell && name && out);

	// Find the location where the pin should be inserted. If the pin already
	// exists, return an error.
	if (array_bsearch(&cell->pins, name, (void*)cmp_name_and_pin, &pos))
		return LIB_ERR_PIN_EXISTS;

	// Create the new pin and insert it at the location found above.
	lib_phx_pin_t *pin = calloc(1, sizeof(*pin));
	pin->cell = cell;
	pin->name = dupstr(name);
	array_insert(&cell->pins, pos, &pin);
	*out = pin;
	return LIB_OK;
}

lib_phx_pin_t *
lib_cell_find_pin(lib_cell_t *cell, const char *name) {
	assert(cell && name);
	return *(lib_phx_pin_t**)array_bsearch(&cell->pins, name, (void*)cmp_name_and_pin, NULL);
}

const char *
lib_cell_get_name(lib_cell_t *cell) {
	assert(cell);
	return cell->name;
}

unsigned
lib_cell_get_num_pins(lib_cell_t *cell) {
	return cell->pins.size;
}

lib_phx_pin_t *
lib_cell_get_pin(lib_cell_t *cell, unsigned idx) {
	assert(cell && idx < cell->pins.size);
	return array_at(cell->pins, lib_phx_pin_t*, idx);
}



static void
pin_free(lib_phx_pin_t *pin) {
	assert(pin);
	free(pin->name);
	free(pin);
}

const char *
lib_pin_get_name(lib_phx_pin_t *pin) {
	assert(pin);
	return pin->name;
}

double
lib_pin_get_capacitance(lib_phx_pin_t *pin) {
	assert(pin);
	return pin->capacitance;
}
