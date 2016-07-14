/* Copyright (c) 2016 Fabian Schuiki */
#pragma once
#include "common.h"

typedef struct lib lib_t;
typedef struct lib_cell lib_cell_t;
typedef struct lib_pin lib_pin_t;
typedef struct lib_timing lib_timing_t;
typedef struct lib_table lib_table_t;
typedef struct lib_table_format lib_table_format_t;

/**
 * Errors produced by the LIB functions.
 */
enum lib_error {
	LIB_OK = 0,
	LIB_ERR_SYNTAX,
	LIB_ERR_CELL_EXISTS,
	LIB_ERR_PIN_EXISTS,
	LIB_ERR_TEMPLATE_EXISTS,
	LIB_ERR_TABLE_EXISTS,
};

enum lib_pin_direction {
	LIB_PIN_IN = 1,
	LIB_PIN_OUT,
	LIB_PIN_INOUT,
	LIB_PIN_INTERNAL,
};

enum lib_timing_type {
	LIB_TMG_UNKNOWN = 0,

	// Edge sensitivity
	LIB_TMG_EDGE_MASK = 0xF,
	LIB_TMG_EDGE_NONE = 0x0,
	LIB_TMG_EDGE_RISE = 0x1,
	LIB_TMG_EDGE_FALL = 0x2,
	LIB_TMG_EDGE_BOTH = 0x3,

	// Applicable Cell Types
	LIB_TMG_CELL_MASK = 0xF0,
	LIB_TMG_CELL_COMB = 0x10,
	LIB_TMG_CELL_SEQ  = 0x20,
	LIB_TMG_CELL_BOTH = 0x30,

	// Timing Types
	LIB_TMG_TYPE_MASK              = 0xF00,

	LIB_TMG_TYPE_COMB              = 0x100 | LIB_TMG_CELL_COMB,
	LIB_TMG_TYPE_TRI_EN            = 0x200 | LIB_TMG_CELL_COMB,
	LIB_TMG_TYPE_TRI_DIS           = 0x300 | LIB_TMG_CELL_COMB,

	LIB_TMG_TYPE_EDGE              = 0x400 | LIB_TMG_CELL_SEQ,
	LIB_TMG_TYPE_PRESET            = 0x500 | LIB_TMG_CELL_SEQ,
	LIB_TMG_TYPE_CLEAR             = 0x600 | LIB_TMG_CELL_SEQ,
	LIB_TMG_TYPE_HOLD              = 0x700 | LIB_TMG_CELL_SEQ,
	LIB_TMG_TYPE_SETUP             = 0x800 | LIB_TMG_CELL_SEQ,
	LIB_TMG_TYPE_RECOVERY          = 0x900 | LIB_TMG_CELL_SEQ,
	LIB_TMG_TYPE_SKEW              = 0xA00 | LIB_TMG_CELL_SEQ,
	LIB_TMG_TYPE_REMOVAL           = 0xB00 | LIB_TMG_CELL_SEQ,
	LIB_TMG_TYPE_MIN_PERIOD        = 0xC00 | LIB_TMG_CELL_SEQ,
	LIB_TMG_TYPE_MIN_PULSE_WIDTH   = 0xD00 | LIB_TMG_CELL_BOTH,
	LIB_TMG_TYPE_MAX_CLK_TREE_PATH = 0xE00 | LIB_TMG_CELL_SEQ,
	LIB_TMG_TYPE_MIN_CLK_TREE_PATH = 0xF00 | LIB_TMG_CELL_SEQ,
};

enum lib_timing_sense {
	LIB_TMG_POSITIVE_UNATE,
	LIB_TMG_NEGATIVE_UNATE,
	LIB_TMG_NON_UNATE
};

enum lib_model_parameter {
	LIB_MODEL_INDEX_MASK = 0xF,

	// Edge the parameter applies to.
	LIB_MODEL_EDGE_MASK  = 0x1,
	LIB_MODEL_EDGE_RISE  = 0x0, // Rising Edge
	LIB_MODEL_EDGE_FALL  = 0x1, // Falling Edge

	// Dimensionality information.
	LIB_MODEL_DIM_MASK   = 0x10,
	LIB_MODEL_SCALAR     = 0x00, // Scalar
	LIB_MODEL_TABLE      = 0x10, // Multidimensional Table

	// Model types.
	LIB_MODEL_TYPE_MASK  = 0x20,
	LIB_MODEL_LINEAR     = 0x00, // CMOS Linear Model
	LIB_MODEL_NONLINEAR  = 0x20, // CMOS Non-Linear Model

	// Linear model.
	LIB_MODEL_INTRINSIC_RISE   = 0x0 | LIB_MODEL_SCALAR | LIB_MODEL_LINEAR,
	LIB_MODEL_INTRINSIC_FALL   = 0x1 | LIB_MODEL_SCALAR | LIB_MODEL_LINEAR,
	LIB_MODEL_RESISTANCE_RISE  = 0x2 | LIB_MODEL_SCALAR | LIB_MODEL_LINEAR,
	LIB_MODEL_RESISTANCE_FALL  = 0x3 | LIB_MODEL_SCALAR | LIB_MODEL_LINEAR,

