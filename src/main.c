/* Copyright (c) 2016 Fabian Schuiki */
#include "common.h"
#include "cell.h"
#include "lef.h"
#include "lib.h"
#include "table.h"
#include "tech.h"
#include <math.h>
#include <cairo.h>
#include <cairo-pdf.h>
#include <gds.h>


static void
dump_cell_nets(phx_cell_t *cell, FILE *out) {
	for (size_t z = 0; z < cell->nets.size; ++z) {
		phx_net_t *net = array_at(cell->nets, phx_net_t*, z);
		fprintf(out, "net %s (%g F) {", net->name ? net->name : "<anon>", net->capacitance);
		for (size_t z = 0; z < net->conns.size; ++z) {
			phx_terminal_t *conn = array_get(&net->conns, z);
			if (conn->inst) {
				fprintf(out, " %s.%s", conn->inst->name, conn->pin->name);
			} else {
				fprintf(out, " %s", conn->pin->name);
			}
		}
		fprintf(out, " }");
		if (net->is_exposed) fprintf(out, " exposed");
		fprintf(out, "\n");
	}
}


static int
phx_net_connects_to(phx_net_t *net, phx_pin_t *pin, phx_inst_t *inst) {
	assert(net && pin);
	for (size_t z = 0; z < net->conns.size; ++z) {
		phx_terminal_t *conn = array_get(&net->conns, z);
		if (conn->pin == pin && conn->inst == inst)
			return 1;
	}
	return 0;
}

static void
connect(phx_cell_t *cell, phx_pin_t *pin_a, phx_inst_t *inst_a, phx_pin_t *pin_b, phx_inst_t *inst_b) {
	assert(cell && pin_a && pin_b);

	// Find any existing nets that contain these pins. If both pins are
	// connected to the same net already, there's nothing left to do.
	phx_net_t *net_a = NULL, *net_b = NULL;
	for (size_t z = 0; z < cell->nets.size; ++z) {
		phx_net_t *net = array_at(cell->nets, phx_net_t*, z);
		if (phx_net_connects_to(net, pin_a, inst_a)) {
			assert(!net_a);
			net_a = net;
		}
		if (phx_net_connects_to(net, pin_b, inst_b)) {
			assert(!net_b);
			net_b = net;
		}
	}
	if (net_a && net_a == net_b)
		return;

	// There are three cases to handle: 1) Two nets exist and need to be joined,
	// 2) one net exists and needs to have a pin added, or 3) no nets exist and
	// one needs to be created.
	if (!net_a && !net_b) {
		// printf("creating new net\n");
		phx_net_t *net = calloc(1, sizeof(*net));
		net->cell = cell;
		char buffer[128];
		static unsigned count = 1;
		snprintf(buffer, sizeof(buffer), "n%u", count++);
		net->name = dupstr(buffer);
		array_init(&net->conns, sizeof(phx_terminal_t));
		array_init(&net->arcs, sizeof(phx_timing_arc_t));
		phx_terminal_t ca = { .pin = pin_a, .inst = inst_a },
		           cb = { .pin = pin_b, .inst = inst_b };
		array_add(&net->conns, &ca);
		array_add(&net->conns, &cb);
		array_add(&cell->nets, &net);
		if (!inst_a || !inst_b)
			net->is_exposed = 1;
	} else if (net_a && net_b) {
		assert(0 && "not implemented");
	} else {
		if (net_a) {
			phx_terminal_t c = { .pin = pin_b, .inst = inst_b };
			array_add(&net_a->conns, &c);
			if (!inst_b)
				net_a->is_exposed = 1;
		} else {
			phx_terminal_t c = { .pin = pin_a, .inst = inst_a };
			array_add(&net_b->conns, &c);
			if (!inst_a)
				net_b->is_exposed = 1;
		}
	}
}


static void
plot_line(cairo_t *cr, mat3_t M, phx_line_t *line, vec2_t *center) {
	assert(0 && "not implemented");
}


static void
plot_shape(cairo_t *cr, mat3_t M, phx_shape_t *shape, vec2_t *center) {
	vec2_t pt = mat3_mul_vec2(M, shape->pts[0]);
	vec2_t c = pt;
	unsigned n = 1;

	cairo_move_to(cr, pt.x, pt.y);
	for (unsigned u = 1; u < shape->num_pts; ++u) {
		pt = mat3_mul_vec2(M, shape->pts[u]);
		cairo_line_to(cr, pt.x, pt.y);
		c = vec2_add(c, pt);
		++n;
	}
	cairo_close_path(cr);

	c.x /= n;
	c.y /= n;
	if (center)
		*center = c;
}


static void
plot_layer(cairo_t *cr, mat3_t M, phx_layer_t *layer, vec2_t *center) {
	vec2_t c = VEC2(0,0);
	unsigned n = 0;

	for (size_t z = 0, zn = phx_layer_get_num_lines(layer); z < zn; ++z) {
		phx_line_t *line = phx_layer_get_line(layer, z);
		vec2_t tc;
		plot_line(cr, M, line, &tc);
		c = vec2_add(c, tc);
		++n;
	}

	for (size_t z = 0, zn = phx_layer_get_num_shapes(layer); z < zn; ++z) {
		phx_shape_t *shape = phx_layer_get_shape(layer, z);
		vec2_t tc;
		plot_shape(cr, M, shape, &tc);
		c = vec2_add(c, tc);
		++n;
	}

	c.x /= n;
	c.y /= n;
	if (center)
		*center = c;
}


