/* Copyright (c) 2016 Fabian Schuiki */
#pragma once
#include "common.h"
#include "util.h"


typedef struct lef lef_t;
typedef struct lef_xy lef_xy_t;
typedef struct lef_geo lef_geo_t;
typedef struct lef_geo_shape lef_geo_shape_t;
typedef struct lef_geo_layer lef_geo_layer_t;
typedef struct lef_geo_via lef_geo_via_t;
typedef struct lef_macro lef_macro_t;
typedef struct lef_pin lef_phx_pin_t;
typedef struct lef_port lef_port_t;


struct lef_xy {
	double x,y;
};


/**
 * A LEF file.
 */
struct lef {
	char *version;
	array_t sites;
	array_t macros; /* lef_macro_t* */
};


/**
 * A LEF macro.
 */
struct lef_macro {
	char *name;
	struct lef_xy origin;
	struct lef_xy size;
	uint8_t symmetry;
	array_t pins; /* lef_phx_pin_t* */
	array_t obs; /* lef_geo_t* */
};

enum lef_macro_symmetry {
	LEF_MACRO_SYM_X   = 1 << 0,
	LEF_MACRO_SYM_Y   = 1 << 1,
	LEF_MACRO_SYM_R90 = 1 << 2,
};


// Pins
enum lef_pin_direction {
	LEF_PIN_DIR_INPUT,
	LEF_PIN_DIR_OUTPUT,
	LEF_PIN_DIR_TRISTATE,
	LEF_PIN_DIR_INOUT,
	LEF_PIN_DIR_FEEDTHRU,
};

enum lef_pin_use {
	LEF_PIN_USE_SIGNAL,
	LEF_PIN_USE_ANALOG,
	LEF_PIN_USE_POWER,
	LEF_PIN_USE_GROUND,
	LEF_PIN_USE_CLOCK,
};

enum lef_pin_shape {
	LEF_PIN_SHAPE_ABUTMENT,
	LEF_PIN_SHAPE_RING,
	LEF_PIN_SHAPE_FEEDTHRU,
};

struct lef_pin {
	char *name;
	enum lef_pin_direction direction;
	enum lef_pin_use use;
	enum lef_pin_shape shape;
	char *must_join;
	array_t ports; /* lef_port_t* */
};

enum lef_port_class {
	LEF_PORT_CLASS_NONE,
	LEF_PORT_CLASS_CORE,
	LEF_PORT_CLASS_BUMP,
};

struct lef_port {
	enum lef_port_class cls;
	array_t geos; /* lef_geo_t* */
	struct lef_geo_layer *last_layer;
};


// Layer Geoemtries
enum lef_geo_kind {
	LEF_GEO_LAYER,
	LEF_GEO_VIA,
};

struct lef_geo {
	enum lef_geo_kind kind;
};

struct lef_geo_layer {
	struct lef_geo geo;
	char *layer;
	double min_spacing;
	double design_rule_width;
	double width;
	array_t shapes; /* lef_geo_shape_t* */
};

struct lef_geo_via {
	struct lef_geo geo;
	char *name;
	int32_t mask;
	struct lef_xy pos;
	struct lef_geo_iterate *iterate;
};

enum lef_geo_shape_kind {
	LEF_SHAPE_PATH,
	LEF_SHAPE_RECT,
	LEF_SHAPE_POLYGON,
};

struct lef_geo_shape {
	enum lef_geo_shape_kind kind;
	int32_t mask;
	uint16_t num_points;
	struct lef_xy *points;
	struct lef_geo_iterate *iterate;
};


lef_t *lef_new();
void lef_free(lef_t *lef);
size_t lef_get_num_macros(lef_t*);
lef_macro_t *lef_get_macro(lef_t*, size_t idx);
void lef_add_macro(lef_t*, lef_macro_t *macro);

lef_macro_t *lef_new_macro(const char *name);
void lef_free_macro(lef_macro_t*);
const char *lef_macro_get_name(lef_macro_t*);
lef_xy_t lef_macro_get_size(lef_macro_t*);
size_t lef_macro_get_num_pins(lef_macro_t*);
lef_phx_pin_t *lef_macro_get_pin(lef_macro_t*, size_t idx);

struct lef_pin *lef_new_pin(const char *name);
void lef_free_pin(lef_phx_pin_t*);
size_t lef_pin_get_num_ports(lef_phx_pin_t*);
lef_port_t *lef_pin_get_port(lef_phx_pin_t*, size_t idx);
const char *lef_pin_get_name(lef_phx_pin_t*);

lef_port_t *lef_new_port();
void lef_free_port(lef_port_t*);
enum lef_port_class lef_port_get_class(lef_port_t*);
size_t lef_port_get_num_geos(lef_port_t*);
lef_geo_t *lef_port_get_geo(lef_port_t*, size_t idx);

lef_geo_layer_t *lef_new_geo_layer();
void lef_free_geo_layer(lef_geo_layer_t*);
void lef_geo_layer_add_shape(lef_geo_layer_t*, lef_geo_shape_t*);
size_t lef_geo_layer_get_num_shapes(lef_geo_layer_t*);
lef_geo_shape_t *lef_geo_layer_get_shape(lef_geo_layer_t*, size_t idx);
const char *lef_geo_layer_get_name(lef_geo_layer_t*);

lef_geo_shape_t *lef_new_geo_shape(enum lef_geo_shape_kind kind, uint32_t num_points, lef_xy_t *points);
void lef_free_geo_shape(lef_geo_shape_t*);
uint16_t lef_geo_shape_get_num_points(lef_geo_shape_t*);
lef_xy_t *lef_geo_shape_get_points(lef_geo_shape_t*);


int read_lef_file(const char *path, lef_t **out);
