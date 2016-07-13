/* Copyright (c) 2016 Fabian Schuiki */
#pragma once
#include "common.h"

enum phx_table_quantity {
	PHX_TABLE_TYPE      = 0xF0,
	PHX_TABLE_TYPE_REAL = 0x00,
	PHX_TABLE_TYPE_INT  = 0x10,

	PHX_TABLE_IN_TRANS = 0x0 | PHX_TABLE_TYPE_REAL,
	PHX_TABLE_OUT_CAP  = 0x1 | PHX_TABLE_TYPE_REAL,
	PHX_TABLE_OUT_EDGE = 0x2 | PHX_TABLE_TYPE_INT,

	PHX_TABLE_MAX_AXES = 0x3,
};

#define PHX_TABLE_INDEX(axis_id) (axis_id & 0xF)
#define PHX_TABLE_MASK(axis_id) (1 << PHX_TABLE_INDEX(axis_id))

enum phx_table_edge {
	PHX_TABLE_FALL = 0,
	PHX_TABLE_RISE = 1,
};

union phx_table_index {
	double real;
	int64_t integer;
};

struct phx_table_axis {
	uint8_t id;
	phx_table_quantity_t quantity;
	uint8_t index;
	unsigned stride;
	uint16_t num_indices;
	phx_table_index_t *indices;
};

struct phx_table_format {
	unsigned refcount;
	/// A bitmask indicating what axes are present in the table.
	uint8_t axes_set;
	/// The number of axes present in the table. Equivalent to a popcount of the
	/// axes_set field.
	uint8_t num_axes;
	/// A lookup table that maps an axis ID to the corresponding entry in the
	/// axes array.
	int8_t lookup[PHX_TABLE_MAX_AXES];
	/// The number of data values the table contains.
	unsigned num_values;
	/// An array containing num_axes entries that describe each table axis.
	phx_table_axis_t axes[];
};

struct phx_table {
	unsigned refcount;
	/// A structure that contains information on the table's axis and data
	/// layout.
	phx_table_format_t *fmt;
	unsigned size;
	double *data;
	uint8_t num_axes;
	phx_table_axis_t axes[];
};

struct phx_table_lerp {
	uint8_t axis_id;
	phx_table_axis_t *axis;
	uint16_t lower;
	uint16_t upper;
	double f;
};

struct phx_table_fix {
	uint8_t axis_id;
	phx_table_index_t index;
};


/* Table */
phx_table_t *phx_table_create_with_format(phx_table_format_t*);
void phx_table_ref(phx_table_t*);
void phx_table_unref(phx_table_t*);
phx_table_format_t *phx_table_get_format(phx_table_t*);
phx_table_t *phx_table_new(uint8_t num_axes, phx_table_quantity_t *quantities, uint16_t *num_indices);
phx_table_t *phx_table_duplicate(phx_table_t*);
void phx_table_free(phx_table_t*);
void phx_table_dump(phx_table_t*, FILE*);
void phx_table_set_indices(phx_table_t*, phx_table_quantity_t, void*);
void phx_table_lerp_axes(phx_table_t*, uint8_t num_lerps, phx_table_quantity_t *quantities, phx_table_index_t *values, phx_table_lerp_t *out);
void phx_table_add(phx_table_t *Tr, phx_table_t *Ta, phx_table_t *Tb);
void phx_table_copy_values(uint8_t, phx_table_t*, phx_table_t*, unsigned, unsigned, unsigned, phx_table_lerp_t*);

/* Format */
phx_table_format_t *phx_table_format_create(unsigned);
void phx_table_format_ref(phx_table_format_t*);
void phx_table_format_unref(phx_table_format_t*);
void phx_table_format_set_indices(phx_table_format_t*, unsigned, unsigned, phx_table_index_t*);
phx_table_axis_t *phx_table_format_get_axis(phx_table_format_t*, unsigned);
void phx_table_format_set_stride(phx_table_format_t*, unsigned, unsigned);
void phx_table_format_update_strides(phx_table_format_t*);
void phx_table_format_finalize(phx_table_format_t*);