static void
plot_cell_as_pdf(phx_cell_t *cell, const char *filename) {
	cairo_t *cr;
	cairo_surface_t *surface;
	cairo_text_extents_t extents;

	double scale = 1e8;
	double grid = 1e-7;
	double clr_grid_maj = 0.75;
	double clr_grid_min = 0.9;

	// Calculate the extents of the cell and determine a transformation matrix
	// for all metric coordinates.
	phx_extents_t ext = cell->ext;
	extents_add(&ext, VEC2(0,0));
	extents_add(&ext, cell_get_origin(cell));
	extents_add(&ext, cell_get_size(cell));
	vec2_t d0 = ext.min, d1 = ext.max;
	mat3_t M = mat3_scale(scale);
	M.v[1][1] *= -1; // flip along y

	vec2_t p0 = mat3_mul_vec2(M, d0);
	vec2_t p1 = mat3_mul_vec2(M, d1);
	p0.x -= 20;
	p1.x += 20;
	double tmp = p0.y;
	p0.y = p1.y;
	p1.y = tmp;
	p0.y -= 20;
	p1.y += 20;

	// Create a new PDF document that covers the entire cell extent, plus some
	// margin.
	surface = cairo_pdf_surface_create(filename, p1.x-p0.x, p1.y-p0.y);
	cr = cairo_create(surface);
	cairo_translate(cr, -p0.x, -p0.y);

	// Draw the origin lines of the grid.
	cairo_save(cr);
	vec2_t p_orig = mat3_mul_vec2(M, cell_get_origin(cell));
	cairo_move_to(cr, p0.x, 0);
	cairo_line_to(cr, p1.x, 0);
	cairo_move_to(cr, 0, p0.y);
	cairo_line_to(cr, 0, p1.y);
	cairo_new_sub_path(cr);
	cairo_arc(cr, p_orig.x, p_orig.y, 3, 0, 2*M_PI);
	cairo_set_line_width(cr, 1);
	cairo_set_source_rgb(cr, clr_grid_maj, clr_grid_maj, clr_grid_maj);
	cairo_stroke(cr);

	// Draw the grid.
	for (double f = floor(d0.x/grid)*grid; f <= d1.x; f += grid) {
		vec2_t gp0 = mat3_mul_vec2(M, VEC2(f,d0.y));
		vec2_t gp1 = mat3_mul_vec2(M, VEC2(f,d1.y));
		cairo_move_to(cr, gp0.x, gp0.y+5);
		cairo_line_to(cr, gp1.x, gp1.y-5);
	}
	for (double f = floor(d0.y/grid)*grid; f <= d1.y; f += grid) {
		vec2_t gp0 = mat3_mul_vec2(M, VEC2(d0.x,f));
		vec2_t gp1 = mat3_mul_vec2(M, VEC2(d1.x,f));
		cairo_move_to(cr, gp0.x-5, gp0.y);
		cairo_line_to(cr, gp1.x+5, gp1.y);
	}
	cairo_set_line_width(cr, 0.5);
	cairo_set_source_rgb(cr, clr_grid_min, clr_grid_min, clr_grid_min);
	cairo_stroke(cr);

	// Draw the cell origin and size.
	vec2_t box0 = mat3_mul_vec2(M, VEC2(0,0));
	vec2_t box1 = mat3_mul_vec2(M, cell_get_size(cell));
	cairo_set_source_rgb(cr, 0, 0, 0);
	cairo_set_dash(cr, (double[]){3.0, 2.0}, 2, 0);
	cairo_rectangle(cr, box0.x, box0.y, box1.x-box0.x, box1.y-box0.y);
	cairo_stroke(cr);
	cairo_restore(cr);

	// Draw the cell name.
	cairo_move_to(cr, p0.x+15, p0.y+15);
	cairo_set_source_rgb(cr, 0, 0, 0);
	cairo_show_text(cr, cell_get_name(cell));

	// Draw the instances in the cell.
	cairo_save(cr);
	cairo_set_line_width(cr, 0.5);
	for (size_t z = 0, zn = cell_get_num_insts(cell); z < zn; ++z) {
		phx_inst_t *inst = cell_get_inst(cell, z);
		phx_cell_t *subcell = inst_get_cell(inst);
		vec2_t box0 = mat3_mul_vec2(M, inst_get_pos(inst));
		vec2_t sz = cell_get_size(subcell);
		if (inst->orientation & PHX_MIRROR_X) sz.x *= -1;
		if (inst->orientation & PHX_MIRROR_Y) sz.y *= -1;
		if (inst->orientation & PHX_ROTATE_90) {
			double tmp = sz.x;
			sz.x = sz.y;
			sz.y = -tmp;
		}
		vec2_t box1 = mat3_mul_vec2(M, vec2_add(inst_get_pos(inst), sz));
		cairo_set_source_rgb(cr, 0, 0, 1);
		cairo_rectangle(cr, box0.x, box0.y, box1.x-box0.x, box1.y-box0.y);
		cairo_move_to(cr, (box0.x*0.75+box1.x*0.25), box0.y);
		cairo_line_to(cr, box0.x, (box0.y*0.75+box1.y*0.25));
		// cairo_move_to(cr, box0.x, box1.y);
		// cairo_line_to(cr, box1.x, box0.y);
		// cairo_move_to(cr, box0.x, box0.y);
		// cairo_line_to(cr, box1.x, box1.y);
		cairo_text_extents(cr, cell_get_name(subcell), &extents);
		cairo_move_to(cr, (box0.x+box1.x-extents.width)/2, (box0.y+box1.y+extents.height)/2);
		cairo_show_text(cr, cell_get_name(subcell));
		cairo_stroke(cr);
	}
	cairo_restore(cr);

	// Draw the cell geometry.
	cairo_set_line_width(cr, 0.5);
	cairo_save(cr);
	for (size_t z = 0, zn = cell->geo.layers.size; z < zn; ++z) {
		cairo_set_source_rgb(cr, 0.75, 0.75, 0.75);
		plot_layer(cr, M, array_get(&cell->geo.layers, z), NULL);
		cairo_stroke(cr);
	}
	cairo_restore(cr);

	// Draw the cell pins.
	cairo_save(cr);
	for (size_t z = 0, zn = cell->pins.size; z < zn; ++z) {
		phx_pin_t *pin = array_at(cell->pins, phx_pin_t*, z);
		const char *name = pin->name;
		for (size_t z = 0, zn = pin->geo.layers.size; z < zn; ++z) {
			vec2_t c;
			cairo_set_source_rgb(cr, 1, 0, 0);
			plot_layer(cr, M, array_get(&pin->geo.layers, z), &c);
			cairo_stroke(cr);

			cairo_set_source_rgb(cr, 0, 0, 0);
			cairo_text_extents(cr, name, &extents);
			cairo_move_to(cr, c.x-extents.width/2, c.y+extents.height/2);
			cairo_show_text(cr, name);
			cairo_stroke(cr);
		}
	}
	cairo_restore(cr);

	cairo_destroy(cr);
	cairo_surface_destroy(surface);
}


