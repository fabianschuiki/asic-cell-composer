/* Copyright (c) 2016 Fabian Schuiki */
#include "common.h"
#include "lib-internal.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


static const char *errstrs[] = {
	[LIB_OK]                  = "OK",
	[LIB_ERR_SYNTAX]          = "Syntax error",
	[LIB_ERR_CELL_EXISTS]     = "Cell already exists",
	[LIB_ERR_PIN_EXISTS]      = "Pin already exists",
	[LIB_ERR_TEMPLATE_EXISTS] = "Template already exists",
	[LIB_ERR_TABLE_EXISTS]    = "Table already exists",
};

const char *
lib_errstr(int err) {
	if (err < 0 || err >= (int)ASIZE(errstrs))
		return "Unknown error";
	else
		return errstrs[err];
}


/**
 * Read a LIB from a file.
 */
int
lib_read(lib_t **out, const char *path) {
	void *ptr;
	size_t len;
	int result = LIB_OK, fd, err;
	struct stat sb;
	struct lib_lexer lex;
	struct lib *lib = NULL;
	assert(path && out);

	// Open the file for reading.
	fd = open(path, O_RDONLY);
	if (fd == -1) {
		result = -errno;
		goto finish;
	}

	// Determine the file size.
	err = fstat(fd, &sb);
	if (err == -1) {
		result = -errno;
		goto finish_fd;
	}
	len = sb.st_size;

	// Map the file into memory.
	ptr = mmap(0, len, PROT_READ, MAP_PRIVATE, fd, 0);
	if (ptr == MAP_FAILED) {
		result = -errno;
		goto finish_fd;
	}

	// Process the file.
	lib_lexer_init(&lex, ptr, len);
	result = lib_parse(&lex, &lib);
	if (result != LIB_OK) {
		char *ls, *le, *line;
		fprintf(stderr, "  in %s:%u:%u\n", path, lex.line+1, lex.column+1);

		// Search backwards to the beginning of the line.
		ls = lex.tkn_base;
		le = lex.tkn_base;
		while (ls-1 > (char*)ptr && *(ls-1) != '\n') --ls;
		while (le < lex.end && *le != '\n') ++le;

		// Print the line and a marker.
		line = malloc(le-ls+1);
		memcpy(line, ls, le-ls);
		line[le-ls] = 0;
		fputs("\n  ", stderr);
		fputs(line, stderr);
		fputs("\n  ", stderr);
		while (ls != lex.tkn_base) {
			fputc(' ', stderr);
			++ls;
		}
		while (ls != lex.tkn_end) {
			fputc('^', stderr);
			++ls;
		}
		fputs("\n\n", stderr);
	}
	lib_lexer_dispose(&lex);

	// Return a pointer to the created LEF structure, or destroy the structure
	// if an error occurred.
	if (result == LIB_OK) {
		*out = lib;
	} else {
		lib_free(lib);
	}

	// Unmap the file from memory.
finish_mmap:
	err = munmap(ptr, len);
	if (err == -1) {
		result = -errno;
		goto finish;
	}

	// Close the file.
finish_fd:
	close(fd);

finish:
	return result;
}


/**
 * Scale an input value with the appropriate SI prefix.
 */
static double
apply_si_prefix(double v, char *prefix) {
	assert(prefix);
	static const struct {
		double scale;
		char prefix;
	} prefices[] = {
		{ 1e9,   'G' },
		{ 1e6,   'M' },
		{ 1e3,   'k' },
		{ 1e0,   0 },
		{ 1e-3,  'm' },
		{ 1e-6,  'u' },
		{ 1e-9,  'n' },
		{ 1e-12, 'p' },
		{ 1e-15, 'f' },
		{ 1e-18, 'a' },
	};
	for (unsigned u = 0; u < ASIZE(prefices); ++u) {
		if (v >= prefices[u].scale) {
			*prefix = prefices[u].prefix;
			return v / prefices[u].scale;
		}
	}
	*prefix = 0;
	return v;
}


