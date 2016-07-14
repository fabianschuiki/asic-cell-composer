/* Copyright (c) 2016 Fabian Schuiki */
#include "common.h"
#include "cell.h"
#include "lef.h"
#include "lib.h"
#include "table.h"
#include "tech.h"
#include "misc.h"
#include <math.h>
#include <cairo.h>
#include <cairo-pdf.h>
#include <gds.h>


lef_macro_t *phx_make_lef_macro_from_cell(phx_cell_t*);
void phx_make_lib_cell(phx_cell_t*, lib_cell_t*);


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
			res = lef_read(&in, arg);
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
			res = lib_read(&in, arg);
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

	// dump_timing_arcs(phx_library_find_cell(lib, "BS1", false));
	// dump_timing_arcs(phx_library_find_cell(lib, "ND2M0R", false));

	// Assemble the bit slice cells.
	gds_lib_t *gds_lib = gds_lib_create();
	gds_lib_set_name(gds_lib, "debug");
	gds_lib_set_version(gds_lib, GDS_VERSION_6);

	gds_lib_add_struct(gds_lib, phx_cell_get_gds(phx_library_find_cell(lib, "BS1", false)));
	gds_lib_add_struct(gds_lib, phx_cell_get_gds(phx_library_find_cell(lib, "ND2M0R", false)));
	gds_lib_add_struct(gds_lib, phx_cell_get_gds(phx_library_find_cell(lib, "NR2M0R", false)));

	lef_t *lef = lef_new();
	lib_t *out_lib = lib_new("debug");

	phx_tech_layer_t *L_ME1 = phx_tech_find_layer_name(tech, "ME1", true);
	phx_tech_layer_t *L_ME2 = phx_tech_find_layer_name(tech, "ME2", true);

	for (unsigned u = 1; u <= 2; ++u) {
		unsigned N = 1 << u;
		unsigned Nh = 1 << (u-1);
		char name[32];
		snprintf(name, sizeof(name), "BS%u", N);
		printf("Assembling %s\n", name);

		phx_cell_t *cell = new_cell(lib, name);
		phx_cell_set_size(cell, (vec2_t){3e-6, N*1.8e-6});

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

			const char *pins[] = {"GP", "GN", "S"};
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

		// Add the data pin.
		phx_pin_t *pD = cell_find_pin(cell, "D"),
		          *pInnerD = cell_find_pin(inner, "D"),
		          *pInnerQ = cell_find_pin(inner, "Q");
		connect(cell, pD, NULL, pInnerD, i0);
		connect(cell, pD, NULL, pInnerD, i1);
		phx_layer_add_shape(phx_geometry_on_layer(phx_pin_get_geometry(pD), L_ME2), 4, (vec2_t[]){
			{ 0.05e-6, 0.75e-6 },
			{ 0.15e-6, 0.75e-6 },
			{ 0.15e-6, N*1.8e-6 - 0.75e-6 },
			{ 0.05e-6, N*1.8e-6 - 0.75e-6 },
		});

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

		// Connect the mux pins.
		phx_pin_t *pQ = cell_find_pin(cell, "Q"),
		          *pMuxA = cell_find_pin(cmux, "A"),
		          *pMuxB = cell_find_pin(cmux, "B"),
		          *pMuxZ = cell_find_pin(cmux, "Z");
		connect(cell, pMuxA, imux, pInnerQ, i0);
		connect(cell, pMuxB, imux, pInnerQ, i1);
		connect(cell, pQ, NULL, pMuxZ, imux);
		phx_inst_copy_geometry_to_parent(imux, &pMuxZ->geo, &pQ->geo);

		// Internal routing.
		if (u == 1) {
			// Connect I1.Q to I2.B
			umc65_route(cell, tech, (vec2_t){2.7e-6, 0.7e-6}, 1, 1, 3, (struct route_segment[]){
				{ROUTE_Y, 1.4e-6, 2},
				{ROUTE_X, 2.1e-6, 3},
				{ROUTE_Y, 2.8e-6, 2},
			});

			// Connect I0.Q to I2.A
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

			// Connect I0.Q to I2.B
			umc65_route(cell, tech, p_src, 1, 1, 6, (struct route_segment[]){
				{ROUTE_Y, p_src.y + 0.1e-6, 1},
				{ROUTE_Y, y_src + 0.4e-6, 2},
				{ROUTE_X, channel, 3},
				{ROUTE_Y, y_dst + 0.2e-6, 2},
				{ROUTE_X, p_dst_a.x, 3},
				{ROUTE_Y, p_dst_a.y, 2},
			});

			// Connect I1.Q to I2.A
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
		// cell_update_capacitances(cell);
		// cell_update_timing_arcs(cell);
		// cell_update_extents(cell);
		phx_cell_update(cell, PHX_ALL_BITS);
		dump_cell_nets(cell, stdout);

		// dump_timing_arcs(cell);
		char path[128];
		snprintf(path, sizeof(path), "debug_%s.pdf", name);
		plot_cell_as_pdf(cell, path);

		// Add the cell to the GDS lib.
		gds_struct_t *str = cell_to_gds(cell, gds_lib);
		gds_lib_add_struct(gds_lib, str);
		gds_struct_unref(str);

		// Add the cell to the LEF file.
		lef_macro_t *macro = phx_make_lef_macro_from_cell(cell);
		lef_add_macro(lef, macro);

		// Add the cell to the LIB file.
		lib_cell_t *lib_cell;
		lib_add_cell(out_lib, name, &lib_cell);
		phx_make_lib_cell(cell, lib_cell);
	}

	// Write the GDS file.
	gds_writer_t *wr;
	gds_writer_open_file(&wr, "debug.gds", 0);
	gds_lib_write(gds_lib, wr);
	gds_writer_close(wr);
	gds_lib_destroy(gds_lib);

	// Write the LEF file.
	lef_write(lef, "debug.lef");
	lef_free(lef);

	// Write the LIB file.
	lib_write(out_lib, "debug.lib");
	lib_free(out_lib);

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

	// phx_pin_t *pins[] = {AN2M1R_pA, AN2M1R_pB};
	// for (unsigned u = 0; u < 2; ++u) {
	// 	double in_trans[] = {1e-12, 9e-12};
	// 	double out_caps[] = {1e-16, 9e-16};
	// 	int64_t out_edges[] = {PHX_TABLE_FALL, PHX_TABLE_RISE};

	// 	phx_table_t *tbl_delay = phx_table_new(2, (phx_table_quantity_t[]){PHX_TABLE_IN_TRANS, PHX_TABLE_OUT_CAP, PHX_TABLE_OUT_EDGE}, (uint16_t[]){2, 2, 2});
	// 	// phx_table_set_indices(tbl_delay, PHX_TABLE_OUT_EDGE, out_edges);
	// 	phx_table_set_indices(tbl_delay, PHX_TABLE_IN_TRANS, in_trans);
	// 	phx_table_set_indices(tbl_delay, PHX_TABLE_OUT_CAP, out_caps);
	// 	memcpy(tbl_delay->data, (double[]){
	// 		// falling
	// 		100e-12, 110e-12,
	// 		300e-12, 330e-12,
	// 		// rising
	// 		// 200e-12, 220e-12,
	// 		// 600e-12, 660e-12,
	// 	}, tbl_delay->size * sizeof(double));

	// 	phx_table_t *tbl_trans = phx_table_new(2, (phx_table_quantity_t[]){PHX_TABLE_IN_TRANS, PHX_TABLE_OUT_CAP, PHX_TABLE_OUT_EDGE}, (uint16_t[]){2, 2, 2});
	// 	// phx_table_set_indices(tbl_trans, PHX_TABLE_OUT_EDGE, out_edges);
	// 	phx_table_set_indices(tbl_trans, PHX_TABLE_IN_TRANS, in_trans);
	// 	phx_table_set_indices(tbl_trans, PHX_TABLE_OUT_CAP, out_caps);
	// 	memcpy(tbl_trans->data, (double[]){
	// 		// falling
	// 		1e-12, 2e-12,
	// 		4e-12, 9e-12,
	// 		// rising
	// 		// 20e-12, 200e-12,
	// 		// 60e-12, 600e-12,
	// 	}, tbl_trans->size * sizeof(double));

	// 	phx_cell_set_timing_table(AN2M1R, AN2M1R_pZ, pins[u], PHX_TIM_DELAY, tbl_delay);
	// 	phx_cell_set_timing_table(AN2M1R, AN2M1R_pZ, pins[u], PHX_TIM_TRANS, tbl_trans);
	// }

	// Create a new cell.
	phx_cell_t *cell = new_cell(lib, "AND4");
	vec2_t AN2M1R_sz = phx_cell_get_size(AN2M1R);

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
	phx_cell_set_size(cell, p);
	// phx_cell_set_origin(cell, VEC2(-0.2e-6, -0.2e-6));

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

	// cell_update_capacitances(cell);
	// cell_update_timing_arcs(cell);
	// cell_update_extents(cell);
	phx_cell_update(cell, PHX_ALL_BITS);
	dump_cell_nets(cell, stdout);

	plot_cell_as_pdf(AN2M1R, "debug_AN2M1R.pdf");
	plot_cell_as_pdf(cell, "debug.pdf");

	// Clean up.
	phx_library_destroy(lib);
	return 0;
}
