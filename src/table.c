/* Copyright (c) 2016 Fabian Schuiki */
#include "table.h"

/**
 * @file
 * @author Fabian Schuiki <fschuiki@student.ethz.ch>
 *
 * This file implements a multidimensional lookup table for real values. The
 * table consists of one or more axes, each with one or more index, and an array
 * of values for each combination of axis indices. The table performs
 * interpolation during lookup for real axis. Indices are required to be in
 * ascending order to simplify interpolation.
 */


static void phx_table_destroy(phx_table_t*);


/**
 * Establishes an ordering by quantity between phx_table_axis_t structs.
 */
static int
compare_axes(phx_table_axis_t *a, phx_table_axis_t *b) {
	return (int)a->quantity - (int)b->quantity;
}


/**
 * Create a new table with a given number of axes and indices.
 *
 * @param num_axes The number of axes the table has, i.e. its dimensionality.
 * @param quantities The quantity each of the axes represents. As such, must be
 *     an array with exactly `num_axes` members. Each quantity is only allowed
 *     once per table.
 * @param num_indices The number of indices each of the axes contains. As such,
 *     must be an array with exactly `num_axes` members.
 */
phx_table_t *
phx_table_new(uint8_t num_axes, phx_table_quantity_t *quantities, uint16_t *num_indices) {
	assert(num_axes == 0 || (quantities && num_indices));

	// Prepare the table axis structs describing the layout of the data in the
	// table. This list needs to be sorted by the quantity field to allow for
	// efficient mappings between tables.
	phx_table_axis_t axes[num_axes];
	size_t index_size[num_axes];
	size_t indices_size = 0;
	uint32_t stride = 1;
	uint8_t axes_set = 0;
	memset(index_size, 0, sizeof(index_size));

	for (unsigned u = 0; u < num_axes; ++u) {
		assert(num_indices[u] > 0);
		axes_set |= (1 << quantities[u]);
		axes[u].quantity = quantities[u];
		axes[u].num_indices = num_indices[u];
		axes[u].index = u;
		axes[u].stride = stride;
		stride *= num_indices[u];
		index_size[u] = num_indices[u] * sizeof(union phx_table_index);
		indices_size += index_size[u];
	}

	// Create the table format.
	phx_table_format_t *fmt = phx_table_format_create(axes_set);
	if (fmt) {
		for (unsigned u = 0; u < num_axes; ++u) {
			phx_table_format_set_stride(fmt, quantities[u], axes[u].stride);
		}
		phx_table_format_finalize(fmt);
	}

	// Allocate enough storage to hold the table structure, the indices, and the
	// table data in one chunk of memory.
	size_t axes_offset = sizeof(phx_table_t);
	size_t indices_offset = axes_offset + sizeof(axes);
	size_t data_offset = indices_offset + indices_size;
	size_t end_offset = data_offset + stride * sizeof(double);

	void *ptr = calloc(1, end_offset);
	phx_table_t *tbl = ptr;
	tbl->refcount = 1;
	tbl->fmt = fmt;
	tbl->data = ptr + data_offset;
	tbl->size = stride;
	tbl->num_axes = num_axes;

	// Allocate the memory for the indices.
	void *index_ptr = ptr + indices_offset;
	for (unsigned u = 0; u < num_axes; ++u) {
		axes[u].indices = index_ptr;
		index_ptr += index_size[u];
	}
	assert(index_ptr == ptr + data_offset);

	// Sort the axes description by the quantity field and copy it into the
	// table.
	if (num_axes) {
		qsort(axes, num_axes, sizeof(phx_table_axis_t), (void*)compare_axes);
		memcpy(tbl->axes, axes, sizeof(axes));
	}

	return tbl;
}


phx_table_t *
phx_table_create_with_format(phx_table_format_t *fmt) {
	// Calculate how many values the table shall have.
	unsigned num_values;
	if (fmt) {
		phx_table_format_ref(fmt);
		num_values = fmt->num_values;
	} else {
		num_values = 1;
	}

	// Allocate the memory for the table.
	assert(num_values > 0);
	void *ptr = calloc(1, sizeof(phx_table_t) + num_values * sizeof(double));
	phx_table_t *tbl = ptr;
	tbl->refcount = 1;
	tbl->fmt = fmt;
	tbl->data = ptr + sizeof(phx_table_t);
	return tbl;
}


