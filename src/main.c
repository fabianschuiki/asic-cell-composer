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
#include <ctype.h>


typedef struct phx_lexer phx_lexer_t;
typedef struct phx_context phx_context_t;

enum phx_token {
	PHX_EOF = 0,
	PHX_LBRACE,
	PHX_RBRACE,
	PHX_SEMICOLON,
	PHX_PERIOD,
	PHX_IDENT,
};

struct phx_lexer {
	FILE *file;
	int tkn;
	int c;
	size_t text_cap;
	size_t text_size;
	char *text;
};

struct phx_context {
	phx_library_t *lib;
	phx_cell_t *cell;
	phx_pin_t *pin;
	phx_inst_t *inst;
	gds_lib_t *gds;
	phx_geometry_t *geometry;
	phx_layer_t *layer;
};

static void phx_lexer_init(phx_lexer_t *lex, FILE *file);
static void phx_lexer_dispose(phx_lexer_t *lex);
static void phx_lexer_next(phx_lexer_t *lex);
static void parse(phx_lexer_t *lex, const phx_context_t *ctx);

static void
phx_lexer_init(phx_lexer_t *lex, FILE *file) {
	assert(lex && file);
	memset(lex, 0, sizeof(*lex));
	lex->file = file;
	lex->text_cap = 128;
	lex->text = malloc(lex->text_cap);
	lex->text[lex->text_size] = 0;
	lex->c = fgetc(lex->file);
	phx_lexer_next(lex);
}

static void
phx_lexer_dispose(phx_lexer_t *lex) {
	assert(lex);
	free(lex->text);
}

static void
phx_push_char(phx_lexer_t *lex, int c) {
	assert(lex);
	if (lex->text_size == lex->text_cap) {
		lex->text_cap *= 2;
		lex->text = realloc(lex->text, lex->text_cap);
	}
	lex->text[lex->text_size] = c;
	++lex->text_size;
}

static void
phx_lexer_next(phx_lexer_t *lex) {
	assert(lex);

relex:
	if (feof(lex->file)) {
		lex->tkn = PHX_EOF;
		return;
	}

	// Skip whitespace.
	while (lex->c && isspace(lex->c))
		lex->c = fgetc(lex->file);

	// Skip comments.
	if (lex->c == '#') {
		while (lex->c && lex->c != '\n')
			lex->c = fgetc(lex->file);
		goto relex;
	}

	// Clear the text buffer.
	lex->tkn = PHX_EOF;
	lex->text[0] = 0;
	lex->text_size = 0;

	if (feof(lex->file)) {
		lex->tkn = PHX_EOF;
		return;
	}

	// Symbols
	int tkn = 0;
	switch (lex->c) {
		case '{': tkn = PHX_LBRACE; break;
		case '}': tkn = PHX_RBRACE; break;
		case ';': tkn = PHX_SEMICOLON; break;
	}
	if (tkn) {
		lex->tkn = tkn;
		phx_push_char(lex, lex->c);
		phx_push_char(lex, 0);
		lex->c = fgetc(lex->file);
		return;
	}

	// Strings
	if (lex->c == '"' || lex->c == '\'') {
		int e = lex->c;
		lex->tkn = PHX_IDENT;
		lex->c = fgetc(lex->file);
		while (lex->c && lex->c != e) {
			if (lex->c == '\\')
				lex->c = fgetc(lex->file);
			phx_push_char(lex, lex->c);
			lex->c = fgetc(lex->file);
		}
		lex->c = fgetc(lex->file);
		phx_push_char(lex, 0);
		return;
	}

	// Identifiers
	lex->tkn = PHX_IDENT;
	while (lex->c && !isspace(lex->c) && lex->c != '{' && lex->c != '}' && lex->c != ';') {
		if (lex->c == '\\')
			lex->c = fgetc(lex->file);
		phx_push_char(lex, lex->c);
		lex->c = fgetc(lex->file);
	}
	phx_push_char(lex, 0);
}


static double
require_real(phx_lexer_t *lex) {
	assert(lex);
	if (lex->tkn != PHX_IDENT) {
		fprintf(stderr, "Expected real number\n");
		exit(1);
	}

	errno = 0;
	double v = strtod(lex->text, NULL);
	if (errno != 0) {
		fprintf(stderr, "Invalid real number, %s\n", strerror(errno));
		exit(1);
	}

	phx_lexer_next(lex);
	return v;
}


