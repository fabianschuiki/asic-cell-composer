/* Copyright (c) 2016 Fabian Schuiki */
#include "lib-internal.h"
#include "util.h"
#include <ctype.h>


enum stmt_kind {
	STMT_GRP,
	STMT_SATTR,
	STMT_CATTR,
};

typedef struct lib_parser lib_parser_t;
typedef int (*stmt_handler_t)(lib_parser_t*, void*, enum stmt_kind, char*, char**, unsigned);

struct lib_parser {
	lib_lexer_t *lexer;
	char **params;
	size_t params_num, params_cap;
	lib_t *lib;
	lib_cell_t *cell;
	lib_pin_t *pin;
};


static int parse_stmts(lib_parser_t *parser, stmt_handler_t handler, void *arg);
static int parse_stmt(lib_parser_t *parser, stmt_handler_t handler, void *arg);


/**
 * Parse a single statement.
 */
static int
parse_stmt(lib_parser_t *parser, stmt_handler_t handler, void *arg) {
	assert(parser);
	int err = LIB_OK;
	lib_lexer_t *lex = parser->lexer;

	if (lex->tkn != LIB_IDENT) {
		fprintf(stderr, "Expected attribute or group name\n");
		err = LIB_ERR_SYNTAX;
		goto finish;
	}
	char *name = dupstr(lex->text);
	lib_lexer_next(lex);

	// If the attribute name is followed by a colon, this statement represents a
	// simple attribute. Parse the attribute value and call the handler.
	if (lex->tkn == LIB_COLON) {
		lib_lexer_next(lex);

		if (lex->tkn != LIB_IDENT) {
			fprintf(stderr, "Expected value of attribute '%s' after colon ':'\n", name);
			err = LIB_ERR_SYNTAX;
			goto finish_name;
		}

		if (handler) {
			err = handler(parser, arg, STMT_SATTR, name, &lex->text, 1);
			if (err != LIB_OK)
				goto finish_name;
		}
		lib_lexer_next(lex);

		if (lex->tkn != LIB_SEMICOLON) {
			fprintf(stderr, "Expected semicolon ';' after attribute '%s'\n", name);
			err = LIB_ERR_SYNTAX;
			goto finish_name;
		}
		lib_lexer_next(lex);
	}

	// If the attribute name is followed by an opening parenthesis, this
	// statement represents either a complex attribute or a group, as determined
	// by whether the closing parenthesis is followed by a semicolon or an
	// opening brace.
	else if (lex->tkn == LIB_LPAREN) {
		lib_lexer_next(lex);

		// Clean up the parameters of the last group.
		for (size_t z = 0; z < parser->params_num; ++z) {
			free(parser->params[z]);
		}
		parser->params_num = 0;

		while (lex->tkn != LIB_RPAREN) {
			if (lex->tkn != LIB_IDENT) {
				fprintf(stderr, "Expected parameter for attribute/group '%s' or closing parenthesis ')'\n", name);
				err = LIB_ERR_SYNTAX;
				goto finish_name;
			}

			if (parser->params_num == parser->params_cap) {
				parser->params_cap *= 2;
				parser->params = realloc(parser->params, sizeof(char**) * parser->params_cap);
			}
			parser->params[parser->params_num++] = dupstr(lex->text);
			lib_lexer_next(lex);

			if (lex->tkn == LIB_COMMA) {
				lib_lexer_next(lex);
			}
		}
		lib_lexer_next(lex);

		enum stmt_kind kind;
		if (lex->tkn == LIB_SEMICOLON) {
			kind = STMT_CATTR;
		} else if (lex->tkn == LIB_LBRACE) {
			kind = STMT_GRP;
		} else {
			fprintf(stderr, "Expected semicolon ';' or opening brace '{' after attribute/group '%s'\n", name);
			err = LIB_ERR_SYNTAX;
			goto finish_name;
		}
		lib_lexer_next(lex);

		if (handler) {
			err = handler(parser, arg, kind, name, parser->params, parser->params_num);
			if (err != LIB_OK)
				goto finish_name;
		} else {
			parse_stmts(parser, NULL, NULL);
		}

		if (kind == STMT_GRP) {
			if (lex->tkn != LIB_RBRACE) {
				fprintf(stderr, "Expected closing brace '}' after group '%s'\n", name);
				err = LIB_ERR_SYNTAX;
				goto finish_name;
			}
			lib_lexer_next(lex);
		}
	}

	// Otherwise complain about the syntax error.
	else {
		fprintf(stderr, "Expected colon ':' or opening parenthesis '(' after attribute/group name '%s'\n", name);
		err = LIB_ERR_SYNTAX;
		goto finish_name;
	}

finish_name:
	free(name);
finish:
	return err;
}

