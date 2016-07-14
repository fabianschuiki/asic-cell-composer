/* Copyright (c) 2016 Fabian Schuiki */
#include "table.h"


/**
 * Calculate the linear interpolation parameters for an index along a table
 * axis.
 */
void
phx_table_axis_get_lerp(phx_table_axis_t *axis, phx_table_index_t index, phx_table_lerp_t *out) {
	assert(axis && out);
	out->axis_id = axis->id;
	out->axis = axis;

	// Find the position in the requested index would be located at in the
	// array of indices. If we find an exact match, no interpolation is
	// necessary and we can return immediately.
	unsigned idx_start = 0, idx_end = axis->num_indices;
	while (idx_start < idx_end) {
		unsigned idx_mid = idx_start + (idx_end - idx_start) / 2;
		int64_t result = index.integer - axis->indices[idx_mid].integer;
		if (result < 0) {
			idx_end = idx_mid;
		} else if (result > 0) {
			idx_start = idx_mid + 1;
		} else {
			out->lower = idx_mid;
			out->upper = idx_mid;
			out->f = 0;
			return;
		}
	}

	// Since location found above may be anywhere in [0,num_indices], make sure
	// that the start location lies within the range of indices.
	if (idx_start+1 >= axis->num_indices) {
		idx_start = axis->num_indices - 2;
	}
	idx_end = idx_start+1;
	assert(idx_start < axis->num_indices);
	assert(idx_end < axis->num_indices);

	// Calculate the linear interpolation factor based on the two indices found
	// above.
	phx_table_index_t idx0 = axis->indices[idx_start];
	phx_table_index_t idx1 = axis->indices[idx_end];
	double f;
	switch (axis->id & PHX_TABLE_TYPE) {
		case PHX_TABLE_TYPE_REAL: {
			f = (index.real - idx0.real) / (idx1.real - idx0.real);
			if (f > 1) f = 1;
			if (f < 0) f = 0;
		} break;
		case PHX_TABLE_TYPE_INT: {
			if (index.integer == idx0.integer) {
				f = 0;
			} else {
				f = 1;
			}
		} break;
		default:
			assert(0 && "invalid table axis type");
			return;
	}

	// Fill in the remaining information.
	out->lower = idx_start;
	out->upper = idx_end;
	out->f = f;
}


/**
 * Calculate the linear interpolation parameters for an index along a table's
 * axis. The output parameter @a out is always populated with valid parameters,
 * regardless of the function's return value.
 *
 * @return `true` if the table contained the axis, `false` otherwise.
 */
bool
phx_table_get_lerp(phx_table_t *tbl, unsigned axis_id, phx_table_index_t index, phx_table_lerp_t *out) {
	assert(tbl && out);

	// In case the table is a scalar, no linear interpolation is possible.
	// Nevertheless populate the output descriptor to valid information.
	if (!tbl->fmt || !(tbl->fmt->axes_set & PHX_TABLE_MASK(axis_id))) {
		out->axis_id = axis_id;
		out->lower = 0;
		out->upper = 0;
		out->f = 0;
		return false;
	}

	// Calculate the linear interpolation for this axis.
	phx_table_axis_t *axis = phx_table_format_get_axis(tbl->fmt, axis_id);
	phx_table_axis_get_lerp(axis, index, out);
	return true;
}


/**
 * Reduce the dimensionality of a table by fixing some of its axes to a specific
 * value. This function issentially calculates the linear interpolation required
 * along the fixed axis and then performs a linearly interpolated copy of the
 * values.
 */
phx_table_t *
phx_table_reduce(phx_table_t *T, unsigned num_fixes, phx_table_fix_t *fixes) {
	assert(T && (num_fixes == 0 || fixes));
	if (!T->fmt || num_fixes == 0) {
		phx_table_ref(T);
		return T;
	}

	// Calculate the table format of the result and at the same time gather the
	// information required for the linear interpolation among the values.
	uint8_t axes_set = T->fmt->axes_set;
	phx_table_lerp_t lerp[num_fixes];
	unsigned num_lerp = 0;
	for (unsigned u = 0; u < num_fixes; ++u) {
		unsigned mask = PHX_TABLE_MASK(fixes[u].axis_id);
		if (axes_set & mask) {
			axes_set &= ~mask;
			if (phx_table_get_lerp(T, fixes[u].axis_id, fixes[u].index, lerp+num_lerp))
				++num_lerp;
		}
	}
	if (axes_set == T->fmt->axes_set) {
		phx_table_ref(T);
		return T;
	}
	phx_table_format_t *fmt = phx_table_format_create(axes_set);
	if (fmt) {
		for (unsigned u = 0; u < fmt->num_axes; ++u) {
			phx_table_axis_t *src_axis = phx_table_format_get_axis(T->fmt, fmt->axes[u].id);
			phx_table_format_set_indices(fmt, fmt->axes[u].id, src_axis->num_indices, src_axis->indices);
		}
		phx_table_format_update_strides(fmt);
		phx_table_format_finalize(fmt);
	}

	// Create the result table and copy things over.
	phx_table_t *tbl = phx_table_create_with_format(fmt);
	if (fmt) phx_table_format_unref(fmt);
	phx_table_copy_values(axes_set, tbl, T, 0, 0, num_lerp, lerp);
	return tbl;
}