static int
require_int(phx_lexer_t *lex) {
	assert(lex);
	if (lex->tkn != PHX_IDENT) {
		fprintf(stderr, "Expected integer number\n");
		exit(1);
	}

	errno = 0;
	int v = strtol(lex->text, NULL, 10);
	if (errno != 0) {
		fprintf(stderr, "Invalid integer number, %s\n", strerror(errno));
		exit(1);
	}

	phx_lexer_next(lex);
	return v;
}


static void
require_pin(phx_lexer_t *lex, phx_cell_t *cell, phx_inst_t **inst, phx_pin_t **pin) {
	assert(lex && inst && pin);

	// Split the text up into instance and pin name.
	assert(lex->tkn == PHX_IDENT);
	char *period = strchr(lex->text, '.');
	char *inst_name, *pin_name;
	if (period == NULL) {
		inst_name = NULL;
		pin_name = lex->text;
	} else {
		inst_name = lex->text;
		pin_name = period+1;
		*period = 0;
	}

	// Find the instance.
	if (inst_name) {
		*inst = phx_cell_find_inst(cell, inst_name);
		if (!*inst) {
			fprintf(stderr, "Cell '%s' does not contain an instance '%s'\n", cell->name, inst_name);
			exit(1);
		}
	} else {
		*inst = NULL;
	}

	// Find the pin.
	phx_cell_t *subcell = *inst ? (*inst)->cell : cell;
	*pin = cell_find_pin(subcell, pin_name);
	if (!*pin) {
		fprintf(stderr, "Cell '%s' does not have a pin '%s'\n", subcell->name, pin_name);
		exit(1);
	}
	phx_lexer_next(lex);
}


static void
parse_sub(phx_lexer_t *lex, const phx_context_t *ctx) {
	assert(lex && ctx);
	if (lex->tkn == PHX_LBRACE) {
		phx_lexer_next(lex);
		while (lex->tkn != PHX_RBRACE) {
			if (lex->tkn == PHX_EOF) {
				fprintf(stderr, "Unexpected end of file while looking for closing brace '}'\n");
				exit(1);
			}
			parse(lex, ctx);
		}
		phx_lexer_next(lex);
		return;
	} else if (lex->tkn == PHX_SEMICOLON) {
		return;
	} else {
		fprintf(stderr, "Expected semicolon ';' or statement block '{'\n");
		exit(1);
	}
}


static void
copy_gds(phx_library_t *lib, gds_struct_t *subgds, gds_lib_t *gds) {
	assert(lib && subgds && gds);
	gds_lib_add_struct(gds, subgds);

	for (unsigned u = 0, un = gds_struct_get_num_elems(subgds); u < un; ++u) {
		gds_elem_t *elem = gds_struct_get_elem(subgds, u);
		int kind = gds_elem_get_kind(elem);
		if (kind == GDS_ELEM_SREF || kind == GDS_ELEM_AREF) {
			const char *name = gds_elem_get_sname(elem);
			phx_cell_t *subcell = phx_library_find_cell(lib, name, false);
			if (subcell && subcell->gds)
				copy_gds(lib, subcell->gds, gds);
		}
	}
}


static void
make_gds_for_cell(phx_library_t *lib, phx_cell_t *cell, gds_lib_t *gds) {
	assert(lib && cell && gds);

	gds_struct_t *str = cell_to_gds(cell, gds);
	gds_lib_add_struct(gds, str);
	gds_struct_unref(str);

	for (unsigned u = 0; u < cell->insts.size; ++u) {
		phx_inst_t *inst = array_at(cell->insts, phx_inst_t*, u);

		bool found = false;
		for (unsigned u = 0, un = gds_lib_get_num_structs(gds); u < un; ++u) {
			gds_struct_t *str = gds_lib_get_struct(gds, u);
			if (strcmp(gds_struct_get_name(str), inst->cell->name) == 0) {
				found = true;
				break;
			}
		}

		if (!found) {
			gds_struct_t *subgds = inst->cell->gds;
			if (subgds) {
				copy_gds(lib, subgds, gds);
			} else {
				make_gds_for_cell(lib, inst->cell, gds);
			}
		}
	}
}