static void
phx_table_destroy(phx_table_t *tbl) {
	assert(tbl && tbl->refcount == 0);
	if (tbl->fmt)
		phx_table_format_unref(tbl->fmt);
	free(tbl);
}


void
phx_table_ref(phx_table_t *tbl) {
	assert(tbl && tbl->refcount > 0);
	++tbl->refcount;
}


void
phx_table_unref(phx_table_t *tbl) {
	assert(tbl && tbl->refcount > 0);
	if (--tbl->refcount)
		phx_table_destroy(tbl);
}


phx_table_format_t *
phx_table_get_format(phx_table_t *tbl) {
	assert(tbl);
	return tbl->fmt;
}


phx_table_t *
phx_table_duplicate(phx_table_t *tbl) {
	assert(tbl);
	assert(0 && "not implemented");
}


/**
 * Destroys a table.
 */
void
phx_table_free(phx_table_t *tbl) {
	assert(tbl);
	free(tbl);
}


/**
 * Called recursively to dump the table contents for debugging purposes.
 */
static void
dump_data(phx_table_t *tbl, uint32_t base, uint8_t ax, FILE *out) {
	assert(tbl && base < tbl->fmt->num_values && ax < tbl->fmt->num_axes && out);
	phx_table_axis_t *axis = tbl->fmt->axes+ax;
	if (ax == 0) {
		for (unsigned u = 0; u < axis->num_indices; ++u) {
			fprintf(out, " % 10.3g", tbl->data[base + u * axis->stride]);
		}
	} else if (ax == 1) {
		phx_table_axis_t *inner = tbl->fmt->axes+ax-1;
		fprintf(out, "             |");
		for (unsigned u = 0; u < inner->num_indices; ++u) {
			fprintf(out, " ");
			switch (inner->quantity & PHX_TABLE_TYPE) {
				case PHX_TABLE_TYPE_REAL: fprintf(out, "% 10.3g", (double)inner->indices[u].real); break;
				case PHX_TABLE_TYPE_INT:  fprintf(out, "% 10ld", (int64_t)inner->indices[u].integer); break;
			}
		}
		fprintf(out, "\n");
		fprintf(out, "  -----------+");
		for (unsigned u = 0; u < inner->num_indices; ++u) {
			fprintf(out, "-----------");
		}
		fprintf(out, "\n");

		for (unsigned u = 0; u < axis->num_indices; ++u) {
			fprintf(out, "  ");
			switch (axis->quantity & PHX_TABLE_TYPE) {
				case PHX_TABLE_TYPE_REAL: fprintf(out, "% 10.3g", (double)axis->indices[u].real); break;
				case PHX_TABLE_TYPE_INT:  fprintf(out, "% 10ld", (int64_t)axis->indices[u].integer); break;
			}
			fprintf(out, " |");
			dump_data(tbl, base + u * axis->stride, ax-1, out);
			fprintf(out, "\n");
		}
	} else {
		for (unsigned u = 0; u < axis->num_indices; ++u) {
			fprintf(out, "  [#%u = ", (unsigned)ax);
			switch (axis->quantity & PHX_TABLE_TYPE) {
				case PHX_TABLE_TYPE_REAL: fprintf(out, "%g", (double)axis->indices[u].real); break;
				case PHX_TABLE_TYPE_INT:  fprintf(out, "%ld", (int64_t)axis->indices[u].integer); break;
			}
			fprintf(out, "]\n");
			dump_data(tbl, base + u * axis->stride, ax-1, out);
		}
	}
}


/**
 * Dumps the contents of a table; useful for debugging.
 */