static const struct {
	unsigned id;
	const char *name;
} params[] = {
	{LIB_MODEL_INTRINSIC_RISE,   "intrinsic_rise"},
	{LIB_MODEL_INTRINSIC_FALL,   "intrinsic_fall"},
	{LIB_MODEL_RESISTANCE_RISE,  "resistance_rise"},
	{LIB_MODEL_RESISTANCE_FALL,  "resistance_fall"},
	{LIB_MODEL_CELL_RISE,        "cell_rise"},
	{LIB_MODEL_CELL_FALL,        "cell_fall"},
	{LIB_MODEL_PROPAGATION_RISE, "propagation_rise"},
	{LIB_MODEL_PROPAGATION_FALL, "propagation_fall"},
	{LIB_MODEL_TRANSITION_RISE,  "transition_rise"},
	{LIB_MODEL_TRANSITION_FALL,  "transition_fall"},
	{LIB_MODEL_CONSTRAINT_RISE,  "constraint_rise"},
	{LIB_MODEL_CONSTRAINT_FALL,  "constraint_fall"},
};


static void
write_table_format(lib_t *lib, lib_table_format_t *fmt, FILE *out, const char *indent) {
	for (unsigned u = 0; u < 3; ++u) {
		const char *name = NULL;
		switch (fmt->variables[u]) {
			case LIB_VAR_IN_TRAN:        name = "input_net_transition"; break;
			case LIB_VAR_OUT_CAP_TOTAL:  name = "total_output_net_capacitance"; break;
			case LIB_VAR_OUT_CAP_PIN:    name = "output_net_pin_cap"; break;
			case LIB_VAR_OUT_CAP_WIRE:   name = "output_net_wire_cap"; break;
			case LIB_VAR_OUT_NET_LENGTH: name = "output_net_length"; break;
			case LIB_VAR_CON_TRAN:       name = "constrained_pin_transition"; break;
			case LIB_VAR_REL_TRAN:       name = "related_pin_transition"; break;
			case LIB_VAR_REL_CAP_TOTAL:  name = "related_out_total_output_net_capacitance"; break;
			case LIB_VAR_REL_CAP_PIN:    name = "related_out_output_net_pin_cap"; break;
			case LIB_VAR_REL_CAP_WIRE:   name = "related_out_output_net_wire_cap"; break;
			case LIB_VAR_REL_NET_LENGTH: name = "related_out_output_net_length"; break;
		}
		if (name) {
			fprintf(out, "%svariable_%u : %s;\n", indent, u+1, name);
		}
	}

	for (unsigned u = 0; u < 3; ++u) {
		if (fmt->variables[u] == LIB_VAR_NONE)
			continue;

		// Lookup the unit the index values should be scaled with.
		double unit = 1;
		switch (fmt->variables[u] & LIB_VAR_UNIT_MASK) {
			case LIB_VAR_UNIT_TIME:   unit = lib->time_unit; break;
			case LIB_VAR_UNIT_CAP:    unit = lib->capacitance_unit; break;
			case LIB_VAR_UNIT_LENGTH: unit = 1e-9; break;
		}

		// Write the index values.
		fprintf(out, "%sindex_%u(\"", indent, u+1);
		for (unsigned v = 0; v < fmt->num_indices[u]; ++v) {
			if (v > 0) fputc(',', out);
			fprintf(out, "%f", fmt->indices[u][v] / unit);
		}
		fprintf(out, "\");\n");
	}
}


/**
 * Write a table to a file.
 */
static void
write_table(lib_t *lib, lib_table_t *tbl, FILE *out, const char *indent) {
	assert(tbl && out);
	assert(tbl->values);

	size_t indent_len = strlen(indent);
	char indent2[indent_len+2];
	memcpy(indent2, indent, indent_len);
	indent2[indent_len] = '\t';
	indent2[indent_len+1] = 0;

	fprintf(out, "(%s) {\n", "some_table_format");
	write_table_format(lib, &tbl->fmt, out, indent2);

	unsigned max[3];
	unsigned index[3];
	unsigned stride[3];
	unsigned num = 0;
	for (unsigned u = 0; u < 3; ++u) {
		if (tbl->fmt.variables[2-u] != LIB_VAR_NONE) {
			max[num] = tbl->fmt.num_indices[2-u];
			assert(max[num] > 0);
			index[num] = 0;
			stride[num] = tbl->strides[2-u];
			++num;
		}
	}

	fprintf(out, "%svalues(", indent2);

	bool carry;
	do {
		if (num > 1 && index[0] == 0 && index[1] > 0)
			fprintf(out, ", \\\n%s       ", indent2);
		if (num > 0) {
			fputc(index[0] == 0 ? '"' : ',', out);
		}

		unsigned idx = 0;
		for (unsigned u = 0; u < num; ++u) {
			idx += index[u] * stride[u];
		}
		fprintf(out, "%f", tbl->values[idx] / lib->time_unit);

		carry = true;
		for (unsigned u = 0; carry && u < num; ++u) {
			++index[u];
			if (index[u] == max[u]) {
				index[u] = 0;
				carry = true;
				if (u == 0)
					fputc('"', out);
			} else {
				carry = false;
			}
		}

	} while (!carry);

	fprintf(out, ");\n");
	fprintf(out, "%s}\n", indent);
}