/**
 * Join two tables by using the values of the index table to index into the base
 * table.
 */
phx_table_t *
phx_table_join(phx_table_t *Tbase, unsigned axis_id, phx_table_t *Tindex) {
	assert(Tbase && Tbase->fmt && Tindex);
	unsigned axis_mask = PHX_TABLE_MASK(axis_id);

	// Handle the special case where the base table does not contain the axis
	// that we're supposed to join along, in which case the base table is the
	// result of the join.
	if (!Tbase->fmt || !(Tbase->fmt->axes_set & axis_mask)) {
		phx_table_ref(Tbase);
		return Tbase;
	}

	// Handle the special case where the index table is a scalar. The join thus
	// becomes a simple reduction.
	if (!Tindex->fmt) {
		return phx_table_reduce(Tbase, 1, (phx_table_fix_t[]){{axis_id, {Tindex->data[0]}}});
	}

	// Calculate the format of the result table. The process removes the axis
	// that is being indexed to, but adds all axes from the indexing table.
	uint8_t axes_set = (Tbase->fmt->axes_set & ~axis_mask) | Tindex->fmt->axes_set;
	phx_table_format_t *fmt = phx_table_format_create(axes_set);
	assert(fmt); // At least all of Tindex' axes are present in the table.
	for (unsigned u = 0; u < fmt->num_axes; ++u) {
		phx_table_axis_t *axis = phx_table_format_get_axis(
			(Tindex->fmt->axes_set & PHX_TABLE_MASK(fmt->axes[u].id))
				? Tindex->fmt
				: Tbase->fmt,
			fmt->axes[u].id
		);
		phx_table_format_set_indices(fmt, fmt->axes[u].id, axis->num_indices, axis->indices);
	}
	phx_table_format_update_strides(fmt);
	phx_table_format_finalize(fmt);
	phx_table_t *tbl = phx_table_create_with_format(fmt);
	phx_table_format_unref(fmt);

	// Establish the nested loops that are required to copy the values over.
	unsigned num_loops = Tindex->fmt->num_axes;
	unsigned max[num_loops];
	unsigned index[num_loops];
	unsigned src_stride[num_loops];
	unsigned dst_stride[num_loops];
	for (unsigned u = 0; u < Tindex->fmt->num_axes; ++u) {
		phx_table_axis_t *axis = Tindex->fmt->axes + u;
		index[u] = 0;
		max[u] = axis->num_indices;
		src_stride[u] = axis->stride;
		dst_stride[u] = phx_table_format_get_axis(Tbase->fmt, axis->id)->stride;
	}

	// For every value in the Tindex table, calculate the linear interpolation
	// within the Tbase table and perform a copy of the values.
	bool carry;
	do {
		// Calculate the index into Tindex and the result table.
		unsigned src_idx = 0, dst_idx = 0;
		for (unsigned u = 0; u < num_loops; ++u) {
			src_idx += index[u] * src_stride[u];
			dst_idx += index[u] * dst_stride[u];
		}

		// Copy the values over.
		phx_table_lerp_t lerp;
		phx_table_get_lerp(Tbase, axis_id, (phx_table_index_t){Tindex->data[src_idx]}, &lerp);
		phx_table_copy_values(Tbase->fmt->axes_set & ~axis_mask, tbl, Tbase, dst_idx, 0, 1, &lerp);

		// Increment the indices.
		carry = true;
		for (unsigned u = 0; carry && u < num_loops; ++u) {
			++index[u];
			if (index[u] == max[u]) {
				index[u] = 0;
				carry = true;
			} else {
				carry = false;
			}
		}
	} while (!carry);

	return tbl;
}


/**
 * Copy values from one table to another. The copy may be limited to only a
 * subset of axes in the destination table, and may linearly interpolated
 * between values in the source table as the copy is performed.
 */