void
phx_table_dump(phx_table_t *tbl, FILE *out) {
	assert(tbl && out);

	if (!tbl->fmt) {
		fprintf(out, "table (0 axes, 1 value) { %g }\n", tbl->data[0]);
		return;
	}

	fprintf(out, "table (%u axes, %u values) {\n", (unsigned)tbl->fmt->num_axes, (unsigned)tbl->fmt->num_values);
	for (unsigned u = 0; u < tbl->fmt->num_axes; ++u) {
		phx_table_axis_t *axis = tbl->fmt->axes+u;
		fprintf(out, "  axis #%u: %02x, stride = %u, %u indices [", u, (unsigned)axis->id, (unsigned)axis->stride, (unsigned)axis->num_indices);
		for (unsigned u = 0; u < axis->num_indices; ++u) {
			if (u != 0) fputc(',', out);
			switch (axis->quantity & PHX_TABLE_TYPE) {
				case PHX_TABLE_TYPE_REAL: fprintf(out, "%g", (double)axis->indices[u].real); break;
				case PHX_TABLE_TYPE_INT:  fprintf(out, "%ld", (int64_t)axis->indices[u].integer); break;
			}
		}
		fprintf(out, "]\n");
	}
	if (tbl->fmt->num_axes == 1) {
		phx_table_axis_t *axis = tbl->fmt->axes;
		fprintf(out, "  ");
		for (unsigned u = 0; u < axis->num_indices; ++u) {
			fprintf(out, " ");
			switch (axis->quantity & PHX_TABLE_TYPE) {
				case PHX_TABLE_TYPE_REAL: fprintf(out, "% 10.3g", (double)axis->indices[u].real); break;
				case PHX_TABLE_TYPE_INT:  fprintf(out, "% 10ld", (int64_t)axis->indices[u].integer); break;
			}
		}
		fprintf(out, "\n  ");
		for (unsigned u = 0; u < axis->num_indices; ++u) {
			fprintf(out, "-----------");
		}
		fprintf(out, "\n  ");
	}
	dump_data(tbl, 0, tbl->fmt->num_axes-1, out);
	if (tbl->fmt->num_axes == 1) {
		fprintf(out, "\n");
	}
	fprintf(out, "}\n");
}


static int
search_axes(phx_table_quantity_t *qty, phx_table_axis_t *axis) {
	return (int)*qty - (int)axis->quantity;
}


void
phx_table_set_indices(phx_table_t *tbl, phx_table_quantity_t qty, void *indices) {
	assert(tbl && indices);
	phx_table_axis_t *axis = bsearch(&qty, tbl->axes, tbl->num_axes, sizeof(phx_table_axis_t), (void*)search_axes);
	assert(axis);
	memcpy(axis->indices, indices, axis->num_indices * sizeof(union phx_table_index));

	assert(tbl->fmt && tbl->fmt->refcount == 1);
	phx_table_format_set_indices(tbl->fmt, axis->quantity, axis->num_indices, indices);
	phx_table_format_finalize(tbl->fmt);
}


void
phx_table_lerp_axes(phx_table_t *tbl, uint8_t num_lerps, phx_table_quantity_t *quantities, phx_table_index_t *values, phx_table_lerp_t *out) {
	assert(num_lerps > 0 && quantities && values && out);
	for (unsigned u = 0; u < num_lerps; ++u) {
		phx_table_lerp_t *lerp = out+u;
		memset(lerp, 0, sizeof(*lerp));

		phx_table_axis_t *axis = bsearch(quantities+u, tbl->axes, tbl->num_axes, sizeof(phx_table_axis_t), (void*)search_axes);
		if (!axis) continue;

		// printf("lerping axis %02x at %g\n", axis->quantity, values[u].real);

		unsigned idx_start = 0, idx_end = axis->num_indices;
		while (idx_start < idx_end) {
			unsigned idx_mid = idx_start + (idx_end - idx_start) / 2;
			int64_t result = values[u].integer - axis->indices[idx_mid].integer;
			if (result < 0) {
				idx_end = idx_mid;
			} else if (result > 0) {
				idx_start = idx_mid + 1;
			} else {
				idx_start = idx_mid;
				idx_end = idx_mid;
			}
		}

		if (idx_start+1 >= axis->num_indices) {
			idx_start = axis->num_indices - 2;
		}
		idx_end = idx_start+1;
		assert(idx_start < axis->num_indices);
		assert(idx_end < axis->num_indices);

		phx_table_index_t ind0 = axis->indices[idx_start];
		phx_table_index_t ind1 = axis->indices[idx_end];
		double f;
		switch (quantities[u] & PHX_TABLE_TYPE) {
			case PHX_TABLE_TYPE_REAL: {
				f = (values[u].real - ind0.real) / (ind1.real - ind0.real);
				if (f > 1) f = 1;
				if (f < 0) f = 0;
			} break;
			case PHX_TABLE_TYPE_INT: {
				if (values[u].integer == ind0.integer) {
					f = 0;
				} else {
					f = 1;
				}
			} break;
		}

		lerp->axis_id = axis->quantity;
		lerp->axis = axis;
		lerp->lower = idx_start;
		lerp->upper = idx_end;
		lerp->f = f;
	}
}


/**
 * Adds the values of two tables and stores the result in a third table.
 *
 * @todo Describe properly how this works, what the inputs and outputs are, and
 *       what constraints exist.
 *
 * @todo Make this function interpolate between values when it reads from table
 *       Ta and Tb to populate a cell in table Tr.
 */
