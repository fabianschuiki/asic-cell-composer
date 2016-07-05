/* Copyright (c) 2016 Fabian Schuiki */
#pragma once
#include "lib.h"
#include "util.h"

typedef struct lib_lexer lib_lexer_t;

enum lib_token {
	LIB_EOF = 0,
	LIB_LPAREN,
	LIB_RPAREN,
	LIB_LBRACE,
	LIB_RBRACE,
	LIB_COLON,
	LIB_SEMICOLON,
	LIB_COMMA,
	LIB_IDENT,
};

struct lib_lexer {
	char *pos, *end;
	enum lib_token tkn;
	char *tkn_base, *tkn_end;
	int line, column;
	char *text;
	size_t text_cap;
};

struct lib {
	char *name;
	array_t cells; /* cell_t* */
};

struct lib_cell {
	lib_t *lib;
	char *name;
	array_t pins; /* phx_pin_t* */
};

struct lib_pin {
	lib_cell_t *cell;
	char *name;
	uint8_t direction;
	double capacitance;
};

void lib_lexer_init(lib_lexer_t *lex, void *ptr, size_t len);
void lib_lexer_dispose(lib_lexer_t *lex);
int lib_lexer_next(lib_lexer_t *lex);

int lib_parse(lib_lexer_t *lex, lib_t **lib);