	// Non-linear model.
	LIB_MODEL_CELL_RISE        = 0x4 | LIB_MODEL_TABLE | LIB_MODEL_NONLINEAR,
	LIB_MODEL_CELL_FALL        = 0x5 | LIB_MODEL_TABLE | LIB_MODEL_NONLINEAR,
	LIB_MODEL_PROPAGATION_RISE = 0x6 | LIB_MODEL_TABLE | LIB_MODEL_NONLINEAR,
	LIB_MODEL_PROPAGATION_FALL = 0x7 | LIB_MODEL_TABLE | LIB_MODEL_NONLINEAR,
	LIB_MODEL_TRANSITION_RISE  = 0x8 | LIB_MODEL_TABLE | LIB_MODEL_NONLINEAR,
	LIB_MODEL_TRANSITION_FALL  = 0x9 | LIB_MODEL_TABLE | LIB_MODEL_NONLINEAR,
	LIB_MODEL_CONSTRAINT_RISE  = 0xA | LIB_MODEL_TABLE | LIB_MODEL_NONLINEAR,
	LIB_MODEL_CONSTRAINT_FALL  = 0xB | LIB_MODEL_TABLE | LIB_MODEL_NONLINEAR,

	LIB_MODEL_NUM_PARAMS       = 0xC,
};

enum lib_table_variable {
	LIB_VAR_UNIT_MASK   = 0x3,
	LIB_VAR_UNIT_TIME   = 0x0,
	LIB_VAR_UNIT_CAP    = 0x1,
	LIB_VAR_UNIT_LENGTH = 0x2,

	LIB_VAR_NONE = 0,
	LIB_VAR_IN_TRAN        = 0x10 | LIB_VAR_UNIT_TIME,
	LIB_VAR_OUT_CAP_TOTAL  = 0x20 | LIB_VAR_UNIT_CAP,
	LIB_VAR_OUT_CAP_PIN    = 0x30 | LIB_VAR_UNIT_CAP,
	LIB_VAR_OUT_CAP_WIRE   = 0x40 | LIB_VAR_UNIT_CAP,
	LIB_VAR_OUT_NET_LENGTH = 0x50 | LIB_VAR_UNIT_LENGTH,
	LIB_VAR_CON_TRAN       = 0x60 | LIB_VAR_UNIT_TIME,
	LIB_VAR_REL_TRAN       = 0x70 | LIB_VAR_UNIT_TIME,
	LIB_VAR_REL_CAP_TOTAL  = 0x80 | LIB_VAR_UNIT_CAP,
	LIB_VAR_REL_CAP_PIN    = 0x90 | LIB_VAR_UNIT_CAP,
	LIB_VAR_REL_CAP_WIRE   = 0xA0 | LIB_VAR_UNIT_CAP,
	LIB_VAR_REL_NET_LENGTH = 0xB0 | LIB_VAR_UNIT_LENGTH,
};

const char *lib_errstr(int err);

int lib_read(lib_t**, const char*);
int lib_write(lib_t*, const char*);

lib_t *lib_new(const char *name);
void lib_free(lib_t*);
int lib_add_cell(lib_t*, const char *name, lib_cell_t**);
lib_cell_t *lib_find_cell(lib_t*, const char *name);
unsigned lib_get_num_cells(lib_t*);
lib_cell_t *lib_get_cell(lib_t*, unsigned);
void lib_set_capacitance_unit(lib_t*, double);
void lib_set_leakage_power_unit(lib_t*, double);
double lib_get_capacitance_unit(lib_t*);
double lib_get_leakage_power_unit(lib_t*);
int lib_add_lut_template(lib_t*, const char*, lib_table_format_t**);
lib_table_format_t *lib_find_lut_template(lib_t*, const char*);

int lib_cell_add_pin(lib_cell_t*, const char *name, lib_pin_t**);
lib_pin_t *lib_cell_find_pin(lib_cell_t*, const char *name);
const char *lib_cell_get_name(lib_cell_t*);
unsigned lib_cell_get_num_pins(lib_cell_t*);
lib_pin_t *lib_cell_get_pin(lib_cell_t*, unsigned);
void lib_cell_set_leakage_power(lib_cell_t*, double);
double lib_cell_get_leakage_power(lib_cell_t*);

const char *lib_pin_get_name(lib_pin_t*);
void lib_pin_set_capacitance(lib_pin_t*, double);
double lib_pin_get_capacitance(lib_pin_t*);
lib_timing_t *lib_pin_add_timing(lib_pin_t*);
unsigned lib_pin_get_num_timings(lib_pin_t*);
lib_timing_t *lib_pin_get_timing(lib_pin_t*, unsigned);

int lib_timing_add_table(lib_timing_t*, unsigned, lib_table_t**);
lib_table_t *lib_timing_find_table(lib_timing_t*, unsigned);
unsigned lib_timing_get_num_related_pins(lib_timing_t*);
const char *lib_timing_get_related_pin(lib_timing_t*, unsigned);
void lib_timing_add_related_pin(lib_timing_t*, const char*);
void lib_timing_set_type(lib_timing_t*, unsigned);
void lib_timing_set_sense(lib_timing_t*, unsigned);
unsigned lib_timing_get_type(lib_timing_t*);
unsigned lib_timing_get_sense(lib_timing_t*);
void lib_timing_set_scalar(lib_timing_t*, unsigned, double);
double lib_timing_get_scalar(lib_timing_t*, unsigned);

unsigned lib_table_get_num_dims(lib_table_t*);
unsigned lib_table_get_variable(lib_table_t*, unsigned);
unsigned lib_table_get_num_indices(lib_table_t*, unsigned);
double *lib_table_get_indices(lib_table_t*, unsigned);
unsigned lib_table_get_num_values(lib_table_t*);
double *lib_table_get_values(lib_table_t*);
void lib_table_set_variable(lib_table_t*, unsigned, unsigned);
void lib_table_set_indices(lib_table_t*, unsigned, unsigned, double*);
void lib_table_set_values(lib_table_t*, unsigned, double*);
void lib_table_set_stride(lib_table_t*, unsigned, unsigned);
