/* Copyright (c) 2016 Fabian Schuiki */
#include "design-internal.h"
#include "table.h"


/**
 * @file
 * @author Fabian Schuiki <fschuiki@student.ethz.ch>
 *
 * This file implements the phx_net struct.
 */


/**
 * Flag bits of a net as invalid.
 */
void
phx_net_invalidate(phx_net_t *net, uint8_t mask) {
	assert(net);
	if (~net->invalid & mask) {
		net->invalid |= mask;
		phx_cell_invalidate(net->cell, mask);
	}
}


static phx_table_t *
add_delay(phx_table_t *Ttail, phx_table_t *Thead) {
	assert(Ttail && Thead);

	// printf("Adding delays of table:\n");
	// phx_table_dump(Thead, stdout);
	// printf("To Table:\n");
	// phx_table_dump(Ttail, stdout);

	// Make a list of axes that is the union of the head's and tail's axes.
	unsigned num_axes_max = Ttail->num_axes + Thead->num_axes;
	unsigned num_axes = 0;
	phx_table_quantity_t quantities[num_axes_max];
	uint16_t num_indices[num_axes_max];
	phx_table_axis_t *src_axes[num_axes_max];

	unsigned uh, ut;
	for (uh = 0, ut = 0; uh < Thead->num_axes && ut < Ttail->num_axes;) {
		phx_table_axis_t *ah = Thead->axes + uh;
		phx_table_axis_t *at = Thead->axes + ut;
		if (ah->quantity <= at->quantity) {
			quantities[num_axes] = ah->quantity;
			num_indices[num_axes] = ah->num_indices;
			src_axes[num_axes] = ah;
			++num_axes;
		} else {
			quantities[num_axes] = at->quantity;
			num_indices[num_axes] = at->num_indices;
			src_axes[num_axes] = at;
			++num_axes;
		}
		if (ah->quantity <= at->quantity) ++uh;
		if (ah->quantity >= at->quantity) ++ut;
	}
	for (; uh < Thead->num_axes; ++uh, ++num_axes) {
		phx_table_axis_t *a = Thead->axes + uh;
		quantities[num_axes] = a->quantity;
		num_indices[num_axes] = a->num_indices;
		src_axes[num_axes] = a;
	}
	for (; ut < Ttail->num_axes; ++ut, ++num_axes) {
		phx_table_axis_t *a = Ttail->axes + ut;
		quantities[num_axes] = a->quantity;
		num_indices[num_axes] = a->num_indices;
		src_axes[num_axes] = a;
	}

	// Create the table and set the indices.
	phx_table_t *tbl = phx_table_new(num_axes, quantities, num_indices);
	for (unsigned u = 0; u < num_axes; ++u) {
		phx_table_set_indices(tbl, src_axes[u]->quantity, src_axes[u]->indices);
	}

	// Add the two tables and return the result.
	phx_table_add(tbl, Ttail, Thead);
	return tbl;
}


static void
combine_arcs(phx_net_t *net, phx_net_t *other_net, phx_timing_arc_t *arc, phx_timing_arc_t *other_arc, phx_table_t *delay, phx_table_t *transition) {

	// The two tables are now relative to the other net's input transition. What
	// we want, however, is to have these tables relative to the cell's input
	// transition. Therefore we resample the transition and delay tables using
	// the other arc's transition table.
	if (other_arc->transition) {
		if (transition) {
			// printf("Resampling transition table\n");
			transition = phx_table_join(transition, PHX_TABLE_IN_TRANS, other_arc->transition);
			// phx_table_dump(transition, stdout);
		}
		if (delay) {
			// printf("Resampling delay table\n");
			delay = phx_table_join(delay, PHX_TABLE_IN_TRANS, other_arc->transition);
			// phx_table_dump(delay, stdout);
		}
	}

	if (delay && other_arc->delay) {
		delay = add_delay(delay, other_arc->delay);
	}

	// if (transition) {
	// 	printf("Transition table:\n");
	// 	phx_table_dump(transition, stdout);
	// }
	// if (delay) {
	// 	printf("Delay table:\n");
	// 	phx_table_dump(delay, stdout);
	// }

	// Store the combined arc in the net.
	if (transition || delay) {
		phx_timing_arc_t *out_arc = array_add(&net->arcs, NULL);
		memset(out_arc, 0, sizeof(*out_arc));
		out_arc->related_pin = other_arc->related_pin;
		out_arc->delay = delay;
		out_arc->transition = transition;
	}

	// if (delay && other_arc->delay)
	// 	delay = table_join_along_in_trans(other_arc->delay, delay);
	// if (transition && other_arc->transition)
	// 	transition = table_join_along_in_trans(other_arc->transition, transition);
}