/**
 * Parse multiple statements up to the end of file or a closing brace.
 */
static int
parse_stmts(lib_parser_t *parser, stmt_handler_t handler, void *arg) {
	assert(parser);
	lib_lexer_t *lex = parser->lexer;
	while (lex->tkn != LIB_EOF && lex->tkn != LIB_RBRACE) {
		int err = parse_stmt(parser, handler, arg);
		if (err != LIB_OK)
			return err;
	}
	return LIB_OK;
}


/**
 * Converts an SI prefix character to the corresponding scaling value.
 */
static double
si_prefix_scale(char c) {
	switch (c) {
		case 'G': return 1e9;
		case 'M': return 1e6;
		case 'k': return 1e3;
		case 'm': return 1e-3;
		case 'u': return 1e-6;
		case 'n': return 1e-9;
		case 'p': return 1e-12;
		case 'f': return 1e-15;
		case 'a': return 1e-18;
		default: return 1;
	}
}


static int
parse_real(const char *str, double *out) {
	char *rest;
	errno = 0;
	*out = strtod(str, &rest);
	if (errno != 0) {
		fprintf(stderr, "'%s' is not a valid real number; %s\n", str, strerror(errno));
		return LIB_ERR_SYNTAX;
	}
	while (isspace(*rest))
		++rest;
	*out *= si_prefix_scale(*rest);
	return LIB_OK;
}


static int
parse_int(const char *str, unsigned *out) {
	errno = 0;
	*out = strtoul(str, NULL, 10);
	if (errno != 0) {
		fprintf(stderr, "'%s' is not a valid integer number; %s\n", str, strerror(errno));
		return LIB_ERR_SYNTAX;
	}
	return LIB_OK;
}


struct option {
	const char *str;
	uint32_t value;
};

static int
compare_options(const void *a, const void *b) {
	return strcmp(*(const char **)a, *(const char **)b);
}


/**
 * A lookup table for the timing_sense attribute of timing() groups.
 * IMPORTANT: Keep this list sorted alphabetically.
 */
static const struct option timing_sense_opts[] = {
	{ "negative_unate", LIB_TMG_NEGATIVE_UNATE },
	{ "non_unate",      LIB_TMG_NON_UNATE      },
	{ "positive_unate", LIB_TMG_POSITIVE_UNATE },
};

/**
 * A lookup table for the timing_type attribute of timing() groups.
 * IMPORTANT: Keep this list sorted alphabetically.
 */