void
phx_table_copy_values(uint8_t axes_set, phx_table_t *dst, phx_table_t *src, unsigned dst_offset, unsigned src_offset, unsigned num_lerp, phx_table_lerp_t *lerp) {
	assert(dst && src);

	// Calculate the number of nested iterations that are necessary to perform
	// the copy. This includes establishing the maximum index and stride for
	// each axis. Note that scalar tables are treated separately with only a
	// single loop iteration.
	unsigned max_slots = dst->fmt ? dst->fmt->num_axes : 1;
	unsigned max[max_slots];
	unsigned index[max_slots];
	unsigned dst_stride[max_slots];
	unsigned src_stride[max_slots];
	unsigned num = 0;

	if (dst->fmt) {
		for (unsigned u = 0; u < dst->fmt->num_axes; ++u) {
			phx_table_axis_t *dst_axis = dst->fmt->axes + u;
			unsigned mask = PHX_TABLE_MASK(dst_axis->id);

			if (axes_set & mask) {
				max[num] = dst_axis->num_indices;
				index[num] = 0;
				dst_stride[num] = dst_axis->stride;

				// If the source table has this axis as well, store its stride
				// for the later iteration. Otherwise the table is constant
				// along this axis and the stride is zero.
				if (src->fmt && src->fmt->axes_set & mask) {
					phx_table_axis_t *src_axis = phx_table_format_get_axis(src->fmt, dst_axis->id);
					assert(max[num] == src_axis->num_indices);
					src_stride[num] = src_axis->stride;
				} else {
					src_stride[num] = 0;
				}

				++num;
			}
		}
	} else {
		max[0] = 1;
		index[0] = 0;
		dst_stride[0] = 0;
		src_stride[0] = 0;
		num = 1;
	}

	// printf("Using %u destination loops:\n", num);
	// for (unsigned u = 0; u < num; ++u) {
	// 	printf("  #%u: max = %u, stride = %u\n", u, max[u], dst_stride[u]);
	// }

	// Calculate the offsets needed to perform the linear interpolation in the
	// source table. This essentially creates two arrays that contain the base
	// index and multiplication factor for each degree of freedom of the linear
	// interpolation. A maxmimum number of 2**num_lerp slots are created. For
	// every value in the destination table, lerp_num values from the source
	// table need to be read and interpolated.
	unsigned lerp_slots = (1 << num_lerp);
	unsigned lerp_base[lerp_slots];
	double lerp_f[lerp_slots];
	unsigned lerp_num;

	lerp_base[0] = 0;
	lerp_f[0] = 1;
	lerp_num = 1;
	if (src->fmt) {
		for (unsigned u = 0; u < num_lerp; ++u) {
			if (src->fmt->axes_set & PHX_TABLE_MASK(lerp[u].axis_id)) {
				phx_table_axis_t *axis = phx_table_format_get_axis(src->fmt, lerp[u].axis_id);
				for (unsigned v = 0; v < lerp_num; ++v) {
					lerp_base[v]          += lerp[u].lower * axis->stride;
					lerp_base[v+lerp_num]  = lerp[u].upper * axis->stride;
					lerp_f[v]             *= 1.0 - lerp[u].f;
					lerp_f[v+lerp_num]     = lerp[u].f;
				}
				lerp_num <<= 1;
			}
		}
	}

	// printf("Using %u interpolation iterations:\n", lerp_num);
	// for (unsigned u = 0; u < lerp_num; ++u) {
	// 	printf("  #%u: base = %u, f = %f\n", u, lerp_base[u], lerp_f[u]);
	// }

	// Perform the copy.
	bool carry;
	do {
		// Calculate the index into the destination and source tables.
		unsigned dst_idx = 0, src_idx = 0;
		for (unsigned u = 0; u < num; ++u) {
			dst_idx += index[u] * dst_stride[u];
			src_idx += index[u] * src_stride[u];
		}

		// Calculate the value for the destination table by performing the
		// linear inteprolations as determined above.
		double v = 0;
		for (unsigned u = 0; u < lerp_num; ++u) {
			v += src->data[src_offset + src_idx + lerp_base[u]] * lerp_f[u];
		}

		// Write the value to the appropriate location in the destination table.
		dst->data[dst_offset + dst_idx] = v;

		// Advance the index counters.
		carry = true;
		for (unsigned u = 0; u < num && carry; ++u) {
			if (++index[u] == max[u]) {
				index[u] = 0;
				carry = true;
			} else {
				carry = false;
			}
		}
	} while (!carry);
}