static void
load_lef(phx_library_t *into, lef_t *lef, phx_tech_t *tech) {
	for (size_t z = 0, zn = lef_get_num_macros(lef); z < zn; ++z) {
		lef_macro_t *macro = lef_get_macro(lef,z);
		phx_cell_t *cell = phx_library_find_cell(into, lef_macro_get_name(macro), true);
		lef_xy_t xy = lef_macro_get_size(macro);
		cell_set_size(cell, VEC2(xy.x*1e-6, xy.y*1e-6));

		for (size_t y = 0, yn = lef_macro_get_num_pins(macro); y < yn; ++y) {
			lef_phx_pin_t *src_pin = lef_macro_get_pin(macro, y);
			phx_pin_t *dst_pin = cell_find_pin(cell, lef_pin_get_name(src_pin));
			phx_geometry_t *dst_geo = &dst_pin->geo;

			for (size_t x = 0, xn = lef_pin_get_num_ports(src_pin); x < xn; ++x) {
				lef_port_t *port = lef_pin_get_port(src_pin, x);

				for (size_t w = 0, wn = lef_port_get_num_geos(port); w < wn; ++w) {
					lef_geo_t *geo = lef_port_get_geo(port, w);
					if (geo->kind == LEF_GEO_LAYER) {
						lef_geo_layer_t *src_layer = (void*)geo;
						const char *layer_name = lef_geo_layer_get_name(src_layer);
						phx_tech_layer_t *tech_layer = phx_tech_find_layer_name(tech, layer_name, true);
						phx_layer_t *dst_layer = phx_geometry_on_layer(dst_geo, tech_layer);

						for (size_t v = 0, vn = lef_geo_layer_get_num_shapes(src_layer); v < vn; ++v) {
							lef_geo_shape_t *shape = lef_geo_layer_get_shape(src_layer, v);
							uint32_t num_points = lef_geo_shape_get_num_points(shape);
							lef_xy_t *points = lef_geo_shape_get_points(shape);
							vec2_t scaled[num_points];
							for (unsigned i = 0; i < num_points; ++i) {
								scaled[i].x = points[i].x * 1e-6;
								scaled[i].y = points[i].y * 1e-6;
							}

							/// @todo Use lef_geo_shape_get_kind(shape)
							switch (shape->kind) {
								case LEF_SHAPE_RECT:
									phx_layer_add_shape(dst_layer, 4, (vec2_t[]){
										{ scaled[0].x, scaled[0].y },
										{ scaled[1].x, scaled[0].y },
										{ scaled[1].x, scaled[1].y },
										{ scaled[0].x, scaled[1].y },
									});
									break;
								case LEF_SHAPE_POLYGON:
									phx_layer_add_shape(dst_layer, num_points, scaled);
									break;
								case LEF_SHAPE_PATH:
									/// @todo Use actual width of the path.
									phx_layer_add_line(dst_layer, 0, num_points, scaled);
									break;
							}
							/// @todo Consider the shape's step pattern and replicate the geometry accordingly.
						}
					}
					/// @todo Add support for the VIA geometry.
				}
			}
		}

		cell_update_extents(cell);
	}
}


static void
load_lib(phx_library_t *into, lib_t *lib, phx_tech_t *tech) {
	for (unsigned u = 0, un = lib_get_num_cells(lib); u < un; ++u) {
		lib_phx_cell_t *src_cell = lib_get_cell(lib, u);
		const char *cell_name = lib_cell_get_name(src_cell);
		phx_cell_t *dst_cell = phx_library_find_cell(into, cell_name, true);

		for (unsigned u = 0, un = lib_cell_get_num_pins(src_cell); u < un; ++u) {
			lib_phx_pin_t *src_pin = lib_cell_get_pin(src_cell, u);
			const char *pin_name = lib_pin_get_name(src_pin);
			phx_pin_t *dst_pin = cell_find_pin(dst_cell, pin_name);

			double d = lib_pin_get_capacitance(src_pin);
			dst_pin->capacitance = d * lib_get_capacitance_unit(lib);
		}
	}
}


