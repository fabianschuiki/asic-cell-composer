/* Copyright (c) 2016 Fabian Schuiki */
#include "common.h"
#include "lib.h"

int main(int argc, char **argv) {
	int num_failed = 0;

	for (int i = 1; i < argc; ++i) {
		char *arg = argv[i];
		lib_t *lib;
		int err;
		printf("Reading %s\n", arg);

		err = lib_read(&lib, arg);
		if (err != LIB_OK) {
			fprintf(stderr, "Failed to read LIB file %s (error %d)\n", arg, err);
			++num_failed;
		}
	}

	return num_failed;
}
