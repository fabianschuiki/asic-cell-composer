/* Copyright (c) 2016 Fabian Schuiki */
#include "table.h"
#include "util.h"

static void phx_table_format_destroy(phx_table_format_t*);


/**
 * Create a new table format for a given set of axes.
 *
 * @return If any axes were set, returns a newly allocated table format.
 * Otherwise returns NULL.
 */
phx_table_format_t *
phx_table_format_create(unsigned axes_set) {
	// If no axes are set, return the scalar table format, which is NULL.
	if (axes_set == 0)
		return NULL;

	// Calculate the number of axes in the format and populate the lookup table
	// that will map between the axis ID and the actual array of axes.
	uint8_t num_axes = 0;
	int8_t lookup_table[PHX_TABLE_MAX_AXES];
	for (unsigned u = 0; u < PHX_TABLE_MAX_AXES; ++u) {
		if (axes_set & (1 << u)) {
			lookup_table[u] = num_axes;
			++num_axes;
		} else {
			lookup_table[u] = -1;
		}
	}

	// Allocate the memory for the table format.
	phx_table_format_t *fmt = calloc(1, sizeof(phx_table_format_t) + num_axes * sizeof(phx_table_axis_t));
	fmt->refcount = 1;
	fmt->axes_set = axes_set;
	fmt->num_axes = num_axes;
	memcpy(fmt->lookup, lookup_table, sizeof(lookup_table));

	// Populate the axes array with the appropriate information.
	for (unsigned u = 0; u < PHX_TABLE_MAX_AXES; ++u) {
		if (lookup_table[u] != -1) {
			phx_table_axis_t *axis = fmt->axes + lookup_table[u];
			axis->id = u;
		}
	}

	return fmt;
}


/**
 * Deallocate all resources held by a table format and free the table format
 * itself. You should not call this function directly, but rather use the
 * phx_table_format_unref function instead.
 */
static void
phx_table_format_destroy(phx_table_format_t *fmt) {
	assert(fmt && fmt->refcount == 0);
	for (unsigned u = 0; u < fmt->num_axes; ++u) {
		if (fmt->axes[u].indices) {
			free(fmt->axes[u].indices);
		}
	}
	free(fmt);
}


/**
 * Increase the reference count of a table format.
 */
void
phx_table_format_ref(phx_table_format_t *fmt) {
	assert(fmt && fmt->refcount > 0);
	++fmt->refcount;
}


/**
 * Decrease the reference count of a table format, potentially destroying it.
 */
void
phx_table_format_unref(phx_table_format_t *fmt) {
	assert(fmt && fmt->refcount > 0);
	if (--fmt->refcount == 0)
		phx_table_format_destroy(fmt);
}


/**
 * Set the number and values of the indices along a table's axis. Note that this
 * may invalidate the strides of all axes in the table, requiring a call to
 * phx_table_format_update_strides unless the strides were manually set before.
 */
void
phx_table_format_set_indices(phx_table_format_t *fmt, unsigned axis_id, unsigned num_indices, phx_table_index_t *indices) {
	phx_table_axis_t *axis = phx_table_format_get_axis(fmt, axis_id);
	axis->num_indices = num_indices;
	if (axis->indices)
		free(axis->indices);
	axis->indices = dupmem(indices, num_indices * sizeof(*indices));
}


/**
 * Get a single axis from a table. The axis must exist.
 */
phx_table_axis_t *
phx_table_format_get_axis(phx_table_format_t *fmt, unsigned axis_id) {
	unsigned id = axis_id & 0xF;
	assert(fmt && id < PHX_TABLE_MAX_AXES && fmt->axes_set & (1 << id));
	return fmt->axes + fmt->lookup[id];
}


/**
 * Set the stride of an axis.
 */
void
phx_table_format_set_stride(phx_table_format_t *fmt, unsigned axis_id, unsigned stride) {
	phx_table_format_get_axis(fmt, axis_id)->stride = stride;
}


/**
 * Update the strides of a table format's axis. This function is useful if a new
 * table is created that contains freshly calculated data where the strides need
 * to be chosen rather than taken from an existing array of values.
 */
void
phx_table_format_update_strides(phx_table_format_t *fmt) {
	assert(fmt);
	unsigned stride = 1;
	for (unsigned u = 0; u < fmt->num_axes; ++u) {
		phx_table_axis_t *axis = fmt->axes + u;
		axis->stride = stride;
		stride *= axis->num_indices;
	}
}


/**
 * Update internal information such that the table format is ready to be used.
 */
void
phx_table_format_finalize(phx_table_format_t *fmt) {
	assert(fmt && fmt->num_axes > 0);
	fmt->num_values = 1;
	for (unsigned u = 0; u < fmt->num_axes; ++u) {
		// assert(fmt->axes[u].num_indices > 0);
		fmt->num_values *= fmt->axes[u].num_indices;
	}
}
