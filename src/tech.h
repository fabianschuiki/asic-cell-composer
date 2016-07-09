/* Copyright (c) 2016 Fabian Schuiki */
#pragma once
#include "common.h"
#include "util.h"


struct phx_tech {
	ptrset_t layers; /* phx_tech_layer_t* */
};

struct phx_tech_layer {
	/// The technology this layer belongs to.
	phx_tech_t *tech;
	/// The layer's name.
	char *name;
	/// The layer ID used in GDS files.
	uint32_t id;
	/// The layer's color when plotting.
	double color[3];
};


phx_tech_t *phx_tech_create();
void phx_tech_destroy(phx_tech_t*);
phx_tech_layer_t *phx_tech_find_layer_id(phx_tech_t*, uint32_t, bool);
phx_tech_layer_t *phx_tech_find_layer_name(phx_tech_t*, const char*, bool);

phx_tech_layer_t *phx_tech_layer_create(phx_tech_t*);
void phx_tech_layer_destroy(phx_tech_layer_t*);
void phx_tech_layer_set_id(phx_tech_layer_t*, uint32_t);
void phx_tech_layer_set_name(phx_tech_layer_t*, const char*);
uint32_t phx_tech_layer_get_id(phx_tech_layer_t*);
const char *phx_tech_layer_get_name(phx_tech_layer_t*);
