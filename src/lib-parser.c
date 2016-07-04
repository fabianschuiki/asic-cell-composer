/* Copyright (c) 2016 Fabian Schuiki */
#include "lib-internal.h"
#include "util.h"


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


static int
parse_real(const char *str, double *out) {
	errno = 0;
	*out = strtod(str, NULL);
	if (errno != 0) {
		fprintf(stderr, "'%s' is not a valid real number; %s\n", str, strerror(errno));
		return LIB_ERR_SYNTAX;
	}
	return LIB_OK;
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
			for (unsigned u = 0; u < num_params; ++u)
				fprintf(stderr, "  Parameter #%d: '%s'\n", u+1, params[u]);
			return LIB_ERR_SYNTAX;
		}
		lib_pin_t *pin;
		err = lib_cell_add_pin(cell, params[0], &pin);
		if (err != LIB_OK) {
			fprintf(stderr, "Cannot declare pin '%s'\n", params[0]);
			return err;
		}
		err = parse_stmts(parser, stmt_pin, pin);
		if (err != LIB_OK) {
			fprintf(stderr, "  in pin '%s'\n", pin->name);
		}
		return err;
	}

	return kind == STMT_GRP ? parse_stmts(parser, NULL, NULL) : LIB_OK;
}


static int
stmt_library(lib_parser_t *parser, void *arg, enum stmt_kind kind, char *name, char **params, unsigned num_params) {
	int err;
	lib_t *lib = arg;
	assert(lib);

	if (strcmp(name, "cell") == 0) {
		lib_cell_t *cell;
		err = lib_add_cell(lib, params[0], &cell);
		if (err != LIB_OK) {
			fprintf(stderr, "Cannot declare cell '%s'\n", params[0]);
			return err;
		}
		err = parse_stmts(parser, stmt_cell, cell);
		if (err != LIB_OK) {
			fprintf(stderr, "  in cell '%s'\n", cell->name);
		}
		return err;
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
		err = parse_stmts(parser, stmt_library, *lib);
		if (err != LIB_OK) {
			fprintf(stderr, "  in library '%s'\n", (*lib)->name);
		}
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