static const struct option timing_type_opts[] = {
	{ "clear",                    LIB_TMG_TYPE_CLEAR                        },
	{ "combinational",            LIB_TMG_TYPE_COMB     | LIB_TMG_EDGE_BOTH },
	{ "combinational_fall",       LIB_TMG_TYPE_COMB     | LIB_TMG_EDGE_FALL },
	{ "combinational_rise",       LIB_TMG_TYPE_COMB     | LIB_TMG_EDGE_RISE },
	{ "falling_edge",             LIB_TMG_TYPE_EDGE     | LIB_TMG_EDGE_FALL },
	{ "hold_falling",             LIB_TMG_TYPE_HOLD     | LIB_TMG_EDGE_FALL },
	{ "hold_rising",              LIB_TMG_TYPE_HOLD     | LIB_TMG_EDGE_RISE },
	{ "max_clock_tree_path",      LIB_TMG_TYPE_MAX_CLK_TREE_PATH            },
	{ "min_clock_tree_path",      LIB_TMG_TYPE_MIN_CLK_TREE_PATH            },
	{ "min_pulse_width",          LIB_TMG_TYPE_MIN_PULSE_WIDTH              },
	{ "minimum_period",           LIB_TMG_TYPE_MIN_PERIOD                   },
	{ "preset",                   LIB_TMG_TYPE_PRESET                       },
	{ "recovery_falling",         LIB_TMG_TYPE_RECOVERY | LIB_TMG_EDGE_FALL },
	{ "recovery_rising",          LIB_TMG_TYPE_RECOVERY | LIB_TMG_EDGE_RISE },
	{ "removal_falling",          LIB_TMG_TYPE_REMOVAL  | LIB_TMG_EDGE_FALL },
	{ "removal_rising",           LIB_TMG_TYPE_REMOVAL  | LIB_TMG_EDGE_RISE },
	{ "rising_edge",              LIB_TMG_TYPE_EDGE     | LIB_TMG_EDGE_RISE },
	{ "setup_falling",            LIB_TMG_TYPE_SETUP    | LIB_TMG_EDGE_FALL },
	{ "setup_rising",             LIB_TMG_TYPE_SETUP    | LIB_TMG_EDGE_RISE },
	{ "skew_falling",             LIB_TMG_TYPE_SKEW     | LIB_TMG_EDGE_FALL },
	{ "skew_rising",              LIB_TMG_TYPE_SKEW     | LIB_TMG_EDGE_RISE },
	{ "three_state_disable",      LIB_TMG_TYPE_TRI_DIS  | LIB_TMG_EDGE_BOTH },
	{ "three_state_disable_fall", LIB_TMG_TYPE_TRI_DIS  | LIB_TMG_EDGE_FALL },
	{ "three_state_disable_rise", LIB_TMG_TYPE_TRI_DIS  | LIB_TMG_EDGE_RISE },
	{ "three_state_enable",       LIB_TMG_TYPE_TRI_EN   | LIB_TMG_EDGE_BOTH },
	{ "three_state_enable_fall",  LIB_TMG_TYPE_TRI_EN   | LIB_TMG_EDGE_FALL },
	{ "three_state_enable_rise",  LIB_TMG_TYPE_TRI_EN   | LIB_TMG_EDGE_RISE },
};

static const struct option scalar_opts[] = {
	{ "fall_resistance", LIB_MODEL_RESISTANCE_FALL },
	{ "intrinsic_fall",  LIB_MODEL_INTRINSIC_FALL  },
	{ "intrinsic_rise",  LIB_MODEL_INTRINSIC_RISE  },
	{ "rise_resistance", LIB_MODEL_RESISTANCE_RISE },
};

static const struct option table_opts[] = {
	{ "cell_fall",        LIB_MODEL_CELL_FALL        },
	{ "cell_rise",        LIB_MODEL_CELL_RISE        },
	{ "fall_constraint",  LIB_MODEL_CONSTRAINT_FALL  },
	{ "fall_propagation", LIB_MODEL_PROPAGATION_FALL },
	{ "fall_transition",  LIB_MODEL_TRANSITION_FALL  },
	{ "rise_constraint",  LIB_MODEL_CONSTRAINT_RISE  },
	{ "rise_propagation", LIB_MODEL_PROPAGATION_RISE },
	{ "rise_transition",  LIB_MODEL_TRANSITION_RISE  },
};

static const struct option variable_opts[] = {
	{ "constrained_pin_transition", 1 },
	{ "input_net_transition", 1 },
	{ "output_net_length", 1 },
	{ "output_net_pin_cap", 1 },
	{ "output_net_wire_cap", 1 },
	{ "related_out_output_net_length", 1 },
	{ "related_out_output_net_pin_cap", 1 },
	{ "related_out_output_net_wire_cap", 1 },
	{ "related_out_total_output_net_capacitance", 1 },
	{ "related_pin_transition", 1 },
	{ "total_output_net_capacitance", 1 },
};


