/* Copyright (c) 2016 Fabian Schuiki */
#pragma once
#include "common.h"

enum phx_table_quantity {
	PHX_TABLE_TYPE      = 0xF,
	PHX_TABLE_TYPE_REAL = 0x0,
	PHX_TABLE_TYPE_INT  = 0x1,

	PHX_TABLE_IN_TRANS = 0x10 | PHX_TABLE_TYPE_REAL,
	PHX_TABLE_OUT_CAP  = 0x20 | PHX_TABLE_TYPE_REAL,
	PHX_TABLE_OUT_EDGE = 0x30 | PHX_TABLE_TYPE_INT,
};

enum phx_table_edge {
	PHX_TABLE_FALL = 0,
	PHX_TABLE_RISE = 1,
};

struct phx_table_axis {
	phx_table_quantity_t quantity;
	uint8_t index;
	uint32_t stride;
	uint16_t num_indices;
	union phx_table_index {
		double real;
		int64_t integer;
	} *indices;
};

struct phx_table {
	uint32_t size;
	double *data;
	uint8_t num_axes;
	phx_table_axis_t axes[];
};

struct phx_table_lerp {
	phx_table_axis_t *axis;
	uint16_t lower;
	uint16_t upper;
	double f;
};


phx_table_t *phx_table_new(uint8_t num_axes, phx_table_quantity_t *quantities, uint16_t *num_indices);
phx_table_t *phx_table_duplicate(phx_table_t*);
void phx_table_free(phx_table_t*);
void phx_table_dump(phx_table_t*, FILE*);
void phx_table_set_indices(phx_table_t*, phx_table_quantity_t, void*);
void phx_table_lerp_axes(phx_table_t*, uint8_t num_lerps, phx_table_quantity_t *quantities, phx_table_index_t *values, phx_table_lerp_t *out);
void phx_table_add(phx_table_t *Tr, phx_table_t *Ta, phx_table_t *Tb);
