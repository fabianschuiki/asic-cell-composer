/* Copyright (c) 2016 Fabian Schuiki */
#include "util.h"

char *
dupstr(const char *src) {
	if (!src) return NULL;
	size_t len = strlen(src);
	char *dst = malloc(len+1);
	memcpy(dst, src, len);
	dst[len] = 0;
	return dst;
}

char *
dupstrn(const char *src, size_t len) {
	if (!src) return NULL;
	char *dst = malloc(len+1);
	memcpy(dst, src, len);
	dst[len] = 0;
	return dst;
}

void *
dupmem(const void *src, size_t len) {
	if (!src) return NULL;
	void *dst = malloc(len);
	memcpy(dst, src, len);
	return dst;
}


void
ref(void *ptr) {
	assert(ptr);
	__sync_add_and_fetch((int32_t*)ptr, 1);
}

void
unref(void *ptr) {
	assert(ptr);
	int32_t rc = __sync_sub_and_fetch((int32_t*)ptr, 1);
	assert(rc >= 0 && "ptr already unref'd");
	if (rc == 0) {
		/// @todo If this is to be any useful, the ptr must also contain a
		/// function ptr that handles freeing. Maybe fat pointers?
		free(ptr);
	}
}