static void
load_gds(phx_library_t *into, gds_lib_t *lib, phx_tech_t *tech) {
	double unit = gds_lib_get_units(lib).dbu_in_m;

	for (size_t z = 0, zn = gds_lib_get_num_structs(lib); z < zn; ++z) {
		gds_struct_t *str = gds_lib_get_struct(lib, z);
		phx_cell_t *cell = phx_library_find_cell(into, gds_struct_get_name(str), true);
		phx_cell_set_gds(cell, str);

		for (size_t z = 0, zn = gds_struct_get_num_elems(str); z < zn; ++z) {
			gds_elem_t *elem = gds_struct_get_elem(str, z);

			uint16_t layer_id = gds_elem_get_layer(elem);
			uint16_t type_id = gds_elem_get_type(elem);
			gds_xy_t *xy = gds_elem_get_xy(elem);
			uint16_t num_xy = gds_elem_get_num_xy(elem);

			phx_tech_layer_t *tech_layer = phx_tech_find_layer_id(tech, (uint32_t)layer_id << 16 | type_id, true);
			phx_layer_t *layer = phx_geometry_on_layer(&cell->geo, tech_layer);
			vec2_t *pts;
			size_t num_pts = 0;

			switch (gds_elem_get_kind(elem)) {
				case GDS_ELEM_BOUNDARY: {
					num_pts = num_xy - 1;
					phx_shape_t *shape = phx_layer_add_shape(layer, num_pts, NULL);
					pts = shape->pts;
					break;
				}
				case GDS_ELEM_PATH: {
					/// @todo Use the element's width instead of 100nm.
					num_pts = num_xy;
					phx_line_t *line = phx_layer_add_line(layer, 0.1e-6, num_pts, NULL);
					pts = line->pts;
					break;
				}
			}

			// Convert the points to local units.
			for (uint16_t u = 0; u < num_pts; ++u) {
				pts[u].x = xy[u].x * unit;
				pts[u].y = xy[u].y * unit;
			}
		}
	}
}


enum route_dir {
	ROUTE_X,
	ROUTE_Y
};

struct route_segment {
	enum route_dir dir;
	double pos;
	unsigned layer;
};

static void
umc65_via(phx_cell_t *cell, phx_tech_t *tech, vec2_t pos, unsigned from_layer, unsigned to_layer) {

	char layer_name[16];
	snprintf(layer_name, sizeof(layer_name), "VI%u", from_layer < to_layer ? from_layer : to_layer);
	phx_tech_layer_t *tech_layer = phx_tech_find_layer_name(tech, layer_name, true);
	phx_layer_t *layer = phx_geometry_on_layer(&cell->geo, tech_layer);

	phx_layer_add_shape(layer, 4, (vec2_t[]){
		{ pos.x - 0.05e-6, pos.y - 0.05e-6 },
		{ pos.x + 0.05e-6, pos.y - 0.05e-6 },
		{ pos.x + 0.05e-6, pos.y + 0.05e-6 },
		{ pos.x - 0.05e-6, pos.y + 0.05e-6 },
	});

}

static void
umc65_route(phx_cell_t *cell, phx_tech_t *tech, vec2_t start_pos, unsigned start_layer, unsigned end_layer, unsigned num_segments, struct route_segment *segments) {

	unsigned cur_layer = start_layer;
	vec2_t cur_pos = start_pos;

	for (unsigned u = 0; u < num_segments; ++u) {
		struct route_segment *seg = segments+u;
		vec2_t pos_a = cur_pos, pos_b = cur_pos;
		if (seg->dir == ROUTE_X) pos_b.x = seg->pos;
		if (seg->dir == ROUTE_Y) pos_b.y = seg->pos;
		int xdir = pos_a.x < pos_b.x ? 1 : (pos_a.x > pos_b.x ? -1 : 0);
		int ydir = pos_a.y < pos_b.y ? 1 : (pos_a.y > pos_b.y ? -1 : 0);
		cur_pos = pos_b;

		// Shift the start and end positions to account for vias and generate
		// the necessary vias.
		if (cur_layer != seg->layer) {
			umc65_via(cell, tech, pos_a, cur_layer, seg->layer);
			pos_a.x -= xdir * 0.04e-6;
			pos_a.y -= ydir * 0.04e-6;
		}
		unsigned next_layer = (u+1 == num_segments ? end_layer : segments[u+1].layer);
		if (seg->layer != next_layer) {
			if (u+1 == num_segments)
				umc65_via(cell, tech, pos_b, seg->layer, end_layer);
			pos_b.x += xdir * 0.04e-6;
			pos_b.y += ydir * 0.04e-6;
		}

		char layer_name[16];
		snprintf(layer_name, sizeof(layer_name), "ME%u", seg->layer);
		phx_tech_layer_t *tech_layer = phx_tech_find_layer_name(tech, layer_name, true);
		phx_layer_t *layer = phx_geometry_on_layer(&cell->geo, tech_layer);

		vec2_t pos_min = {
			(pos_a.x < pos_b.x ? pos_a.x : pos_b.x) - 0.05e-6,
			(pos_a.y < pos_b.y ? pos_a.y : pos_b.y) - 0.05e-6,
		};
		vec2_t pos_max = {
			(pos_a.x > pos_b.x ? pos_a.x : pos_b.x) + 0.05e-6,
			(pos_a.y > pos_b.y ? pos_a.y : pos_b.y) + 0.05e-6,
		};

		phx_layer_add_shape(layer, 4, (vec2_t[]){
			{ pos_min.x, pos_min.y },
			{ pos_max.x, pos_min.y },
			{ pos_max.x, pos_max.y },
			{ pos_min.x, pos_max.y },
		});

		cur_layer = seg->layer;
	}
}


