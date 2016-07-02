/* Copyright (c) 2016 Fabian Schuiki */
#pragma once
#include "common.h"

typedef struct lib lib_t;
typedef struct lib_cell lib_cell_t;
typedef struct lib_pin lib_pin_t;

/**
 * Errors produced by the LIB functions.
 */
enum lib_error {
	LIB_OK = 0,
	LIB_ERR_SYNTAX,
	LIB_ERR_CELL_EXISTS,
	LIB_ERR_PIN_EXISTS,
};

enum lib_pin_direction {
	LIB_PIN_IN,
	LIB_PIN_OUT,
	LIB_PIN_INOUT,
	LIB_PIN_INTERNAL,
};

int lib_read(const char*, lib_t**);
int lib_write(const char*, lib_t*);

lib_t *lib_new(const char *name);
void lib_free(lib_t*);
int lib_add_cell(lib_t*, const char *name, lib_cell_t**);
lib_cell_t *lib_find_cell(lib_t*, const char *name);

int lib_cell_add_pin(lib_cell_t*, const char *name, lib_pin_t**);
lib_pin_t *lib_cell_find_pin(lib_cell_t*, const char *name);