static void
phx_net_update_timing_arc_forward(phx_net_t *net, phx_net_t *other_net, phx_timing_arc_t *arc) {
	assert(net && other_net && arc);
	// printf("Update arc %s -> %s (arc %p)\n", other_net->name, net->name, arc);

	// If the net is not exposed to outside circuitry, the capacitive load is
	// known and the delay and transition tables can be reduced by fixing the
	// output capacitance.
	phx_table_t *delay = arc->delay;
	phx_table_t *transition = arc->transition;
	if (!net->is_exposed) {
		phx_table_fix_t fix = {PHX_TABLE_OUT_CAP, {net->capacitance}};
		if (delay) delay = phx_table_reduce(delay, 1, &fix);
		if (transition) transition = phx_table_reduce(transition, 1, &fix);
	} else {
		/// @todo Account for the capacitance of the net (due to routing, etc.)
		/// by subtracting it from the output capacitance indices of the delay
		/// and transition tables. IMPORTANT: What happens if one of the values
		/// becomes negative?
	}

	// if (delay) {
	// 	printf("Delay:\n");
	// 	phx_table_dump(delay, stdout);
	// }
	// if (transition) {
	// 	printf("Transition:\n");
	// 	phx_table_dump(transition, stdout);
	// }


	// If the other net is attached to an input pin, form the initial timing arc
	// by associating the tables calculated above with the input pin and storing
	// the arc in the net struct.
	if (other_net->is_exposed) {
		// printf("Using as initial arc\n");
		phx_timing_arc_t *out_arc = array_add(&net->arcs, NULL);
		memset(out_arc, 0, sizeof(*out_arc));
		for (unsigned u = 0; u < other_net->conns.size; ++u) {
			phx_terminal_t *term = array_get(&other_net->conns, u);
			if (!term->inst) {
				assert(!out_arc->related_pin && "Two input pins connected to the same net. What should I do now?");
				// printf("  to pin %s\n", term->pin->name);
				out_arc->related_pin = term->pin;
			}
		}
		out_arc->delay = delay;
		out_arc->transition = transition;
	}


	// If the other net is not attached to an input pin, combine the timing arcs
	// already established for that net with the tables calculated above.
	else {
		// printf("Combining with other net\n");
		for (unsigned u = 0; u < other_net->arcs.size; ++u) {
			phx_timing_arc_t *other_arc = array_get(&other_net->arcs, u);
			// printf("  which already has arc to pin %s\n", other_arc->related_pin->name);
			combine_arcs(net, other_net, arc, other_arc, delay, transition);
		}
	}

}


static void
phx_net_update_timing(phx_net_t *net) {
	assert(net);
	net->invalid &= ~PHX_TIMING;
	printf("Updating timing arcs of net %s.%s\n", net->cell->name, net->name);

	// Iterate over the terminals attached to this net.
	for (unsigned u = 0; u < net->conns.size; ++u) {
		phx_terminal_t *term = array_get(&net->conns, u);
		if (!term->inst) continue;
		printf("  %s.%s\n", term->inst->name, term->pin->name);

		// Determine the timing arcs associated with the connected pin.
		for (unsigned u = 0; u < term->inst->cell->arcs.size; ++u) {
			phx_timing_arc_t *arc = array_get(&term->inst->cell->arcs, u);
			if (arc->pin != term->pin) continue;
			printf("    has timing arc to %s.%s\n", term->inst->name, arc->related_pin->name);

			// Ensure the timing arcs are updated for the net connecting to this
			// pin.
			for (unsigned u = 0; u < net->cell->nets.size; ++u) {
				phx_net_t *other_net = array_at(net->cell->nets, phx_net_t*, u);
				for (unsigned u = 0; u < other_net->conns.size; ++u) {
					phx_terminal_t *other_term = array_get(&other_net->conns, u);
					if (other_term->pin == arc->related_pin && other_term->inst == term->inst) {
						// printf("    dependent on net %s (%p)\n", other_net->name, other_net);
						phx_net_update(other_net, PHX_TIMING);
						// printf("    ----------------\n");
					}
				}
			}

			for (unsigned u = 0; u < net->cell->nets.size; ++u) {
				phx_net_t *other_net = array_at(net->cell->nets, phx_net_t*, u);
				int is_related = 0;
				for (unsigned u = 0; u < other_net->conns.size; ++u) {
					phx_terminal_t *other_term = array_get(&other_net->conns, u);
					if (other_term->pin == arc->related_pin && other_term->inst == term->inst) {
						is_related = 1;
						break;
					}
				}
				if (is_related) {
					phx_net_update_timing_arc_forward(net, other_net, arc);
				}
			}
		}
	}

	// Push the timing arcs of this net that concern exposed pins to the cell.
	for (unsigned u = 0; u < net->conns.size; ++u) {
		phx_terminal_t *term = array_get(&net->conns, u);
		if (term->pin->cell == net->cell && term->inst == NULL) {
			for (unsigned u = 0; u < net->arcs.size; ++u) {
				phx_timing_arc_t *arc = array_get(&net->arcs, u);
				printf("Copying timing arc %s.%s -> %s.%s to pin %s.%s\n", arc->related_pin->cell->name, arc->related_pin->name, net->cell->name, net->name, term->pin->cell->name, term->pin->name);
				phx_cell_set_timing_table(net->cell, term->pin, arc->related_pin, PHX_TIM_TRANS, arc->transition);
				phx_cell_set_timing_table(net->cell, term->pin, arc->related_pin, PHX_TIM_DELAY, arc->delay);
			}
		}
	}
}


/**
 * Update invalid bits of a net.
 */
void
phx_net_update(phx_net_t *net, uint8_t bits) {
	assert(net);
	if (net->invalid & bits & PHX_TIMING)
		phx_net_update_timing(net);
}
