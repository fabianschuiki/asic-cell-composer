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