static int
parse_real_fields(char *str, array_t *into) {
	while (*str) {
		// Skip whitespace before the value.
		while (isspace(*str) || *str == '\\')
			++str;

		// Parse the value.
		errno = 0;
		const char *base = str;
		double v = strtod(str, &str);
		if (errno != 0) {
			fprintf(stderr, "'%s' is not a valid real number; %s\n", base, strerror(errno));
			return LIB_ERR_SYNTAX;
		}
		if (base == str) {
			fprintf(stderr, "'%s' is not a valid real number\n", base);
			return LIB_ERR_SYNTAX;
		}
		array_add(into, &v);

		// Skip whitespace after the value.
		while (*str && (isspace(*str) || *str == '\\'))
			++str;

		// Ensure there is a comma after the value.
		if (*str) {
			if (*str != ',') {
				fprintf(stderr, "Expected a comma after the value '%s'\n", base);
				return LIB_ERR_SYNTAX;
			}
			++str;
		}
	}

	return LIB_OK;
}


static int
stmt_table_scalar(lib_parser_t *parser, void *arg, enum stmt_kind kind, char *name, char **params, unsigned num_params) {
	int err;
	assert(arg);

	if (kind == STMT_CATTR && strcmp(name, "values") == 0) {
		if (num_params != 1) {
			fprintf(stderr, "Values statement in scalar table must have exactly one value\n");
			return LIB_ERR_SYNTAX;
		}
		err = parse_real(params[0], arg);
		if (err != LIB_OK) {
			fprintf(stderr, "  in table value\n");
		}
		return err;
	} else {
		fprintf(stderr, "Only single values(\"...\"); statement allowed in scalar tables, but got %s\n", name);
		return LIB_ERR_SYNTAX;
	}
}


struct table_template {
	lib_table_format_t fmt;
	unsigned num_values;
	char **values;
};


static int
stmt_table_format(lib_parser_t *parser, void *arg, enum stmt_kind kind, char *name, char **params, unsigned num_params) {
	int err = LIB_OK;
	lib_table_format_t *fmt = arg;
	assert(fmt);

	if (kind == STMT_CATTR && strncmp(name, "index_", 6) == 0) {
		unsigned idx;
		if (num_params != 1) {
			fprintf(stderr, "Index attribute must have exactly one parameter\n");
			fprintf(stderr, "  in %s\n", name);
			return LIB_ERR_SYNTAX;
		}
		err = parse_int(name+6, &idx);
		if (err != LIB_OK) {
			fprintf(stderr, " in %s\n", name);
			return LIB_ERR_SYNTAX;
		}
		--idx;
		if (idx >= 3) {
			fprintf(stderr, "Index number must be between 1 and 3, got %u instead\n", idx+1);
			fprintf(stderr, "  in %s\n", name);
			return LIB_ERR_SYNTAX;
		}

		array_t indices;
		array_init(&indices, sizeof(double));
		err = parse_real_fields(params[0], &indices);
		array_shrink(&indices);

		if (err != LIB_OK) {
			fprintf(stderr, "  in %s\n", name);
			array_dispose(&indices);
			return LIB_ERR_SYNTAX;
		}

		if (fmt->indices[idx])
			free(fmt->indices[idx]);
		fmt->num_indices[idx] = indices.size;
		fmt->indices[idx] = indices.items;
		return LIB_OK;
	}

	else if (kind == STMT_SATTR && strncmp(name, "variable_", 9) == 0) {
		unsigned idx;
		if (num_params != 1) {
			fprintf(stderr, "Variable attribute must have exactly one parameter\n");
			fprintf(stderr, "  in %s\n", name);
			return LIB_ERR_SYNTAX;
		}
		err = parse_int(name+9, &idx);
		if (err != LIB_OK) {
			fprintf(stderr, " in %s\n", name);
			return LIB_ERR_SYNTAX;
		}
		--idx;
		if (idx >= 3) {
			fprintf(stderr, "Variable index must be between 1 and 3, got %u instead\n", idx+1);
			fprintf(stderr, "  in %s\n", name);
			return LIB_ERR_SYNTAX;
		}

		struct option *opt = bsearch(&params[0], variable_opts, ASIZE(variable_opts), sizeof(struct option), compare_options);
		if (!opt) {
			fprintf(stderr, "'%s' is not a valid table variable\n", params[0]);
			return LIB_ERR_SYNTAX;
		}
		fmt->variables[idx] = opt->value;
		return LIB_OK;
	}

	return kind == STMT_GRP ? parse_stmts(parser, NULL, NULL) : LIB_OK;
}


