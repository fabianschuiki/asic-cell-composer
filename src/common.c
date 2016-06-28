/* Copyright (c) 2016 Fabian Schuiki */
#include "common.h"


static const char *error_strings[] = {
	[PHALANX_OK] = "OK",
	[PHALANX_ERR_LEF_SYNTAX] = "LEF Syntax Error",
};


const char *
errstr(int err) {
	if (err < 0) {
		return strerror(-err);
	} else if ((size_t)err < ASIZE(error_strings)) {
		return error_strings[err];
	} else {
		return "Unknown error";
	}
}


vec2_t
vec2_add(vec2_t a, vec2_t b) {
	return VEC2(a.x+b.x, a.y+b.y);
}

vec2_t
vec2_sub(vec2_t a, vec2_t b) {
	return VEC2(a.x-b.x, a.y-b.y);
}

vec2_t
vec2_mul(vec2_t v, double k) {
	return VEC2(v.x*k, v.y*k);
}

vec2_t
vec2_div(vec2_t v, double k) {
	return VEC2(v.x/k, v.y/k);
}


mat3_t
mat3_scale(double k) {
	return (mat3_t){{
		{k,0,0},
		{0,k,0},
		{0,0,k},
	}};
}

/**
 * Calculates the matrix-vector product, assuming the third component of v is 1.
 */
vec2_t
mat3_mul_vec2(mat3_t m, vec2_t v) {
	return VEC2(
		m.v[0][0]*v.x + m.v[0][1]*v.y + m.v[0][2],
		m.v[1][0]*v.x + m.v[1][1]*v.y + m.v[1][2]
	);
}
