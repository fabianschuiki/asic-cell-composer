/* Copyright (c) 2016 Fabian Schuiki */
#include "lef-internal.h"


/**
 * @file
 * @author Fabian Schuiki <fschuiki@student.ethz.ch>
 *
 * This file implements writing LEF structures to disk.
 */


static const double unit = 1e-6;


static void
write_xy(lef_xy_t xy, FILE *out) {
	assert(out);
	fprintf(out, "( %f %f )", xy.x / unit, xy.y / unit);
}


static void
write_geos(lef_geo_t **geos, unsigned num_geos, FILE *out, const char *indent) {
	assert((geos || num_geos == 0) && out);
	lef_geo_t *prev = NULL;
	for (unsigned u = 0; u < num_geos; ++u) {
		lef_geo_t *geo = geos[u];

		if (geo->kind == LEF_GEO_LAYER) {
			lef_geo_layer_t *layer = (void*)geo;

			fprintf(out, "%sLAYER %s ", indent, layer->layer);

			// SPACING
			if (layer->min_spacing != 0) {
				fprintf(out, "SPACING %f ", layer->min_spacing/unit);
			}

			// DESIGNRULEWIDTH
			if (layer->design_rule_width != 0) {
				fprintf(out, "DESIGNRULEWIDTH %f ", layer->design_rule_width/unit);
			}

			fputs(";\n", out);

			// WIDTH
			if (layer->width != 0) {
				fprintf(out, "%sWIDTH %f ;\n", indent, layer->width/unit);
			}

			// Shapes
			for (unsigned u = 0; u < layer->shapes.size; ++u) {
				lef_geo_shape_t *shape = array_at(layer->shapes, lef_geo_shape_t*, u);

				const char *kind = NULL;
				switch (shape->kind) {
					case LEF_SHAPE_PATH:    kind = "PATH"; break;
					case LEF_SHAPE_RECT:    kind = "RECT"; break;
					case LEF_SHAPE_POLYGON: kind = "POLYGON"; break;
					default:
						assert(0 && "Invalid shape kind");
				}
				fprintf(out, "%s%s ", indent, kind);

				// MASK
				if (shape->mask != 0) {
					fprintf(out, "MASK %d ", (int)shape->mask);
				}

				// ITERATE
				if (shape->iterate) {
					fputs("ITERATE ", out);
				}

				// Points
				for (unsigned u = 0; u < shape->num_points; ++u) {
					write_xy(shape->points[u], out);
					fputc(' ', out);
				}

				// Step Pattern
				if (shape->iterate) {
					assert(0 && "Step pattern not implemented");
					/// @todo Add support for step pattern.
				}

				fputs(";\n", out);
			}

		} else if (geo->kind == LEF_GEO_VIA) {
			assert(0 && "not implemented");
			/// @todo Implement VIA geometries.
		}

		prev = geo;
	}
}


static void
write_port(lef_port_t *port, FILE *out, const char *indent) {
	assert(port && out);

	size_t indent_len = strlen(indent);
	char indent2[indent_len+2];
	memcpy(indent2, indent, indent_len);
	indent2[indent_len] = '\t';
	indent2[indent_len+1] = 0;

	fprintf(out, "%sPORT\n", indent);

	// CLASS
	const char *class = NULL;
	switch (port->cls) {
		case LEF_PORT_CLASS_NONE: class = "NONE"; break;
		case LEF_PORT_CLASS_CORE: class = "CORE"; break;
		case LEF_PORT_CLASS_BUMP: class = "BUMP"; break;
	}
	if (class) {
		fprintf(out, "%sCLASS %s ;\n", indent2, class);
	}

	// Layer Geometries
	write_geos(port->geos.items, port->geos.size, out, indent2);

	fprintf(out, "%sEND\n", indent);
}


