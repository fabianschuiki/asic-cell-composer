/* Copyright (c) 2016 Fabian Schuiki */
#include "lef-internal.h"



// -----------------------------------------------------------------------------
// MACRO
// -----------------------------------------------------------------------------

/**
 * Create an empty macro with a given name.
 */
lef_macro_t *
lef_new_macro(const char *name) {
	lef_macro_t *macro;
	assert(name);
	macro = calloc(1, sizeof(*macro));
	macro->name = dupstr(name);
	array_init(&macro->pins, sizeof(lef_pin_t*));
	array_init(&macro->obs, sizeof(lef_geo_t*));
	return macro;
}

/**
 * Destroy a macro.
 */
void
lef_free_macro(lef_macro_t *macro) {
	size_t z;
	assert(macro);
	for (z = 0; z < macro->pins.size; ++z) {
		lef_free_pin(array_at(macro->pins, lef_pin_t*, z));
	}
	if (macro->name) {
		free(macro->name);
	}
	array_dispose(&macro->pins);
	array_dispose(&macro->obs);
	free(macro);
}


/**
 * Add a pin to a macro.
 */
void
lef_macro_add_pin(lef_macro_t *macro, lef_pin_t *pin) {
	assert(macro && pin);
	array_add(&macro->pins, &pin);
}


/**
 * Add an obstruction to a macro.
 */
void
lef_macro_add_obs(lef_macro_t *macro, lef_geo_t *obs) {
	assert(macro && obs);
	array_add(&macro->obs, &obs);
}


/**
 * Get the name of a macro.
 */
const char *
lef_macro_get_name(lef_macro_t *macro) {
	assert(macro);
	return macro->name;
}


void
lef_macro_set_size(lef_macro_t *macro, lef_xy_t xy) {
	assert(macro);
	macro->size = xy;
}


void
lef_macro_set_origin(lef_macro_t *macro, lef_xy_t xy) {
	assert(macro);
	macro->origin = xy;
}


/**
 * Get the size of a macro.
 */
lef_xy_t
lef_macro_get_size(lef_macro_t *macro) {
	assert(macro);
	return macro->size;
}


size_t
lef_macro_get_num_pins(lef_macro_t *macro) {
	assert(macro);
	return macro->pins.size;
}


lef_pin_t *
lef_macro_get_pin(lef_macro_t *macro, size_t idx) {
	assert(macro && idx < macro->pins.size);
	return array_at(macro->pins, lef_pin_t*, idx);
}



// -----------------------------------------------------------------------------
// PORT
// -----------------------------------------------------------------------------

void
lef_port_add_geometry(lef_port_t *port, lef_geo_t *geo) {
	assert(port && geo);
	array_add(&port->geos, &geo);
}