void
phx_table_add(phx_table_t *Tr, phx_table_t *Ta, phx_table_t *Tb) {
	assert(Tr && Ta && Tb);

	unsigned Na = Tr->num_axes;
	unsigned idx[Na];
	memset(idx, 0, sizeof(idx));

	// Make a list of axis triples, one for every axis in the result table. The
	// triples associate corresponding axis from the three tables. A NULL
	// pointer for one of the axis indicates that that table does not contain
	// such an axis.
	struct axis_triple {
		phx_table_axis_t *r, *a, *b;
	} triples[Na];
	for (unsigned ur = 0, ua = 0, ub = 0; ur < Na; ++ur) {
		struct axis_triple *triple = triples + ur;
		triple->r = Tr->axes + ur;
		while (ua < Ta->num_axes && Ta->axes[ua].quantity < Tr->axes[ur].quantity) ++ua;
		while (ub < Tb->num_axes && Tb->axes[ub].quantity < Tr->axes[ur].quantity) ++ub;
		triple->a = (ua < Ta->num_axes && Ta->axes[ua].quantity == triple->r->quantity)
			? Ta->axes + ua
			: NULL;
		triple->b = (ub < Tb->num_axes && Tb->axes[ub].quantity == triple->r->quantity)
			? Tb->axes + ub
			: NULL;
	}

	// printf("Axis Triples:\n");
	for (unsigned u = 0; u < Na; ++u) {
		struct axis_triple *t = triples+u;
		// printf("  - r: %p, a: %p, b: %p\n", t->r, t->a, t->b);
		if (t->a) {
			assert(t->r->num_indices == t->a->num_indices && memcmp(t->r->indices, t->a->indices, t->r->num_indices * sizeof(phx_table_index_t)) == 0 && "interpolation not supported");
		}
		if (t->b) {
			assert(t->r->num_indices == t->b->num_indices && memcmp(t->r->indices, t->b->indices, t->r->num_indices * sizeof(phx_table_index_t)) == 0 && "interpolation not supported");
		}
	}

	/// @todo Establish an interpolation table. That is, a table that for each
	///       axis lists the interpolation settings for every index in the
	///       result table along this axis. Follows later, for now simply assume
	///       that the indices are equivalent.

	// printf("Adding stuff\n");

	for (unsigned u = 0; u < Tr->size; ++u) {
		size_t off_r = 0;
		size_t off_a = 0;
		size_t off_b = 0;
		for (unsigned u = 0; u < Na; ++u) {
			struct axis_triple *triple = triples + u;
			off_r += idx[u] * triple->r->stride;
			if (triple->a) off_a += idx[u] * triple->a->stride;
			if (triple->b) off_b += idx[u] * triple->b->stride;
		}

		// printf("  {");
		// for (unsigned u = 0; u < Na; ++u) {
		// 	if (u != 0) printf(",");
		// 	printf("%u", idx[u]);
		// }
		// printf("} ");
		// printf("Tr[%u] = Ta[%u] + Tb[%u]\n", off_r, off_a, off_b);

		Tr->data[off_r] = Ta->data[off_a] + Tb->data[off_b];

		int carry = 1;
		for (unsigned u = 0; u < Na; ++u) {
			if (carry)
				++idx[u];
			if (idx[u] >= triples[u].r->num_indices) {
				idx[u] = 0;
				carry = 1;
			} else {
				carry = 0;
			}
		}
	}
}


/**
 * Copy values from one table to another. The copy may be limited to only a
 * subset of axes in the destination table, and may linearly interpolated
 * between values in the source table as the copy is performed.
 */
void
phx_table_copy_values(uint8_t axes_set, phx_table_t *dst, phx_table_t *src, unsigned dst_offset, unsigned src_offset, unsigned num_lerp, phx_table_lerp_t *lerp) {
	assert(dst && src);
	if (axes_set == 0)
		return;

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

	printf("Using %u destination loops:\n", num);
	for (unsigned u = 0; u < num; ++u) {
		printf("  #%u: max = %u, stride = %u\n", u, max[u], dst_stride[u]);
	}

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

	printf("Using %u interpolation iterations:\n", lerp_num);
	for (unsigned u = 0; u < lerp_num; ++u) {
		printf("  #%u: base = %u, f = %f\n", u, lerp_base[u], lerp_f[u]);
	}

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
