/* Copyright (c) 2016 Fabian Schuiki */
#pragma once
#include "lib.h"
#include "util.h"

typedef struct lib_lexer lib_lexer_t;

enum lib_token {
	LIB_EOF = 0,
	LIB_LPAREN,
	LIB_RPAREN,
	LIB_LBRACE,
	LIB_RBRACE,
	LIB_COLON,
	LIB_SEMICOLON,
	LIB_COMMA,
	LIB_IDENT,
};

struct lib_lexer {
	char *pos, *end;
	enum lib_token tkn;
	char *tkn_base, *tkn_end;
	int line, column;
	char *text;
	size_t text_cap;
};

struct lib {
	/// The library's name.
	char *name;
	/// The time unit in seconds.
	double time_unit;
	/// The voltage unit in Volt.
	double voltage_unit;
	/// The current unit in Ampère.
	double current_unit;
	/// The capacitance unit in Farad.
	double capacitance_unit;
	/// The leakage power unit in Watt.
	double leakage_power_unit;
	/// The cells in this library.
	array_t cells; /* lib_cell_t* */
	array_t templates; /* struct lib_table_template* */
};

struct lib_cell {
	/// The library that contains the cell.
	lib_t *lib;
	/// The cell's name. Unique within the library that contains the cell.
	char *name;
	/// The leakage power dissipated by this cell.
	double leakage_power;
	/// The cell's pins.
	array_t pins; /* lib_pin_t* */
};

struct lib_pin {
	lib_cell_t *cell;
	/// The pin's name.
	char *name;
	/// The pin's direction.
	uint8_t direction;
	/// The pin's capacitance.
	double capacitance;
	/// The timing groups specified for this pin.
	array_t timings; /* lib_timing_t* */
};

struct lib_timing {
	/// The pin this timing is associated with.
	lib_pin_t *pin;
	uint16_t timing_type;
	uint16_t timing_sense;
	array_t related_pins; /* char* */
	double scalars[LIB_MODEL_NUM_PARAMS];
	lib_table_t *tables[LIB_MODEL_NUM_PARAMS];
};

struct lib_table_format {
	uint16_t variables[3];
	unsigned num_indices[3];
	double *indices[3];
};

struct lib_table {
	lib_timing_t *tmg;
	lib_table_format_t fmt;
	unsigned strides[3];
	unsigned num_values;
	double *values;
};


void lib_lexer_init(lib_lexer_t *lex, void *ptr, size_t len);
void lib_lexer_dispose(lib_lexer_t *lex);
int lib_lexer_next(lib_lexer_t *lex);

int lib_parse(lib_lexer_t *lex, lib_t **lib);

void lib_table_format_init(lib_table_format_t*);
void lib_table_format_copy(lib_table_format_t*, lib_table_format_t*);
void lib_table_format_dispose(lib_table_format_t*);
