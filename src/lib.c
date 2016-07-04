/* Copyright (c) 2016 Fabian Schuiki */
#include "common.h"
#include "lib-internal.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


static const char *errstrs[] = {
	[LIB_OK] = "OK",
	[LIB_ERR_SYNTAX] = "Syntax errpr",
	[LIB_ERR_CELL_EXISTS] = "Cell already exists",
	[LIB_ERR_PIN_EXISTS] = "Pin already exists",
};

const char *
lib_errstr(int err) {
	if (err < 0 || err >= (int)ASIZE(errstrs))
		return "Unknown error";
	else
		return errstrs[err];
}


int
lib_read(const char *path, lib_t **out) {
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


int
lib_write(const char *path, lib_t *lib) {
	return LIB_OK;
}