static void
write_pin(lef_pin_t *pin, FILE *out, const char *indent) {
	assert(pin && out);

	size_t indent_len = strlen(indent);
	char indent2[indent_len+2];
	memcpy(indent2, indent, indent_len);
	indent2[indent_len] = '\t';
	indent2[indent_len+1] = 0;

	fprintf(out, "\n%sPIN %s\n", indent, pin->name);

	// TAPERRULE

	// DIRECTION
	const char *dir = NULL;
	switch (pin->direction) {
		case LEF_PIN_DIR_INPUT:    dir = "INPUT"; break;
		case LEF_PIN_DIR_OUTPUT:   dir = "OUTPUT"; break;
		case LEF_PIN_DIR_TRISTATE: dir = "OUTPUT TRISTATE"; break;
		case LEF_PIN_DIR_INOUT:    dir = "INOUT"; break;
		case LEF_PIN_DIR_FEEDTHRU: dir = "FEEDTHRU"; break;
	}
	if (dir) {
		fprintf(out, "%sDIRECTION %s ;\n", indent2, dir);
	}

	// USE
	const char *use = NULL;
	switch (pin->use) {
		case LEF_PIN_USE_SIGNAL: dir = "SIGNAL"; break;
		case LEF_PIN_USE_ANALOG: dir = "ANALOG"; break;
		case LEF_PIN_USE_POWER:  dir = "POWER"; break;
		case LEF_PIN_USE_GROUND: dir = "GROUND"; break;
		case LEF_PIN_USE_CLOCK:  dir = "CLOCK"; break;
	}
	if (use) {
		fprintf(out, "%sUSE %s ;\n", indent2, use);
	}

	// SHAPE
	const char *shape = NULL;
	switch (pin->shape) {
		case LEF_PIN_SHAPE_ABUTMENT: dir = "ABUTMENT"; break;
		case LEF_PIN_SHAPE_RING:     dir = "RING"; break;
		case LEF_PIN_SHAPE_FEEDTHRU: dir = "FEEDTHRU"; break;
	}
	if (shape) {
		fprintf(out, "%sSHAPE %s ;\n", indent2, shape);
	}

	// MUSTJOIN
	if (pin->must_join) {
		fprintf(out, "%sMUSTJOIN %s ;\n", indent2, pin->must_join);
	}

	// PORT
	for (unsigned u = 0; u < pin->ports.size; ++u) {
		write_port(array_at(pin->ports, lef_port_t*, u), out, indent2);
	}

	fprintf(out, "%sEND %s\n", indent, pin->name);
}


static void
write_macro(lef_macro_t *macro, FILE *out, const char *indent) {
	assert(macro && out);

	size_t indent_len = strlen(indent);
	char indent2[indent_len+2];
	memcpy(indent2, indent, indent_len);
	indent2[indent_len] = '\t';
	indent2[indent_len+1] = 0;

	// MACRO <name>
	fprintf(out, "\n%sMACRO %s\n", indent, macro->name);

	// FOREIGN

	// ORIGIN
	if (macro->origin.x != 0 || macro->origin.y != 0) {
		fprintf(out, "%sORIGIN ", indent2);
		write_xy(macro->origin, out);
		fputs(" ;\n", out);
	}

	// SIZE
	if (macro->size.x != 0 || macro->size.y != 0) {
		fprintf(out, "%sSIZE %f BY %f ;\n", indent2, macro->size.x/unit, macro->size.y/unit);
	}

	// SYMMETRY
	if (macro->symmetry != 0) {
		fputs(indent2, out);
		fputs("SYMMETRY ", out);
		if (macro->symmetry & LEF_MACRO_SYM_X)   fputs("X ", out);
		if (macro->symmetry & LEF_MACRO_SYM_Y)   fputs("Y ", out);
		if (macro->symmetry & LEF_MACRO_SYM_R90) fputs("R90 ", out);
		fputs(";\n", out);
	}

	// SITE

	// PIN
	for (unsigned u = 0; u < macro->pins.size; ++u) {
		write_pin(array_at(macro->pins, lef_pin_t*, u), out, indent2);
	}

	// OBS
	if (macro->obs.size > 0) {
		char indent3[indent_len+3];
		memcpy(indent3, indent2, indent_len+1);
		indent3[indent_len+2] = 0;

		fputs(indent2, out);
		fputs("OBS\n", out);
		write_geos(macro->obs.items, macro->obs.size, out, indent3);
		fputs(indent2, out);
		fputs("END\n", out);
	}

	// END <name>
	fprintf(out, "%sEND %s\n", indent, macro->name);
}


static int
write(lef_t *lef, FILE *out, const char *indent) {
	assert(lef && out);
	errno = 0;

	// VERSION
	if (lef->version) {
		fprintf(out, "%sVERSION %s ;\n", indent, lef->version);
	}

	// SITEs
	// for (unsigned u = 0; u < lef->sites.size; ++u) {
	// 	write_site(array_at(lef->sites, lef_site_t*, u), out, indent);
	// }

	// MACROs
	for (unsigned u = 0; u < lef->macros.size; ++u) {
		write_macro(array_at(lef->macros, lef_macro_t*, u), out, indent);
	}

	// END LIBRARY
	fprintf(out, "%sEND LIBRARY ;\n", indent);

	return -errno;
}


int
lef_write(lef_t *lef, const char *path) {
	FILE *out;
	int err = LEF_OK;
	assert(lef && path);

	// Open the file for writing.
	out = fopen(path, "w");
	if (!out) {
		err = -errno;
		goto finish;
	}

	// Write out the contents of the LEF library.
	err = write(lef, out, "");

finish_file:
	fclose(out);
finish:
	return err;
}
