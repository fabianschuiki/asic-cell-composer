/* Copyright (c) 2016 Fabian Schuiki */
#pragma once
#include <gds.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <math.h>

#define ASIZE(a) (sizeof(a)/sizeof(*a))

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef enum phx_orientation phx_orientation_t;
typedef enum phx_table_quantity phx_table_quantity_t;
typedef enum phx_timing_type phx_timing_type_t;
typedef struct mat3 mat3_t;
typedef struct phx_cell phx_cell_t;
typedef struct phx_extents phx_extents_t;
typedef struct phx_geometry phx_geometry_t;
typedef struct phx_inst phx_inst_t;
typedef struct phx_layer phx_layer_t;
typedef struct phx_library phx_library_t;
typedef struct phx_line phx_line_t;
typedef struct phx_net phx_net_t;
typedef struct phx_pin phx_pin_t;
typedef struct phx_pin_timing phx_pin_timing_t;
typedef struct phx_shape phx_shape_t;
typedef struct phx_table phx_table_t;
typedef struct phx_table_axis phx_table_axis_t;
typedef struct phx_table_format phx_table_format_t;
typedef struct phx_table_lerp phx_table_lerp_t;
typedef struct phx_table_fix phx_table_fix_t;
typedef struct phx_tech phx_tech_t;
typedef struct phx_tech_layer phx_tech_layer_t;
typedef struct phx_terminal phx_terminal_t;
typedef struct phx_timing_arc phx_timing_arc_t;
typedef struct vec2 vec2_t;
typedef union phx_table_index phx_table_index_t;


enum {
	PHALANX_OK = 0,
	PHALANX_ERR_LEF_SYNTAX,
};

struct vec2 {
	double x;
	double y;
};

struct mat3 {
	double v[3][3];
};

#define VEC2(x,y) ((vec2_t){(x),(y)})

vec2_t vec2_add(vec2_t a, vec2_t b);
vec2_t vec2_sub(vec2_t a, vec2_t b);
vec2_t vec2_mul(vec2_t v, double k);
vec2_t vec2_div(vec2_t v, double k);

mat3_t mat3_scale(double k);
vec2_t mat3_mul_vec2(mat3_t m, vec2_t v);


const char *errstr(int err);
