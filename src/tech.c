/* Copyright (c) 2016 Fabian Schuiki */
#include "tech.h"


phx_tech_t *
phx_tech_create() {
	phx_tech_t *tech = calloc(1, sizeof(*tech));
	ptrset_init(&tech->layers);
	return tech;
}


void
phx_tech_destroy(phx_tech_t *tech) {
	assert(tech);
	ptrset_dispose(&tech->layers);
	free(tech);
}


phx_tech_layer_t *
phx_tech_find_layer_id(phx_tech_t *tech, uint32_t id, bool create) {
	phx_tech_layer_t *layer;
	assert(tech);
	for (size_t z = 0; z < tech->layers.size; ++z) {
		layer = tech->layers.items[z];
		if (layer->id == id)
			return layer;
	}
	if (create) {
		layer = phx_tech_layer_create(tech);
		layer->id = id;
		return layer;
	} else {
		return NULL;
	}
}


phx_tech_layer_t *
phx_tech_find_layer_name(phx_tech_t *tech, const char *name, bool create) {
	phx_tech_layer_t *layer;
	assert(tech);
	for (size_t z = 0; z < tech->layers.size; ++z) {
		layer = tech->layers.items[z];
		if (strcmp(layer->name, name) == 0)
			return layer;
	}
	if (create) {
		layer = phx_tech_layer_create(tech);
		layer->name = dupstr(name);
		return layer;
	} else {
		return NULL;
	}
}


phx_tech_layer_t *
phx_tech_layer_create(phx_tech_t *tech) {
	assert(tech);
	phx_tech_layer_t *layer = calloc(1, sizeof(*layer));
	layer->tech = tech;
	ptrset_add(&tech->layers, layer);
	return layer;
}


void
phx_tech_layer_destroy(phx_tech_layer_t *layer) {
	assert(layer);
	ptrset_remove(&layer->tech->layers, layer);
	if (layer->name) free(layer->name);
	free(layer);
}


void
phx_tech_layer_set_id(phx_tech_layer_t *layer, uint32_t id) {
	assert(layer);
	layer->id = id;
}


void
phx_tech_layer_set_name(phx_tech_layer_t *layer, const char *name) {
	assert(layer && name);
	if (layer->name)
		free(layer->name);
	layer->name = dupstr(name);
}


uint32_t
phx_tech_layer_get_id(phx_tech_layer_t *layer) {
	assert(layer);
	return layer->id;
}


const char *
phx_tech_layer_get_name(phx_tech_layer_t *layer) {
	assert(layer);
	return layer->name;
}