static int
stmt_table(lib_parser_t *parser, void *arg, enum stmt_kind kind, char *name, char **params, unsigned num_params) {
	struct table_template *tmpl = arg;
	assert(tmpl);

	if (kind == STMT_CATTR && strcmp(name, "values") == 0) {
		if (num_params == 0) {
			fprintf(stderr, "Table must contain at least one group of values\n");
			return LIB_ERR_SYNTAX;
		}
		if (tmpl->values) {
			fprintf(stderr, "Values for table defined multiple times\n");
			return LIB_ERR_SYNTAX;
		}
		tmpl->num_values = num_params;
		tmpl->values = malloc(num_params * sizeof(char*));
		for (unsigned u = 0; u < num_params; ++u) {
			tmpl->values[u] = dupstr(params[u]);
		}
		return LIB_OK;
	} else {
		return stmt_table_format(parser, &tmpl->fmt, kind, name, params, num_params);
	}
}


static void
dump_format(lib_table_format_t *fmt) {
	assert(fmt);
	printf("fmt { vars = {%u, %u, %u}, num_indices = {%u, %u, %u} }\n",
		fmt->variables[0],
		fmt->variables[1],
		fmt->variables[2],
		fmt->num_indices[0],
		fmt->num_indices[1],
		fmt->num_indices[2]
	);
}


