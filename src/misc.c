/* Copyright (c) 2016 Fabian Schuiki */
#include "misc.h"
#include "cell.h"
#include "table.h"
#include "tech.h"
#include <math.h>
#include <cairo.h>
#include <cairo-pdf.h>
#include <gds.h>


void
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


int
phx_net_connects_to(phx_net_t *net, phx_pin_t *pin, phx_inst_t *inst) {
	assert(net && pin);
	for (size_t z = 0; z < net->conns.size; ++z) {
		phx_terminal_t *conn = array_get(&net->conns, z);
		if (conn->pin == pin && conn->inst == inst)
			return 1;
	}
	return 0;
}

void
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


void
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
	phx_extents_add(&ext, VEC2(0,0));
	phx_extents_add(&ext, phx_cell_get_origin(cell));
	phx_extents_add(&ext, phx_cell_get_size(cell));
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
	vec2_t p_orig = mat3_mul_vec2(M, phx_cell_get_origin(cell));
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
	vec2_t box1 = mat3_mul_vec2(M, phx_cell_get_size(cell));
	cairo_set_source_rgb(cr, 0, 0, 0);
	cairo_set_dash(cr, (double[]){3.0, 2.0}, 2, 0);
	cairo_rectangle(cr, box0.x, box0.y, box1.x-box0.x, box1.y-box0.y);
	cairo_stroke(cr);
	cairo_restore(cr);

	// Draw the cell name.
	cairo_move_to(cr, p0.x+15, p0.y+15);
	cairo_set_source_rgb(cr, 0, 0, 0);
	cairo_show_text(cr, phx_cell_get_name(cell));

	// Draw the instances in the cell.
	cairo_save(cr);
	cairo_set_line_width(cr, 0.5);
	for (size_t z = 0, zn = phx_cell_get_num_insts(cell); z < zn; ++z) {
		phx_inst_t *inst = phx_cell_get_inst(cell, z);
		phx_cell_t *subcell = inst_get_cell(inst);
		vec2_t box0 = mat3_mul_vec2(M, inst_get_pos(inst));
		vec2_t sz = phx_cell_get_size(subcell);
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
		cairo_text_extents(cr, phx_cell_get_name(subcell), &extents);
		cairo_move_to(cr, (box0.x+box1.x-extents.width)/2, (box0.y+box1.y+extents.height)/2);
		cairo_show_text(cr, phx_cell_get_name(subcell));
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


void
load_lef(phx_library_t *into, lef_t *lef, phx_tech_t *tech) {
	for (size_t z = 0, zn = lef_get_num_macros(lef); z < zn; ++z) {
		lef_macro_t *macro = lef_get_macro(lef,z);
		phx_cell_t *cell = phx_library_find_cell(into, lef_macro_get_name(macro), true);
		lef_xy_t xy = lef_macro_get_size(macro);
		phx_cell_set_size(cell, VEC2(xy.x*1e-6, xy.y*1e-6));

		for (size_t y = 0, yn = lef_macro_get_num_pins(macro); y < yn; ++y) {
			lef_pin_t *src_pin = lef_macro_get_pin(macro, y);
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

		// cell_update_extents(cell);
	}
}


static phx_table_t *
load_lib_table(lib_table_t *src_tbl) {
	/// @todo Map LIB indices to table axis, create table, done.

	// Create a new table with the same information as the table in the LIB
	// file.
	unsigned ndim = lib_table_get_num_dims(src_tbl);
	phx_table_quantity_t quantities[ndim];
	uint16_t num_indices[ndim];

	for (unsigned u = 0; u < ndim; ++u) {
		unsigned var = lib_table_get_variable(src_tbl, u);
		switch (var) {
			case LIB_VAR_IN_TRAN: quantities[u] = PHX_TABLE_IN_TRANS; break;
			case LIB_VAR_OUT_CAP_TOTAL: quantities[u] = PHX_TABLE_OUT_CAP; break;
			// Unsupported Axis
			/// @todo Rather than rejecting the entire table, find a way to
			/// eliminate the unknown column and use the rest of the table as
			/// is.
			default: return NULL;
		}
		num_indices[u] = lib_table_get_num_indices(src_tbl, u);
	}

	phx_table_t *dst_tbl = phx_table_new(ndim, quantities, num_indices);
	for (unsigned u = 0; u < ndim; ++u) {
		phx_table_set_indices(dst_tbl, quantities[u], lib_table_get_indices(src_tbl, u));
	}
	memcpy(dst_tbl->data, lib_table_get_values(src_tbl), lib_table_get_num_values(src_tbl) * sizeof(double));
	return dst_tbl;
}


static void
load_lib_timing(phx_pin_t *dst_pin, phx_pin_t *related_pin, lib_timing_t *src_tmg) {

	if (lib_timing_get_type(src_tmg) == (LIB_TMG_TYPE_COMB|LIB_TMG_EDGE_BOTH)) {
		lib_table_t *tbl;
		// printf("Loading timing %s: %s -> %s\n", dst_pin->cell->name, related_pin->name, dst_pin->name);

		if ((tbl = lib_timing_find_table(src_tmg, LIB_MODEL_CELL_RISE))) {
			// printf("  cell_rise\n");
			phx_table_t *dst_tbl = load_lib_table(tbl);
			if (dst_tbl)
				phx_cell_set_timing_table(dst_pin->cell, dst_pin, related_pin, PHX_TIM_DELAY, dst_tbl);
		}

		if ((tbl = lib_timing_find_table(src_tmg, LIB_MODEL_TRANSITION_RISE))) {
			// printf("  rise_transition\n");
			phx_table_t *dst_tbl = load_lib_table(tbl);
			if (dst_tbl)
				phx_cell_set_timing_table(dst_pin->cell, dst_pin, related_pin, PHX_TIM_TRANS, dst_tbl);
		}
	}
}


void
load_lib(phx_library_t *into, lib_t *lib, phx_tech_t *tech) {
	for (unsigned u = 0, un = lib_get_num_cells(lib); u < un; ++u) {
		lib_cell_t *src_cell = lib_get_cell(lib, u);
		const char *cell_name = lib_cell_get_name(src_cell);
		phx_cell_t *dst_cell = phx_library_find_cell(into, cell_name, true);

		dst_cell->leakage_power = lib_cell_get_leakage_power(src_cell);

		for (unsigned u = 0, un = lib_cell_get_num_pins(src_cell); u < un; ++u) {
			lib_pin_t *src_pin = lib_cell_get_pin(src_cell, u);
			const char *pin_name = lib_pin_get_name(src_pin);
			phx_pin_t *dst_pin = cell_find_pin(dst_cell, pin_name);

			dst_pin->capacitance = lib_pin_get_capacitance(src_pin);

			for (unsigned u = 0, un = lib_pin_get_num_timings(src_pin); u < un; ++u) {
				lib_timing_t *src_tmg = lib_pin_get_timing(src_pin, u);
				for (unsigned u = 0, un = lib_timing_get_num_related_pins(src_tmg); u < un; ++u) {
					const char *related_pin_name = lib_timing_get_related_pin(src_tmg, u);
					phx_pin_t *related_pin = cell_find_pin(dst_cell, related_pin_name);
					load_lib_timing(dst_pin, related_pin, src_tmg);
				}
			}
		}
	}
}


void
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

void
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


gds_struct_t *
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

	// Additional GDS text.
	for (unsigned u = 0; u < cell->gds_text.size; ++u) {
		phx_gds_text_t *txt = array_at(cell->gds_text, phx_gds_text_t*, u);
		gds_xy_t xy = {
			.x = txt->pos.x * unit + 0.5,
			.y = txt->pos.y * unit + 0.5,
		};
		gds_elem_t *elem = gds_elem_create_text(txt->layer, txt->type, xy, txt->text);
		gds_struct_add_elem(str, elem);
	}

	for (unsigned u = 0; u < cell->insts.size; ++u) {
		phx_inst_t *inst = array_at(cell->insts, phx_inst_t*, u);
		gds_elem_t *elem = gds_elem_create_sref(inst->cell->name, (gds_xy_t){inst->pos.x*unit+0.5, inst->pos.y*unit+0.5});

		// Apply the transformations.
		gds_strans_t strans = { .mag = 1 };
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

void
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


void
dump_timing_arcs(phx_cell_t *cell) {
	assert(cell);
	printf("%s Timing Arcs:\n", cell->name);

	for (unsigned u = 0; u < cell->arcs.size; ++u) {
		phx_timing_arc_t *arc = array_get(&cell->arcs, u);
		printf("  %s -> %s\n", arc->related_pin->name, arc->pin->name);
		if (arc->delay) {
			printf("    Delay:\n");
			phx_table_dump(arc->delay, stdout);
		}
		if (arc->transition) {
			printf("    Transition:\n");
			phx_table_dump(arc->transition, stdout);
		}
	}
}