static gds_struct_t *
cell_to_gds(phx_cell_t *cell, gds_lib_t *target) {
	double unit = 1.0/gds_lib_get_units(target).dbu_in_m;
	gds_struct_t *str = gds_struct_create(cell->name);

	for (unsigned u = 0; u < cell->geo.layers.size; ++u) {
		phx_layer_t *layer = array_get(&cell->geo.layers, u);
		uint16_t layer_id = layer->tech->id >> 16;
		uint16_t type_id  = layer->tech->id & 0xFFFF;

		// Lines
		for (size_t z = 0, zn = phx_layer_get_num_lines(layer); z < zn; ++z) {
			phx_line_t *line = phx_layer_get_line(layer, z);
			gds_xy_t xy[line->num_pts];
			for (uint16_t u = 0; u < line->num_pts; ++u) {
				xy[u].x = line->pts[u].x * unit + 0.5;
				xy[u].y = line->pts[u].y * unit + 0.5;
				// Adding 0.5 ensures the coordinates are rounded to dbu properly.
			}
			gds_elem_t *elem = gds_elem_create_path(layer_id, type_id, line->num_pts, xy);
			gds_struct_add_elem(str, elem);
		}

		// Shapes
		for (size_t z = 0, zn = phx_layer_get_num_shapes(layer); z < zn; ++z) {
			phx_shape_t *shape = phx_layer_get_shape(layer, z);
			gds_xy_t xy[shape->num_pts+1];
			for (uint16_t u = 0; u < shape->num_pts; ++u) {
				xy[u].x = shape->pts[u].x * unit + 0.5;
				xy[u].y = shape->pts[u].y * unit + 0.5;
				// Adding 0.5 ensures the coordinates are rounded to dbu properly.
			}
			xy[shape->num_pts] = xy[0];
			gds_elem_t *elem = gds_elem_create_boundary(layer_id, type_id, shape->num_pts+1, xy);
			gds_struct_add_elem(str, elem);
		}
	}

	for (unsigned u = 0; u < cell->insts.size; ++u) {
		phx_inst_t *inst = array_at(cell->insts, phx_inst_t*, u);
		gds_elem_t *elem = gds_elem_create_sref(inst->cell->name, (gds_xy_t){inst->pos.x*unit+0.5, inst->pos.y*unit+0.5});

		// Apply the transformations.
		gds_strans_t strans = gds_elem_get_strans(elem);
		if (inst->orientation & PHX_MIRROR_Y) {
			strans.flags ^= GDS_STRANS_REFLECTION;
		}
		if (inst->orientation & PHX_MIRROR_X) {
			strans.flags ^= GDS_STRANS_REFLECTION;
			strans.angle += 180;
		}
		if (inst->orientation & PHX_ROTATE_90) {
			strans.angle += 90;
		}
		gds_elem_set_strans(elem, strans);
		gds_struct_add_elem(str, elem);
	}

	return str;
}


static void
skip_whitespace(char **ptr) {
	while (**ptr == ' ' || **ptr == '\t' || **ptr == '\n' || **ptr == '\r')
		++*ptr;
}

static void
load_tech_layer_map(phx_tech_t *tech, const char *filename) {
	assert(tech);

	FILE *f = fopen(filename, "r");
	if (!f) {
		fprintf(stderr, "Unable to open layer map file %s, %s\n", filename, strerror(errno));
		exit(1);
	}

	while (!feof(f)) {
		char line[1024];
		fgets(line, sizeof(line), f);
		char *ptr = line;

		// Skip whitespace and comments.
		skip_whitespace(&ptr);
		if (*ptr == '#')
			continue;

		// Parse line.
		char *start;
		start = ptr;
		while (*ptr > 0x20) ++ptr;
		char *name = dupstrn(start, ptr-start);
		skip_whitespace(&ptr);
		start = ptr;
		while (*ptr > 0x20) ++ptr;
		char *type = dupstrn(start, ptr-start);

		skip_whitespace(&ptr);
		unsigned long int layer_id = strtoul(ptr, &ptr, 10);
		skip_whitespace(&ptr);
		unsigned long int type_id  = strtoul(ptr, &ptr, 10);\

		if (strlen(name) > 0 && layer_id < (1 << 16) && type_id < (1 << 16)) {
			phx_tech_layer_t *layer = phx_tech_find_layer_name(tech, name, true);
			phx_tech_layer_set_id(layer, layer_id << 16 | type_id);
		}

		free(name);
		free(type);
	}
}