static int
stmt_timing(lib_parser_t *parser, void *arg, enum stmt_kind kind, char *name, char **params, unsigned num_params) {
	int err;
	lib_timing_t *tmg = arg;
	assert(tmg);

	if (kind == STMT_SATTR) {
		if (strcmp(name, "related_pin") == 0) {
			char *str = dupstr(params[0]);
			array_add(&tmg->related_pins, &str);
			return LIB_OK;
		}

		if (strcmp(name, "timing_sense") == 0) {
			struct option *opt = bsearch(&params[0], timing_sense_opts, ASIZE(timing_sense_opts), sizeof(struct option), compare_options);
			// if (!opt) {
			// 	fprintf(stderr, "Unknown timing sense '%s'\n", params[0]);
			// 	return LIB_ERR_SYNTAX;
			// }
			if (opt)
				tmg->timing_sense = opt->value;
			return LIB_OK;
		}

		if (strcmp(name, "timing_type") == 0) {
			struct option *opt = bsearch(&params[0], timing_type_opts, ASIZE(timing_type_opts), sizeof(struct option), compare_options);
			// if (!opt) {
			// 	fprintf(stderr, "Unknown timing type '%s'\n", params[0]);
			// 	return LIB_ERR_SYNTAX;
			// }
			if (opt)
				tmg->timing_type = opt->value;
			return LIB_OK;
		}

		// Model Parameters
		struct option *opt = bsearch(&name, scalar_opts, ASIZE(scalar_opts), sizeof(struct option), compare_options);
		if (opt) {
			double *ptr = &tmg->scalars[opt->value & LIB_MODEL_INDEX_MASK];
			err = parse_real(params[0], ptr);
			if (err != LIB_OK) {
				fprintf(stderr, "  in %s parameter value\n", opt->str);
			}
			if (opt->value == LIB_MODEL_RESISTANCE_RISE || opt->value == LIB_MODEL_RESISTANCE_FALL)
				// *ptr *= parser->lib->resistance_unit;
				; /// @todo Implement resistance unit.
			else
				*ptr *= parser->lib->time_unit;
			return err;
		}
	}

	else if (kind == STMT_CATTR) {
		/// @todo Implement "mode()" statements.
	}

	else if (kind == STMT_GRP) {
		// Model Parameters
		struct option *opt = bsearch(&name, table_opts, ASIZE(table_opts), sizeof(struct option), compare_options);
		if (opt) {
			if (num_params != 1) {
				fprintf(stderr, "Expected lookup table template name\n");
				fprintf(stderr, "  as parameter to %s table\n", name);
				return LIB_ERR_SYNTAX;
			}

			// Treat "scalar" tables just like regular scalars.
			if (strcmp(params[0], "scalar") == 0) {
				double *ptr = &tmg->scalars[opt->value & LIB_MODEL_INDEX_MASK];
				err = parse_stmts(parser, stmt_table_scalar, ptr);
				if (err != LIB_OK) {
					fprintf(stderr, "  in %s table\n", opt->str);
				}
				return err;
			}

			// Actual tables are first parsed into a table template, from which
			// the final table is assembled at the end of the group. This
			// ensures that template parameters can be properly overridden.
			else {
				struct table_template tmpl;
				memset(&tmpl, 0, sizeof(tmpl));
				lib_table_format_init(&tmpl.fmt);

				// Populate the template with the corresponding lookup table
				// template specified in the file.
				/// @todo Find lookup table template.
				lib_table_format_t *fmt = lib_find_lut_template(parser->lib, params[0]);
				if (!fmt) {
					fprintf(stderr, "Unknown lookup table template '%s'\n", params[0]);
					err = LIB_ERR_SYNTAX;
					goto fail_tmpl;
				}
				lib_table_format_copy(&tmpl.fmt, fmt);

				// Parse the table body.
				err = parse_stmts(parser, stmt_table, &tmpl);
				if (err != LIB_OK) {
					fprintf(stderr, "  in %s table\n", opt->str);
					goto fail_tmpl;
				}

				// Ensure the table template contains enough information to
				// allocate storage for the final table.
				if (tmpl.fmt.indices[0] == LIB_IDX_NONE) {
					fprintf(stderr, "Table %s must have at least one axis\n", name);
					err = LIB_ERR_SYNTAX;
					goto fail_tmpl;
				}
				unsigned num_idx;
				for (unsigned u = 1; u < ASIZE(tmpl.fmt.indices); ++u) {
					num_idx = u;
					if (tmpl.fmt.indices[u] && !tmpl.fmt.indices[u-1]) {
						fprintf(stderr, "Table %s cannot have index %u set while index %u is left undefined\n", name, u+1, u);
						err = LIB_ERR_SYNTAX;
						goto fail_tmpl;
					}
				}

				// Assemble the final table.
				array_t values;
				array_init(&values, sizeof(double));
				for (unsigned u = 0; u < tmpl.num_values; ++u) {
					err = parse_real_fields(tmpl.values[u], &values);
					free(tmpl.values[u]);
					tmpl.values[u] = NULL;
					if (err != LIB_OK) {
						fprintf(stderr, "  in table '%s'\n", name);
						goto fail_values;
					}
				}

				// Calculate the strides of the table axis and verify that
				// enough values were provided.
				lib_table_t *tbl;
				err = lib_timing_add_table(tmg, opt->value, &tbl);
				if (err != LIB_OK) {
					fprintf(stderr, "Cannot add table '%s'\n", name);
					goto fail_values;
				}

				unsigned stride = 1;
				for (int i = ASIZE(tmpl.fmt.indices)-1; i >= 0; --i) {
					if (tmpl.fmt.variables[i] != LIB_IDX_NONE) {
						tbl->strides[i] = stride;
						stride *= tmpl.fmt.num_indices[i];
					}
				}
				if (stride != values.size) {
					fprintf(stderr, "Table '%s' requires %u values, but only %u provided\n", name, stride, values.size);
					err = LIB_ERR_SYNTAX;
					goto fail_values;
				}

				array_shrink(&values);
				tbl->fmt = tmpl.fmt;
				tbl->num_values = values.size;
				tbl->values = values.items;
				for (unsigned u = 0; u < tbl->num_values; ++u) {
					tbl->values[u] *= parser->lib->time_unit;
				}
				return LIB_OK;

				// Failure Path
			fail_values:
				array_dispose(&values);
			fail_tmpl:
				for (unsigned u = 0; u < tmpl.num_values; ++u) {
					if (tmpl.values[u])
						free(tmpl.values[u]);
				}
				lib_table_format_dispose(&tmpl.fmt);
				return err;
			}
		}
	}

	return kind == STMT_GRP ? parse_stmts(parser, NULL, NULL) : LIB_OK;
}