/**
 * Write a timing group to a file.
 */
static void
write_timing(lib_t *lib, lib_timing_t *tmg, FILE *out, const char *indent) {
	assert(tmg && out);

	size_t indent_len = strlen(indent);
	char indent2[indent_len+2];
	memcpy(indent2, indent, indent_len);
	indent2[indent_len] = '\t';
	indent2[indent_len+1] = 0;

	fprintf(out, "%stiming() {\n", indent);

	// Related pins
	if (tmg->related_pins.size > 0) {
		fprintf(out, "%srelated_pin : \"", indent2);
		for (unsigned u = 0; u < tmg->related_pins.size; ++u) {
			if (u > 0)
				fputc(' ', out);
			fprintf(out, "%s", array_at(tmg->related_pins, const char*, u));
		}
		fprintf(out, "\";\n");
	}

	// Timing sense
	const char *sense = NULL;
	switch (tmg->timing_sense) {
		case LIB_TMG_POSITIVE_UNATE: sense = "positive_unate"; break;
		case LIB_TMG_NEGATIVE_UNATE: sense = "negative_unate"; break;
		case LIB_TMG_NON_UNATE:      sense = "non_unate"; break;
	}
	if (sense) {
		fprintf(out, "%stiming_sense : %s;\n", indent2, sense);
	}

	// Timing type
	const char *type = NULL;
	switch (tmg->timing_type) {
		case LIB_TMG_TYPE_COMB | LIB_TMG_EDGE_BOTH: type = "combinational"; break;
		case LIB_TMG_TYPE_COMB | LIB_TMG_EDGE_RISE: type = "combinational_rise"; break;
		case LIB_TMG_TYPE_COMB | LIB_TMG_EDGE_FALL: type = "combinational_fall"; break;
	}
	if (type) {
		fprintf(out, "%stiming_type : %s;\n", indent2, type);
	}

	// Tables
	for (unsigned u = 0; u < LIB_MODEL_NUM_PARAMS; ++u) {
		unsigned dim = params[u].id & LIB_MODEL_DIM_MASK;
		if (dim == LIB_MODEL_SCALAR) {
			if (tmg->scalars[u] != 0) {
				fprintf(out, "%s%s : %f;\n", indent2, params[u].name, tmg->scalars[u] / lib->time_unit);
			}
		} else if (dim == LIB_MODEL_TABLE) {
			if (tmg->tables[u]) {
				fprintf(out, "%s%s ", indent2, params[u].name);
				write_table(lib, tmg->tables[u], out, indent2);
			} else if (tmg->scalars[u] != 0) {
				fprintf(out, "%s%s (scalar) {\n", indent2, params[u].name);
				fprintf(out, "%s\tvalues(\"%f\");\n", indent2, tmg->scalars[u] / lib->time_unit);
				fprintf(out, "%s}\n", indent2);
			}
		}
	}

	fprintf(out, "%s}\n", indent);
}


/**
 * Write a pin to a file.
 */
static void
write_pin(lib_t *lib, lib_pin_t *pin, FILE *out, const char *indent) {
	assert(pin && out);

	size_t indent_len = strlen(indent);
	char indent2[indent_len+2];
	memcpy(indent2, indent, indent_len);
	indent2[indent_len] = '\t';
	indent2[indent_len+1] = 0;

	fprintf(out, "\n%spin (%s) {\n", indent, pin->name);

	// Capacitance
	fprintf(out, "%scapacitance : %f;\n", indent2, pin->capacitance / lib->capacitance_unit);

	// Timings
	for (unsigned u = 0; u < pin->timings.size; ++u) {
		write_timing(lib, array_at(pin->timings, lib_timing_t*, u), out, indent2);
	}

	fprintf(out, "%s} /* %s */\n", indent, pin->name);
}