int
main(int argc, char **argv) {
	int res;
	int i;

	// Create a technology library for UMC65.
	phx_tech_t *tech = phx_tech_create();
	load_tech_layer_map(tech, "/home/msc16f2/umc65/encounter/tech/streamOut_noObs.map");

	// Create a new library into which cells shall be laoded.
	phx_library_t *lib = phx_library_create(tech);

	for (i = 1; i < argc; i++) {
		const char *arg = argv[i];
		const char *suffix = strrchr(arg, '.');
		if (suffix == arg)
			suffix = NULL;
		else if (suffix)
			++suffix;

		if (strcasecmp(suffix, "lef") == 0) {
			lef_t *in;
			res = read_lef_file(arg, &in);
			if (res != PHALANX_OK) {
				printf("Unable to read LEF file %s: %s\n", arg, errstr(res));
				return 1;
			}
			load_lef(lib, in, tech);
			printf("Loaded %u cells from %s\n", (unsigned)lef_get_num_macros(in), arg);
			lef_free(in);
		}

		else if (strcasecmp(suffix, "lib") == 0) {
			lib_t *in;
			res = lib_read(arg, &in);
			if (res != LIB_OK) {
				printf("Unable to read LIB file %s: %s\n", arg, lib_errstr(res));
				return 1;
			}
			if (in) {
				load_lib(lib, in, tech);
				printf("Loaded %u cells from %s\n", (unsigned)lib_get_num_cells(in), arg);
				lib_free(in);
			}
		}

		else if (strcasecmp(suffix, "gds") == 0) {
			gds_lib_t *in;
			gds_reader_t *rd;
			res = gds_reader_open_file(&rd, arg, 0);
			if (res != GDS_OK) {
				fprintf(stderr, "Unable to open GDS file %s: %s\n", arg, gds_errstr(res));
				return 1;
			}
			res = gds_lib_read(&in, rd);
			if (res != GDS_OK) {
				fprintf(stderr, "Unable to read GDS file %s: %s\n", arg, gds_errstr(res));
				return 1;
			}
			gds_reader_close(rd);
			load_gds(lib, in, tech);
			printf("Loaded %u cells from %s\n", (unsigned)gds_lib_get_num_structs(in), arg);
			gds_lib_destroy(in);
		}
	}

	// Plot the basic cells.
	plot_cell_as_pdf(phx_library_find_cell(lib, "BS1", false), "debug_BS1.pdf");
	plot_cell_as_pdf(phx_library_find_cell(lib, "ND2M0R", false), "debug_ND2M0R.pdf");
	plot_cell_as_pdf(phx_library_find_cell(lib, "NR2M0R", false), "debug_NR2M0R.pdf");

	// Assemble the bit slice cells.
	struct gds_lib *gds_lib = gds_lib_create();
	gds_lib_set_name(gds_lib, "debug");
	gds_lib_set_version(gds_lib, GDS_VERSION_6);

	gds_lib_add_struct(gds_lib, phx_cell_get_gds(phx_library_find_cell(lib, "BS1", false)));
	gds_lib_add_struct(gds_lib, phx_cell_get_gds(phx_library_find_cell(lib, "ND2M0R", false)));
	gds_lib_add_struct(gds_lib, phx_cell_get_gds(phx_library_find_cell(lib, "NR2M0R", false)));

	phx_tech_layer_t *L_ME1 = phx_tech_find_layer_name(tech, "ME1", true);

	for (unsigned u = 1; u <= 8; ++u) {
		unsigned N = 1 << u;
		unsigned Nh = 1 << (u-1);
		char name[32];
		snprintf(name, sizeof(name), "BS%u", N);
		printf("Assembling %s\n", name);

		phx_cell_t *cell = new_cell(lib, name);
		cell_set_size(cell, (vec2_t){3e-6, N*1.8e-6});

		// Instantiate the two inner cells.
		char inner_name[32];
		snprintf(inner_name, sizeof(inner_name), "BS%u", Nh);
		phx_cell_t *inner = phx_library_find_cell(lib, inner_name, false);
		assert(inner);
		phx_inst_t *i0 = new_inst(cell, inner, "I0");
		phx_inst_t *i1 = new_inst(cell, inner, "I1");
		inst_set_pos(i0, (vec2_t){0, 0});
		if (u == 1) {
			inst_set_pos(i1, (vec2_t){0, N*1.8e-6});
			phx_inst_set_orientation(i1, PHX_MIRROR_Y);
		} else {
			inst_set_pos(i1, (vec2_t){0, Nh*1.8e-6});
		}

		// Create the supply pins.
		phx_pin_t *pVDD = cell_find_pin(cell, "VDD");
		phx_pin_t *pVSS = cell_find_pin(cell, "VSS");
		phx_layer_t *geoVDD = phx_geometry_on_layer(&pVDD->geo, L_ME1);
		phx_layer_t *geoVSS = phx_geometry_on_layer(&pVSS->geo, L_ME1);
		for (unsigned u = 0; u < N+1; ++u) {
			double y = u * 1.8e-6;
			vec2_t pa = {0,    y - 0.15e-6};
			vec2_t pb = {3e-6, y + 0.15e-6};
			phx_layer_add_shape(u % 2 == 0 ? geoVSS : geoVDD, 4, (vec2_t[]){
				{ pa.x, pa.y },
				{ pb.x, pa.y },
				{ pb.x, pb.y },
				{ pa.x, pb.y },
			});
		}

		// Copy the input pins of the two inner cells.
		for (unsigned u = 0; u < Nh; ++u) {
			char bus_lo[32];
			char bus_hi[32];
			snprintf(bus_lo, sizeof(bus_lo), "[%u]", u);
			snprintf(bus_hi, sizeof(bus_hi), "[%u]", u + Nh);

			const char *pins[] = {"D", "GP", "GN", "S"};
			for (unsigned u = 0; u < ASIZE(pins); ++u) {
				char buffer[128];

				strcpy(buffer, pins[u]);
				strcat(buffer, bus_lo);
				phx_pin_t *src = cell_find_pin(inner, Nh == 1 ? pins[u] : buffer);
				phx_pin_t *dst0 = cell_find_pin(cell, buffer);
				strcpy(buffer, pins[u]);
				strcat(buffer, bus_hi);
				phx_pin_t *dst1 = cell_find_pin(cell, buffer);
				assert(src && dst0 && dst1);

				phx_inst_copy_geometry_to_parent(i0, &src->geo, &dst0->geo);
				phx_inst_copy_geometry_to_parent(i1, &src->geo, &dst1->geo);

				connect(cell, dst0, NULL, src, i0);
				connect(cell, dst1, NULL, src, i1);
			}
		}

		// Connect the input pins.
		unsigned D_route_layer = (u == 1 ? 1 : 2);
		umc65_route(cell, tech, (vec2_t){0.1e-6, Nh*1.8e-6 - 1e-6}, D_route_layer, D_route_layer, 1, (struct route_segment[]){
			{ROUTE_Y, Nh*1.8e-6 + 1e-6, 2},
		});

		// Instantiate the appropriate cell for the output multiplexer.
		phx_cell_t *cmux = phx_library_find_cell(lib, u % 2 == 0 ? "NR2M0R" : "ND2M0R", false);
		assert(cmux);
		phx_inst_t *imux = new_inst(cell, cmux, "I2");
		if (u == 1) {
			inst_set_pos(imux, (vec2_t){2.2e-6, 0});
		} else {
			inst_set_pos(imux, (vec2_t){2.2e-6, Nh*1.8e-6});
			phx_inst_set_orientation(imux, PHX_MIRROR_Y);
		}

		if (u == 1) {
			// Connect I1.Z to I2.B
			umc65_route(cell, tech, (vec2_t){2.7e-6, 0.7e-6}, 1, 1, 3, (struct route_segment[]){
				{ROUTE_Y, 1.4e-6, 2},
				{ROUTE_X, 2.1e-6, 3},
				{ROUTE_Y, 2.8e-6, 2},
			});

			// Connect I0.Z to I2.A
			umc65_route(cell, tech, (vec2_t){2.2e-6, 0.8e-6}, 1, 1, 1, (struct route_segment[]){
				{ROUTE_X, 2.2e-6, 1},
			});
		} else {
			vec2_t p_src, p_dst_a, p_dst_b;
			p_src = VEC2(2.9e-6, 0.7e-6);
			p_dst_a = VEC2(2.5e-6, 1e-6);
			p_dst_b = VEC2(2.7e-6, 1e-6);

			double y_dst = (Nh-1)*1.8e-6;
			double y_src = u > 2 ? (Nh/2-1)*1.8e-6 : 0;
			int channel_idx = u-2;
			if (channel_idx >= 1) ++channel_idx;
			if (channel_idx >= 3) ++channel_idx;
			if (channel_idx >= 8) ++channel_idx;
			if (channel_idx >= 9) ++channel_idx;
			double channel = 2.3e-6 - channel_idx*0.2e-6;
			p_dst_a.y += y_dst;
			p_dst_b.y += y_dst;
			p_src.y += y_src;

			// Connect I0.Z to I2.B
			umc65_route(cell, tech, p_src, 1, 1, 6, (struct route_segment[]){
				{ROUTE_Y, p_src.y + 0.1e-6, 1},
				{ROUTE_Y, y_src + 0.4e-6, 2},
				{ROUTE_X, channel, 3},
				{ROUTE_Y, y_dst + 0.2e-6, 2},
				{ROUTE_X, p_dst_a.x, 3},
				{ROUTE_Y, p_dst_a.y, 2},
			});

			// Connect I1.Z to I2.A
			p_src.y += Nh*1.8e-6;
			y_src += Nh*1.8e-6;
			umc65_route(cell, tech, p_src, 1, 1, 5, (struct route_segment[]){
				{ROUTE_Y, y_src + 0.4e-6, 2},
				{ROUTE_X, channel, 3},
				{ROUTE_Y, y_dst + 1.4e-6, 2},
				{ROUTE_X, p_dst_b.x, 3},
				{ROUTE_Y, p_dst_b.y, 2},
			});
		}

		// Plot the cell for debugging purposes.
		cell_update_extents(cell);
		cell_update_capacitances(cell);
		cell_update_timing_arcs(cell);

		dump_cell_nets(cell, stdout);
		char path[128];
		snprintf(path, sizeof(path), "debug_%s.pdf", name);
		plot_cell_as_pdf(cell, path);

		// Add the cell to the GDS lib.
		gds_struct_t *str = cell_to_gds(cell, gds_lib);
		gds_lib_add_struct(gds_lib, str);
		gds_struct_unref(str);
	}

	// Dump stuff as GDS.
	struct gds_writer *wr;
	gds_writer_open_file(&wr, "debug.gds", 0);
	gds_lib_write(gds_lib, wr);
	gds_writer_close(wr);
	gds_lib_destroy(gds_lib);

	// Add some dummy timing tables to the AND2M1R cell.
	phx_cell_t *AN2M1R = phx_library_find_cell(lib, "AN2M1R", false);
	if (!AN2M1R) {
		fprintf(stderr, "Cannot find cell AN2M1R\n");
		phx_library_destroy(lib);
		return 1;
	}
	phx_pin_t *AN2M1R_pA = cell_find_pin(AN2M1R, "A");
	phx_pin_t *AN2M1R_pB = cell_find_pin(AN2M1R, "B");
	phx_pin_t *AN2M1R_pZ = cell_find_pin(AN2M1R, "Z");

	phx_pin_t *pins[] = {AN2M1R_pA, AN2M1R_pB};
	for (unsigned u = 0; u < 2; ++u) {
		double in_trans[] = {1e-12, 9e-12};
		double out_caps[] = {1e-16, 9e-16};
		int64_t out_edges[] = {PHX_TABLE_FALL, PHX_TABLE_RISE};

		phx_table_t *tbl_delay = phx_table_new(2, (phx_table_quantity_t[]){PHX_TABLE_IN_TRANS, PHX_TABLE_OUT_CAP, PHX_TABLE_OUT_EDGE}, (uint16_t[]){2, 2, 2});
		// phx_table_set_indices(tbl_delay, PHX_TABLE_OUT_EDGE, out_edges);
		phx_table_set_indices(tbl_delay, PHX_TABLE_IN_TRANS, in_trans);
		phx_table_set_indices(tbl_delay, PHX_TABLE_OUT_CAP, out_caps);
		memcpy(tbl_delay->data, (double[]){
			// falling
			100e-12, 110e-12,
			300e-12, 330e-12,
			// rising
			// 200e-12, 220e-12,
			// 600e-12, 660e-12,
		}, tbl_delay->size * sizeof(double));

		phx_table_t *tbl_trans = phx_table_new(2, (phx_table_quantity_t[]){PHX_TABLE_IN_TRANS, PHX_TABLE_OUT_CAP, PHX_TABLE_OUT_EDGE}, (uint16_t[]){2, 2, 2});
		// phx_table_set_indices(tbl_trans, PHX_TABLE_OUT_EDGE, out_edges);
		phx_table_set_indices(tbl_trans, PHX_TABLE_IN_TRANS, in_trans);
		phx_table_set_indices(tbl_trans, PHX_TABLE_OUT_CAP, out_caps);
		memcpy(tbl_trans->data, (double[]){
			// falling
			1e-12, 2e-12,
			4e-12, 9e-12,
			// rising
			// 20e-12, 200e-12,
			// 60e-12, 600e-12,
		}, tbl_trans->size * sizeof(double));

		phx_cell_set_timing_table(AN2M1R, AN2M1R_pZ, pins[u], PHX_TIM_DELAY, tbl_delay);
		phx_cell_set_timing_table(AN2M1R, AN2M1R_pZ, pins[u], PHX_TIM_TRANS, tbl_trans);
	}

	// Create a new cell.
	phx_cell_t *cell = new_cell(lib, "AND4");
	vec2_t AN2M1R_sz = cell_get_size(AN2M1R);

	phx_inst_t *i0 = new_inst(cell, AN2M1R, "I0");
	phx_inst_t *i1 = new_inst(cell, AN2M1R, "I1");
	phx_inst_t *i2 = new_inst(cell, AN2M1R, "I2");

	// Place the AND gates.
	vec2_t p = {0,0};
	inst_set_pos(i0, p);
	p.x += AN2M1R_sz.x;
	inst_set_pos(i1, p);
	p.x += AN2M1R_sz.x;
	inst_set_pos(i2, p);
	p.x += AN2M1R_sz.x;
	p.y += AN2M1R_sz.y;
	cell_set_size(cell, p);
	// cell_set_origin(cell, VEC2(-0.2e-6, -0.2e-6));

	// Add the pins.
	phx_pin_t *pA   = cell_find_pin(cell, "A"),
	          *pB   = cell_find_pin(cell, "B"),
	          *pC   = cell_find_pin(cell, "C"),
	          *pD   = cell_find_pin(cell, "D"),
	          *pZ   = cell_find_pin(cell, "Z"),
	          *pVDD = cell_find_pin(cell, "VDD"),
	          *pVSS = cell_find_pin(cell, "VSS");

	phx_inst_copy_geometry_to_parent(i0, &AN2M1R_pA->geo, &pA->geo);
	phx_inst_copy_geometry_to_parent(i0, &AN2M1R_pB->geo, &pB->geo);
	phx_inst_copy_geometry_to_parent(i1, &AN2M1R_pA->geo, &pC->geo);
	phx_inst_copy_geometry_to_parent(i1, &AN2M1R_pB->geo, &pD->geo);
	phx_inst_copy_geometry_to_parent(i2, &AN2M1R_pZ->geo, &pZ->geo);

	// layer_add_shape(phx_geometry_find_layer(&pVDD->geo, "ME1"), (vec2_t[]){
	// 	{0, 1.65e-6}, {p.x, 1.95e-6}
	// }, 2);
	// layer_add_shape(phx_geometry_find_layer(&pVSS->geo, "ME1"), (vec2_t[]){
	// 	{0, -0.15e-6}, {p.x, 0.15e-6}
	// }, 2);

	// Add the internal connections of the cell.
	connect(cell, pVDD, NULL, cell_find_pin(AN2M1R, "VDD"), i0);
	connect(cell, pVDD, NULL, cell_find_pin(AN2M1R, "VDD"), i1);
	connect(cell, pVDD, NULL, cell_find_pin(AN2M1R, "VDD"), i2);

	connect(cell, pVSS, NULL, cell_find_pin(AN2M1R, "VSS"), i0);
	connect(cell, pVSS, NULL, cell_find_pin(AN2M1R, "VSS"), i1);
	connect(cell, pVSS, NULL, cell_find_pin(AN2M1R, "VSS"), i2);

	connect(cell, pA, NULL, AN2M1R_pA, i0);
	connect(cell, pB, NULL, AN2M1R_pB, i0);
	connect(cell, pC, NULL, AN2M1R_pA, i1);
	connect(cell, pD, NULL, AN2M1R_pB, i1);
	connect(cell, pZ, NULL, AN2M1R_pZ, i2);
	connect(cell, AN2M1R_pZ, i0, AN2M1R_pA, i2);
	connect(cell, AN2M1R_pZ, i1, AN2M1R_pB, i2);

	cell_update_capacitances(cell);
	cell_update_timing_arcs(cell);
	cell_update_extents(cell);
	dump_cell_nets(cell, stdout);

	plot_cell_as_pdf(AN2M1R, "debug_AN2M1R.pdf");
	plot_cell_as_pdf(cell, "debug.pdf");

	// Clean up.
	phx_library_destroy(lib);
	return 0;
}