static void
parse(phx_lexer_t *lex, const phx_context_t *ctx) {
	assert(lex && ctx);

	// Skip semicolons.
	if (lex->tkn == PHX_SEMICOLON) {
		phx_lexer_next(lex);
		return;
	}

	// Parse statements.
	if (strcmp(lex->text, "load_lef") == 0) {
		assert(ctx->lib);
		phx_lexer_next(lex);
		while (lex->tkn == PHX_IDENT) {
			lef_t *in;
			int res = lef_read(&in, lex->text);
			if (res != PHALANX_OK) {
				fprintf(stderr, "Unable to read LEF file %s: %s\n", lex->text, errstr(res));
				exit(1);
			}
			load_lef(ctx->lib, in, ctx->lib->tech);
			fprintf(stderr, "Loaded %u cells from %s\n", (unsigned)lef_get_num_macros(in), lex->text);
			lef_free(in);
			phx_lexer_next(lex);
		}
	}
	else if (strcmp(lex->text, "load_lib") == 0) {
		assert(ctx->lib);
		phx_lexer_next(lex);
		while (lex->tkn == PHX_IDENT) {
			lib_t *in;
			int res = lib_read(&in, lex->text);
			if (res != LIB_OK) {
				fprintf(stderr, "Unable to read LIB file %s: %s\n", lex->text, lib_errstr(res));
				exit(1);
			}
			if (in) {
				load_lib(ctx->lib, in, ctx->lib->tech);
				fprintf(stderr, "Loaded %u cells from %s\n", (unsigned)lib_get_num_cells(in), lex->text);
				lib_free(in);
			}
			phx_lexer_next(lex);
		}
	}
	else if (strcmp(lex->text, "load_gds") == 0) {
		assert(ctx->lib);
		phx_lexer_next(lex);
		while (lex->tkn == PHX_IDENT) {
			gds_lib_t *in;
			gds_reader_t *rd;
			int res = gds_reader_open_file(&rd, lex->text, 0);
			if (res != GDS_OK) {
				fprintf(stderr, "Unable to open GDS file %s: %s\n", lex->text, gds_errstr(res));
				exit(1);
			}
			res = gds_lib_read(&in, rd);
			if (res != GDS_OK) {
				fprintf(stderr, "Unable to read GDS file %s: %s\n", lex->text, gds_errstr(res));
				exit(1);
			}
			gds_reader_close(rd);
			load_gds(ctx->lib, in, ctx->lib->tech);
			fprintf(stderr, "Loaded %u cells from %s\n", (unsigned)gds_lib_get_num_structs(in), lex->text);
			gds_lib_destroy(in);
			phx_lexer_next(lex);
		}
	}
	else if (strcmp(lex->text, "cell") == 0) {
		assert(ctx->lib);
		phx_lexer_next(lex);
		assert(lex->tkn == PHX_IDENT);
		phx_context_t subctx = *ctx;
		subctx.cell = phx_library_find_cell(ctx->lib, lex->text, true);
		phx_lexer_next(lex);
		parse_sub(lex, &subctx);
		return;
	}
	else if (strcmp(lex->text, "inst") == 0) {
		assert(ctx->lib && ctx->cell);
		phx_lexer_next(lex);
		assert(lex->tkn == PHX_IDENT);
		phx_cell_t *cell = phx_library_find_cell(ctx->lib, lex->text, true);
		if (!cell) {
			fprintf(stderr, "Unable to find cell '%s'\n", lex->text);
			exit(1);
		}
		phx_lexer_next(lex);
		assert(lex->tkn == PHX_IDENT);
		phx_context_t subctx = *ctx;
		subctx.inst = new_inst(ctx->cell, cell, lex->text);
		phx_lexer_next(lex);
		parse_sub(lex, &subctx);
		return;
	}
	else if (strcmp(lex->text, "pin") == 0) {
		assert(ctx->cell);
		phx_lexer_next(lex);
		assert(lex->tkn == PHX_IDENT);
		phx_context_t subctx = *ctx;
		subctx.pin = cell_find_pin(ctx->cell, lex->text);
		phx_lexer_next(lex);
		parse_sub(lex, &subctx);
		return;
	}
	else if (strcmp(lex->text, "geometry") == 0) {
		assert(ctx->cell);
		phx_lexer_next(lex);
		phx_context_t subctx = *ctx;
		subctx.geometry = phx_cell_get_geometry(ctx->cell);
		parse_sub(lex, &subctx);
		return;
	}
	else if (strcmp(lex->text, "layer") == 0) {
		assert(ctx->geometry && ctx->lib->tech);
		phx_lexer_next(lex);
		assert(lex->tkn == PHX_IDENT);
		phx_tech_layer_t *layer = phx_tech_find_layer_name(ctx->lib->tech, lex->text, false);
		if (!layer) {
			fprintf(stderr, "Cannot find layer '%s'\n", lex->text);
			exit(1);
		}
		phx_lexer_next(lex);
		phx_context_t subctx = *ctx;
		subctx.layer = phx_geometry_on_layer(ctx->geometry, layer);
		parse_sub(lex, &subctx);
		return;
	}
	else if (strcmp(lex->text, "gds") == 0) {
		phx_lexer_next(lex);
		assert(lex->tkn == PHX_IDENT);
		phx_context_t subctx = *ctx;
		subctx.gds = gds_lib_create();
		gds_lib_set_name(subctx.gds, lex->text);
		gds_lib_set_version(subctx.gds, GDS_VERSION_6);
		phx_lexer_next(lex);
		parse_sub(lex, &subctx);
		gds_lib_destroy(subctx.gds);
		return;
	}
	else if (strcmp(lex->text, "set_size") == 0) {
		assert(ctx->cell);
		phx_lexer_next(lex);
		double w = require_real(lex);
		double h = require_real(lex);
		phx_cell_set_size(ctx->cell, (vec2_t){w,h});
	}
	else if (strcmp(lex->text, "set_position") == 0) {
		assert(ctx->inst);
		phx_lexer_next(lex);
		double w = require_real(lex);
		double h = require_real(lex);
		inst_set_pos(ctx->inst, (vec2_t){w,h});
	}
	else if (strcmp(lex->text, "set_orientation") == 0) {
		assert(ctx->inst);
		phx_lexer_next(lex);
		unsigned mask = 0;
		while (lex->tkn == PHX_IDENT) {
			if (strcmp(lex->text, "MX") == 0)
				mask |= PHX_MIRROR_X;
			else if (strcmp(lex->text, "MY") == 0)
				mask |= PHX_MIRROR_Y;
			else if (strcmp(lex->text, "R90") == 0)
				mask |= PHX_ROTATE_90;
			else if (strcmp(lex->text, "R180") == 0)
				mask |= PHX_ROTATE_180;
			else if (strcmp(lex->text, "R270") == 0)
				mask |= PHX_ROTATE_270;
			else {
				fprintf(stderr, "Unknown orientation flag '%s'\n", lex->text);
				exit(1);
			}
			phx_lexer_next(lex);
		}
		phx_inst_set_orientation(ctx->inst, mask);
	}
	else if (strcmp(lex->text, "rect") == 0) {
		assert(ctx->layer);
		phx_lexer_next(lex);
		double x0 = require_real(lex),
		       y0 = require_real(lex),
		       x1 = require_real(lex),
		       y1 = require_real(lex);
		phx_layer_add_shape(ctx->layer, 4, (vec2_t[]){
			{x0,y0},
			{x1,y0},
			{x1,y1},
			{x0,y1},
		});
	}
	else if (strcmp(lex->text, "add_gds_text") == 0) {
		assert(ctx->cell);
		phx_lexer_next(lex);
		int layer = require_int(lex);
		int type = require_int(lex);
		double w = require_real(lex);
		double h = require_real(lex);
		assert(lex->tkn == PHX_IDENT);
		phx_cell_add_gds_text(ctx->cell, layer, type, (vec2_t){w,h}, lex->text);
		phx_lexer_next(lex);
	}

	else if (strcmp(lex->text, "plot_to_pdf") == 0) {
		assert(ctx->cell);
		phx_lexer_next(lex);
		assert(lex->tkn == PHX_IDENT);
		phx_cell_update(ctx->cell, PHX_ALL_BITS);
		plot_cell_as_pdf(ctx->cell, lex->text);
		phx_lexer_next(lex);
	}

	else if (strcmp(lex->text, "copy_pin_geometry") == 0) {
		assert(ctx->cell);
		phx_lexer_next(lex);

		// Find the source pin.
		phx_inst_t *src_inst;
		phx_pin_t *src_pin;
		require_pin(lex, ctx->cell, &src_inst, &src_pin);
		if (!src_inst) {
			fprintf(stderr, "Can only copy geometry from instance pin\n");
			exit(1);
		}

		// Find the destination pin.
		phx_inst_t *dst_inst;
		phx_pin_t *dst_pin;
		require_pin(lex, ctx->cell, &dst_inst, &dst_pin);
		if (dst_inst) {
			fprintf(stderr, "Can only copy geometry to cell pin\n");
			exit(1);
		}

		// Copy over geoemtry.
		phx_inst_copy_geometry_to_parent(src_inst, &src_pin->geo, &dst_pin->geo);
	}

	else if (strcmp(lex->text, "connect") == 0) {
		assert(ctx->cell);
		phx_lexer_next(lex);
		phx_inst_t *src_inst, *dst_inst;
		phx_pin_t *src_pin, *dst_pin;
		require_pin(lex, ctx->cell, &src_inst, &src_pin);
		while (lex->tkn == PHX_IDENT) {
			require_pin(lex, ctx->cell, &dst_inst, &dst_pin);
			connect(ctx->cell, src_pin, src_inst, dst_pin, dst_inst);
		}
	}

	else if (strcmp(lex->text, "copy_cell_gds") == 0) {
		assert(ctx->gds && ctx->lib);
		phx_lexer_next(lex);
		assert(lex->tkn == PHX_IDENT);
		phx_cell_t *cell = phx_library_find_cell(ctx->lib, lex->text, false);
		if (!cell) {
			fprintf(stderr, "Unknown cell '%s'\n", lex->text);
			exit(1);
		}
		phx_lexer_next(lex);
		gds_struct_t *gds = phx_cell_get_gds(cell);
		if (!gds) {
			fprintf(stderr, "Cell '%s' has no associated GDS data\n", cell->name);
			exit(1);
		}
		copy_gds(ctx->lib, gds, ctx->gds);
		// gds_lib_add_struct(ctx->gds, gds);
	}

	else if (strcmp(lex->text, "make_gds_for_cell") == 0) {
		assert(ctx->gds && ctx->lib);
		phx_lexer_next(lex);
		assert(lex->tkn == PHX_IDENT);
		phx_cell_t *cell = phx_library_find_cell(ctx->lib, lex->text, false);
		if (!cell) {
			fprintf(stderr, "Unknown cell '%s'\n", lex->text);
			exit(1);
		}
		phx_lexer_next(lex);
		make_gds_for_cell(ctx->lib, cell, ctx->gds);
		// gds_struct_t *str = cell_to_gds(cell, ctx->gds);
		// gds_lib_add_struct(ctx->gds, str);
		// gds_struct_unref(str);
	}

	else if (strcmp(lex->text, "write_gds") == 0) {
		assert(ctx->gds);
		phx_lexer_next(lex);
		assert(lex->tkn == PHX_IDENT);
		gds_writer_t *wr;
		gds_writer_open_file(&wr, lex->text, 0);
		gds_lib_write(ctx->gds, wr);
		gds_writer_close(wr);
		phx_lexer_next(lex);
	}

	else {
		fprintf(stderr, "Unknown command '%s'\n", lex->text);
		exit(1);
	}

	// Skip trailing semicolon.
	if (lex->tkn != PHX_SEMICOLON) {
		fprintf(stderr, "Expected ';' semicolon after command\n");
		exit(1);
	}
	phx_lexer_next(lex);
}


int
main(int argc, char **argv) {

	// Create a technology library for UMC65.
	phx_tech_t *tech = phx_tech_create();
	load_tech_layer_map(tech, "/home/msc16f2/umc65/encounter/tech/streamOut_noObs.map");

	// Create a new library into which cells shall be laoded.
	phx_library_t *lib = phx_library_create(tech);

	phx_lexer_t lex;
	phx_lexer_init(&lex, stdin);
	phx_context_t ctx;
	memset(&ctx, 0, sizeof(ctx));
	ctx.lib = lib;
	while (lex.tkn != PHX_EOF) {
		parse(&lex, &ctx);
	}
	phx_lexer_dispose(&lex);

	// Clean up.
	phx_library_destroy(lib);
	return 0;
}