/**
 * Write a cell to a file.
 */
static void
write_cell(lib_t *lib, lib_cell_t *cell, FILE *out, const char *indent) {
	assert(cell && out);

	size_t indent_len = strlen(indent);
	char indent2[indent_len+2];
	memcpy(indent2, indent, indent_len);
	indent2[indent_len] = '\t';
	indent2[indent_len+1] = 0;

	fprintf(out, "\n%scell (%s) {\n", indent, cell->name);

	// Leakage Power
	if (cell->leakage_power != 0) {
		fprintf(out, "%sleakage_power : %f;\n", indent2, cell->leakage_power/lib->leakage_power_unit);
	}

	// Pins
	for (unsigned u = 0; u < cell->pins.size; ++u) {
		write_pin(lib, array_at(cell->pins, lib_pin_t*, u), out, indent2);
	}

	fprintf(out, "%s} /* %s */\n", indent, cell->name);
}


/**
 * Write an entire library to a file.
 */
static int
write_lib(lib_t *lib, FILE *out, const char *indent) {
	assert(lib && out);

	size_t indent_len = strlen(indent);
	char indent2[indent_len+2];
	memcpy(indent2, indent, indent_len);
	indent2[indent_len] = '\t';
	indent2[indent_len+1] = 0;

	errno = 0;
	fprintf(out, "%slibrary (%s) {\n", indent, lib->name);

	// Units
	static const struct {
		const char *name;
		const char *suffix;
		size_t offset;
	} units[] = {
		{ "time_unit", "s", offsetof(lib_t, time_unit) },
		{ "voltage_unit", "V", offsetof(lib_t, voltage_unit) },
		{ "current_unit", "A", offsetof(lib_t, current_unit) },
		{ "leakage_power_unit", "W", offsetof(lib_t, leakage_power_unit) },
	};
	for (unsigned u = 0; u < ASIZE(units); ++u) {
		double *ptr = (void*)lib + units[u].offset;
		if (*ptr != 0) {
			double v_scaled;
			char v_prefix;
			v_scaled = apply_si_prefix(*ptr, &v_prefix);
			fprintf(out, "%s%s : %f", indent2, units[u].name, v_scaled);
			if (v_prefix) fputc(v_prefix, out);
			fprintf(out, "%s;\n", units[u].suffix);
		}
	}

	// Capacitance Unit
	double cap_scale = lib_get_capacitance_unit(lib);
	const char *cap_unit = NULL;
	static const struct { double scale; const char *suffix; } cap_units[] = {
		{ 1e-3,  "mf" },
		{ 1e-6,  "uf" },
		{ 1e-9,  "nf" },
		{ 1e-12, "pf" },
		{ 1e-15, "ff" },
		{ 1e-18, "af" },
	};
	for (unsigned u = 0; u < ASIZE(cap_units); ++u) {
		if (cap_scale >= cap_units[u].scale) {
			cap_scale /= cap_units[u].scale;
			cap_unit = cap_units[u].suffix;
			break;
		}
	}
	fprintf(out, "%scapacitive_load_unit(%f,%s);\n", indent2, cap_scale, cap_unit);

	// Cells
	for (unsigned u = 0; u < lib->cells.size; ++u) {
		write_cell(lib, array_at(lib->cells, lib_cell_t*, u), out, indent2);
	}

	fprintf(out, "%s} /* %s */\n", indent, lib->name);
	return -errno;
}


/**
 * Write a LIB to a file.
 */
int
lib_write(lib_t *lib, const char *path) {
	FILE *f;
	int err = LIB_OK;
	assert(lib && path);

	// Open the file for writing.
	f = fopen(path, "w");
	if (!f) {
		err = -errno;
		goto finish;
	}

	// Write the library.
	err = write_lib(lib, f, "");

finish_file:
	fclose(f);
finish:
	return err;
}
