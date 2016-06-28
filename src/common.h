/* Copyright (c) 2016 Fabian Schuiki */
#pragma once
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <math.h>

#define ASIZE(a) (sizeof(a)/sizeof(*a))
#define M_PI 3.14159265358979323846

typedef struct library library_t;
typedef struct cell cell_t;
typedef struct pin pin_t;
typedef struct geometry geometry_t;
typedef struct layer layer_t;
typedef struct shape shape_t;
typedef struct inst inst_t;
typedef struct vec2 vec2_t;
typedef struct mat3 mat3_t;
typedef struct extents extents_t;
typedef struct tech tech_t;
typedef struct tech_layer tech_layer_t;
typedef struct net net_t;
typedef struct net_conn net_conn_t;


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

int read_lib_file(const char *path);

