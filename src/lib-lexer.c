/* Copyright (c) 2016 Fabian Schuiki */
#include "lib-internal.h"


void
lib_lexer_init(lib_lexer_t *lex, void *ptr, size_t len) {
	assert(lex);
	memset(lex, 0, sizeof(*lex));
	lex->pos = ptr;
	lex->end = ptr+len;

	// Initialize the text buffer.
	lex->text_cap = 128;
	lex->text = malloc(lex->text_cap);

	// Prime the lexer.
	lib_lexer_next(lex);
}


void
lib_lexer_dispose(lib_lexer_t *lex) {
	assert(lex);
	free(lex->text);
	memset(lex, 0, sizeof(*lex));
}


/**
 * Advances the lexer to the next character, keeping track of the location
 * within the file.
 */
static void
step(lib_lexer_t *lex) {
	if (*lex->pos++ == '\n') {
		++lex->line;
		lex->column = 0;
	} else {
		++lex->column;
	}
}


static int
is_whitespace(char c) {
	return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\\';
}


static int
is_identifier(char c) {
	return c >= 0x21 && c <= 0x7E &&
	       c != '(' && c != ')' && c != '{' && c != '}' &&
	       c != ':' && c != ';' && c != ',';
}


static void
store_token_text(lib_lexer_t *lex) {
	size_t len = lex->tkn_end - lex->tkn_base;
	if (lex->text_cap < len+1) {
		lex->text_cap = len+1;
		lex->text = realloc(lex->text, lex->text_cap);
	}
	memcpy(lex->text, lex->tkn_base, len);
	lex->text[len] = 0;
}


/**
 * Advances the lexer to the next token.
 */
int
lib_lexer_next(lib_lexer_t *lex) {
	assert(lex);
	char last;

relex:
	// Skip whitespace characters.
	while (lex->pos < lex->end && is_whitespace(*lex->pos)) {
		step(lex);
	}

	// Skip comments.
	if (lex->pos+1 < lex->end && lex->pos[0] == '/' && lex->pos[1] == '*') {
		step(lex);
		step(lex);
		last = 0;
		while (lex->pos < lex->end && !(last == '*' && *lex->pos == '/')) {
			last = *lex->pos;
			step(lex);
		}
		if (lex->pos == lex->end) {
			fprintf(stderr, "Unexepcted end of file within comment\n");
			lex->tkn = LIB_EOF;
			return LIB_ERR_SYNTAX;
		}
		if (lex->pos < lex->end)
			step(lex);
		goto relex;
	}

	// Reset the token in the lexer.
	lex->tkn_base = lex->pos;
	lex->tkn_end = lex->pos;
	lex->text[0] = 0; // clear the text buffer

	// Do nothing if the end of the source file has been reached.
	if (lex->pos == lex->end) {
		lex->tkn = LIB_EOF;
		return LIB_OK;
	}

	// Symbols
	int tkn = 0;
	switch (*lex->pos) {
		case '(': tkn = LIB_LPAREN; break;
		case ')': tkn = LIB_RPAREN; break;
		case '{': tkn = LIB_LBRACE; break;
		case '}': tkn = LIB_RBRACE; break;
		case ':': tkn = LIB_COLON; break;
		case ';': tkn = LIB_SEMICOLON; break;
		case ',': tkn = LIB_COMMA; break;
	}
	if (tkn) {
		step(lex);
		lex->tkn = tkn;
		lex->tkn_end = lex->pos;
		store_token_text(lex);
		return LIB_OK;
	}

	// Strings in quotation marks
	if (*lex->pos == '"') {
		step(lex);
		lex->tkn = LIB_IDENT;
		lex->tkn_base = lex->pos;
		last = 0;
		while (lex->pos < lex->end && (*lex->pos != '"' || last == '\\')) {
			last = *lex->pos;
			step(lex);
		}
		if (lex->pos == lex->end) {
			fprintf(stderr, "Unexpected end of file within string literal\n");
			lex->tkn = LIB_EOF;
			return LIB_ERR_SYNTAX;
		}
		lex->tkn_end = lex->pos;
		step(lex);
		store_token_text(lex);
		return LIB_OK;
	}

	// Regular identifiers
	if (is_identifier(*lex->pos)) {
		step(lex);
		lex->tkn = LIB_IDENT;
		while (lex->pos < lex->end && is_identifier(*lex->pos)) {
			step(lex);
		}
		lex->tkn_end = lex->pos;
		store_token_text(lex);
		return LIB_OK;
	}

	fprintf(stderr, "Invalid character '%c' 0x%02x\n", *lex->pos, *lex->pos);
	return LIB_ERR_SYNTAX;
}