static int
stmt_pin(lib_parser_t *parser, void *arg, enum stmt_kind kind, char *name, char **params, unsigned num_params) {
	int err;
	lib_pin_t *pin = arg;
	assert(pin);

	if (kind == STMT_SATTR) {
		if (strcmp(name, "direction") == 0) {
			if (strcmp(params[0], "input") == 0)
				pin->direction = LIB_PIN_IN;
			else if (strcmp(params[0], "output") == 0)
				pin->direction = LIB_PIN_OUT;
			else if (strcmp(params[0], "inout") == 0)
				pin->direction = LIB_PIN_INOUT;
			else if (strcmp(params[0], "internal") == 0)
				pin->direction = LIB_PIN_INTERNAL;
			else {
				fprintf(stderr, "Unknown pin direction '%s'\n", params[0]);
				return LIB_ERR_SYNTAX;
			}
			return LIB_OK;
		}

		if (strcmp(name, "capacitance") == 0) {
			err = parse_real(params[0], &pin->capacitance);
			if (err != LIB_OK) {
				fprintf(stderr, "  in capacitance value\n");
			}
			pin->capacitance *= parser->lib->capacitance_unit;
			return err;
		}
	}

	else if (kind == STMT_GRP) {
		if (strcmp(name, "timing") == 0) {
			if (num_params != 0) {
				fprintf(stderr, "Timing group does not take any arguments\n");
				return LIB_ERR_SYNTAX;
			}
			lib_timing_t *tmg = lib_pin_add_timing(pin);
			err = parse_stmts(parser, stmt_timing, tmg);
			if (err != LIB_OK) {
				fprintf(stderr, "  in timing group\n");
			}
			return err;
		}
	}

	return kind == STMT_GRP ? parse_stmts(parser, NULL, NULL) : LIB_OK;
}


static int
stmt_cell(lib_parser_t *parser, void *arg, enum stmt_kind kind, char *name, char **params, unsigned num_params) {
	int err;
	lib_cell_t *cell = arg;
	assert(cell);

	if (kind == STMT_GRP && strcmp(name, "pin") == 0) {
		if (num_params != 1) {
			fprintf(stderr, "Expected 1 argument parentheses (pin name), but got %d\n", num_params);
			return LIB_ERR_SYNTAX;
		}
		lib_pin_t *pin;
		err = lib_cell_add_pin(cell, params[0], &pin);
		if (err != LIB_OK) {
			fprintf(stderr, "Cannot declare pin '%s'\n", params[0]);
			return err;
		}
		parser->pin = pin;
		err = parse_stmts(parser, stmt_pin, pin);
		if (err != LIB_OK) {
			fprintf(stderr, "  in pin '%s'\n", pin->name);
		}
		parser->pin = NULL;
		return err;
	}

	else if (kind == STMT_SATTR) {
		if (strcmp(name, "cell_leakage_power") == 0) {
			err = parse_real(params[0], &cell->leakage_power);
			cell->leakage_power *= parser->lib->leakage_power_unit;
			if (err != LIB_OK) {
				fprintf(stderr, "  in leakage power value\n");
			}
			return err;
		}
	}

	return kind == STMT_GRP ? parse_stmts(parser, NULL, NULL) : LIB_OK;
}


