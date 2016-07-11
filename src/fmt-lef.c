/* Copyright (c) 2016 Fabian Schuiki */
#include "common.h"
#include "lef.h"
#include "cell.h"
#include "tech.h"


/**
 * @file
 * @author Fabian Schuiki <fschuiki@student.ethz.ch>
 *
 * This file implements the conversion from and to LEF files.
 */


static void
make_lef_geo(phx_geometry_t *geo, void (*commit)(void*, lef_geo_t*), void *arg) {
	assert(geo && commit);

	for (unsigned u = 0, un = phx_geometry_get_num_layers(geo); u < un; ++u) {
		phx_layer_t *layer = phx_geometry_get_layer(geo, u);
		const char *layer_name = phx_tech_layer_get_name(phx_layer_get_tech(layer));

		/// @todo Detect regularities among the lines and shapes and convert
		/// them to step patterns to simplify the LEF output.

		// Lines
		for (unsigned u = 0, un = phx_layer_get_num_lines(layer); u < un; ++u) {
			phx_line_t *line = phx_layer_get_line(layer, u);
			assert(0 && "not implemented");
			/// @todo Implement lines.
		}

		// Shapes
		lef_geo_layer_t *dst_layer = lef_new_geo_layer(layer_name);
		for (unsigned u = 0, un = phx_layer_get_num_shapes(layer); u < un; ++u) {
			phx_shape_t *shape = phx_layer_get_shape(layer, u);

			bool is_rect = false;
			if (shape->num_pts == 4) {
				unsigned i = (shape->pts[0].x != shape->pts[1].x ? 1 : 0);
				is_rect = (
					fabs(shape->pts[(i+0)%4].x - shape->pts[(i+1)%4].x) < 1e-10 &&
					fabs(shape->pts[(i+1)%4].y - shape->pts[(i+2)%4].y) < 1e-10 &&
					fabs(shape->pts[(i+2)%4].x - shape->pts[(i+3)%4].x) < 1e-10 &&
					fabs(shape->pts[(i+3)%4].y - shape->pts[(i+4)%4].y) < 1e-10
				);
			}

			lef_geo_shape_t *dst_shape;
			if (is_rect) {
				dst_shape = lef_new_geo_shape(LEF_SHAPE_RECT, 2, (lef_xy_t[]){
					{ shape->pts[0].x, shape->pts[0].y },
					{ shape->pts[2].x, shape->pts[2].y },
				});
			} else {
				dst_shape = lef_new_geo_shape(LEF_SHAPE_POLYGON, shape->num_pts, (void*)shape->pts);
			}
			lef_geo_layer_add_shape(dst_layer, dst_shape);
		}
		commit(arg, (lef_geo_t*)dst_layer);
	}
}


lef_macro_t *
phx_make_lef_macro_from_cell(phx_cell_t *cell) {
	lef_macro_t *macro = lef_new_macro(phx_cell_get_name(cell));
	vec2_t v;

	v = phx_cell_get_size(cell);
	lef_macro_set_size(macro, (lef_xy_t){v.x,v.y});
	v = phx_cell_get_origin(cell);
	lef_macro_set_origin(macro, (lef_xy_t){v.x,v.y});

	// Pins
	for (unsigned u = 0, un = phx_cell_get_num_pins(cell); u < un; ++u) {
		phx_pin_t *src_pin = phx_cell_get_pin(cell, u);
		lef_pin_t *dst_pin = lef_new_pin(phx_pin_get_name(src_pin));
		lef_port_t *port = lef_new_port();
		make_lef_geo(phx_pin_get_geometry(src_pin), (void*)lef_port_add_geometry, port);
		lef_pin_add_port(dst_pin, port);
		lef_macro_add_pin(macro, dst_pin);
	}

	return macro;
}