static int
stmt_library(lib_parser_t *parser, void *arg, enum stmt_kind kind, char *name, char **params, unsigned num_params) {
	int err;
	lib_t *lib = arg;
	assert(lib);

	// Groups
	if (kind == STMT_GRP) {
		if (strcmp(name, "cell") == 0) {
			if (num_params != 1) {
				fprintf(stderr, "Cell must have a name\n");
				return LIB_ERR_SYNTAX;
			}
			lib_cell_t *cell;
			err = lib_add_cell(lib, params[0], &cell);
			if (err != LIB_OK) {
				fprintf(stderr, "Cannot declare cell '%s'\n", params[0]);
				return err;
			}
			parser->cell = cell;
			err = parse_stmts(parser, stmt_cell, cell);
			if (err != LIB_OK) {
				fprintf(stderr, "  in cell '%s'\n", cell->name);
			}
			parser->cell = NULL;
			return err;
		}

		if (strcmp(name, "lu_table_template") == 0) {
			if (num_params != 1) {
				fprintf(stderr, "Table template must have a name\n");
				return LIB_ERR_SYNTAX;
			}
			lib_table_format_t *fmt;
			err = lib_add_lut_template(lib, params[0], &fmt);
			if (err != LIB_OK) {
				fprintf(stderr, "Cannot declare table format '%s'\n", params[0]);
				return err;
			}
			err = parse_stmts(parser, stmt_table_format, fmt);
			if (err != LIB_OK) {
				fprintf(stderr, "  in table template '%s'\n", params[0]);
			}
			return err;
		}
	}

	// Simple Attributes
	else if (kind == STMT_SATTR) {
		// Units
		static const struct {
			const char *name;
			const char *human_name;
			size_t offset;
		} units[] = {
			{ "time_unit", "time unit", offsetof(lib_t, time_unit) },
			{ "voltage_unit", "voltage unit", offsetof(lib_t, voltage_unit) },
			{ "current_unit", "current unit", offsetof(lib_t, current_unit) },
			{ "leakage_power_unit", "leakage power unit", offsetof(lib_t, leakage_power_unit) },
		};
		for (unsigned u = 0; u < ASIZE(units); ++u) {
			if (strcmp(name, units[u].name) == 0) {
				double *ptr = (void*)lib + units[u].offset;
				err = parse_real(params[0], ptr);
				if (err != LIB_OK) {
					fprintf(stderr, "  in %s\n", units[u].human_name);
				}
				return err;
			}
		}
	}

	// Complex Attributes
	else if (kind == STMT_CATTR) {
		if (strcmp(name, "capacitive_load_unit") == 0) {
			if (num_params != 2) {
				fprintf(stderr, "Expected scale and SI prefix in capacitive load unit\n");
				return LIB_ERR_SYNTAX;
			}
			err = parse_real(params[0], &lib->capacitance_unit);
			if (err != LIB_OK) {
				fprintf(stderr, "  in capacitive load unit\n");
				return err;
			}
			lib->capacitance_unit *= si_prefix_scale(params[1][0]);
		}
	}

	return kind == STMT_GRP ? parse_stmts(parser, NULL, NULL) : LIB_OK;
}


static int
stmt_root(lib_parser_t *parser, void *arg, enum stmt_kind kind, char *name, char **params, unsigned num_params) {
	int err;
	lib_t **lib = arg;
	assert(lib);

	if (strcmp(name, "library") == 0) {
		*lib = lib_new(params[0]);
		parser->lib = *lib;
		err = parse_stmts(parser, stmt_library, *lib);
		if (err != LIB_OK) {
			fprintf(stderr, "  in library '%s'\n", (*lib)->name);
		}
		parser->lib = NULL;
		return err;
	}

	return kind == STMT_GRP ? parse_stmts(parser, NULL, NULL) : LIB_OK;
}


/**
 * Parses an entire LIB file.
 */
int
lib_parse(lib_lexer_t *lex, lib_t **lib) {
	assert(lex && lib);
	int err = LIB_OK;
	lib_parser_t parser;

	// Prepare the parser which provides access to the lexer as well as a buffer
	// for lexed tokens.
	memset(&parser, 0, sizeof(parser));
	parser.lexer = lex;
	parser.params_cap = 32;
	parser.params = malloc(sizeof(char**) * parser.params_cap);

	err = parse_stmts(&parser, stmt_root, lib);

finish:
	free(parser.params);
	return err;
}
