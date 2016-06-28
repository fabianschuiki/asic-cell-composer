/* Copyright (c) 2016 Fabian Schuiki */
#include "lef.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/**
 * @file
 * @author Fabian Schuiki <fschuiki@student.ethz.ch>
 *
 * Facilities for reading Library Exchange Format (LEF) files. Based on the
 * LEF/DEF 5.8 Language Reference.
 */


enum lef_token {
	LEF_EOF = 0,

	// Symbols
	LEF_LPAREN,
	LEF_RPAREN,
	LEF_SEMICOLON,

	// Strings
	LEF_IDENT,
	LEF_STRING,
	LEF_KW_BUSBITCHARS,
	LEF_KW_BY,
	LEF_KW_DIVIDERCHAR,
	LEF_KW_END,
	LEF_KW_LIBRARY,
	LEF_KW_MACRO,
	LEF_KW_NAMESCASESENSITIVE,
	LEF_KW_OBS,
	LEF_KW_OFF,
	LEF_KW_ON,
	LEF_KW_ORIGIN,
	LEF_KW_PIN,
	LEF_KW_PROPERTYDEFINITIONS,
	LEF_KW_R90,
	LEF_KW_SITE,
	LEF_KW_SIZE,
	LEF_KW_SYMMETRY,
	LEF_KW_VERSION,
	LEF_KW_X,
	LEF_KW_Y,
	LEF_KW_PORT,
	LEF_KW_NONE,
	LEF_KW_CORE,
	LEF_KW_BUMP,
	LEF_KW_LAYER,
	LEF_KW_VIA,
	LEF_KW_CLASS,
	LEF_KW_WIDTH,
	LEF_KW_PATH,
	LEF_KW_RECT,
	LEF_KW_POLYGON,
};

struct lef_kw {
	const char *str;
	enum lef_token tkn;
};

struct lef_lexer {
	unsigned line, column;
	char *pos, *end;    // position and end of the input data
	char *tbase, *tend; // token start and end
	enum lef_token tkn; // lexed token
	char *text;
	size_t text_cap;
	int ncs;
};


/**
 * Create an empty LEF file.
 */
lef_t *
lef_new() {
	lef_t *lef;
	lef = calloc(1, sizeof(*lef));
	array_init(&lef->macros, sizeof(lef_macro_t*));
	// array_init(&lef->sites, sizeof(lef_site_t*));
	return lef;
}

/**
 * Destroy a LEF file.
 */
void
lef_free(lef_t *lef) {
	size_t z;
	assert(lef);
	for (z = 0; z < lef->macros.size; ++z) {
		lef_free_macro(array_at(lef->macros, lef_macro_t*, z));
	}
	array_dispose(&lef->macros);
	// array_dispose(&lef->sites);
	free(lef);
}

/**
 * Get the number of macros a LEF file.
 */
size_t
lef_get_num_macros(lef_t *lef) {
	assert(lef);
	return lef->macros.size;
}

/**
 * Access a macro in a LEF file.
 */
lef_macro_t *
lef_get_macro(lef_t *lef, size_t idx) {
	assert(lef && idx < lef->macros.size);
	return array_at(lef->macros, lef_macro_t*, idx);
}

/**
 * Add a macro to a LEF file.
 */
void
lef_add_macro(lef_t *lef, lef_macro_t *macro) {
	assert(lef && macro);
	array_add(&lef->macros, &macro);
}


/**
 * Create an empty macro with a given name.
 */
lef_macro_t *
lef_new_macro(const char *name) {
	lef_macro_t *macro;
	assert(name);
	macro = calloc(1, sizeof(*macro));
	macro->name = dupstr(name);
	array_init(&macro->pins, sizeof(lef_pin_t*));
	array_init(&macro->obs, sizeof(lef_geo_t*));
	return macro;
}

/**
 * Destroy a macro.
 */
void
lef_free_macro(lef_macro_t *macro) {
	size_t z;
	assert(macro);
	for (z = 0; z < macro->pins.size; ++z) {
		lef_free_pin(array_at(macro->pins, lef_pin_t*, z));
	}
	if (macro->name) {
		free(macro->name);
	}
	array_dispose(&macro->pins);
	array_dispose(&macro->obs);
	free(macro);
}

/**
 * Add a pin to a macro.
 */
void
lef_macro_add_pin(lef_macro_t *macro, lef_pin_t *pin) {
	assert(macro && pin);
	array_add(&macro->pins, &pin);
}

/**
 * Add an obstruction to a macro.
 */
void
lef_macro_add_obs(lef_macro_t *macro, lef_geo_t *obs) {
	assert(macro && obs);
	array_add(&macro->obs, &obs);
}

/**
 * Get the name of a macro.
 */
const char *
lef_macro_get_name(lef_macro_t *macro) {
	assert(macro);
	return macro->name;
}

/**
 * Get the size of a macro.
 */
lef_xy_t
lef_macro_get_size(lef_macro_t *macro) {
	assert(macro);
	return macro->size;
}

size_t
lef_macro_get_num_pins(lef_macro_t *macro) {
	assert(macro);
	return macro->pins.size;
}

lef_pin_t *
lef_macro_get_pin(lef_macro_t *macro, size_t idx) {
	assert(macro && idx < macro->pins.size);
	return array_at(macro->pins, lef_pin_t*, idx);
}



/**
 * Create a new pin with a given name.
 */
struct lef_pin *
lef_new_pin(const char *name) {
	struct lef_pin *pin;
	assert(name);
	pin = calloc(1, sizeof(*pin));
	pin->name = dupstr(name);
	array_init(&pin->ports, sizeof(lef_port_t*));
	return pin;
}

/**
 * Destroy a pin.
 */
void
lef_free_pin(struct lef_pin *pin) {
	size_t z;
	assert(pin);
	for (z = 0; z < pin->ports.size; ++z) {
		lef_free_port(array_at(pin->ports, lef_port_t*, z));
	}
	if (pin->name) free(pin->name);
	if (pin->must_join) free(pin->must_join);
	array_dispose(&pin->ports);
	free(pin);
}

/**
 * Add a port to a pin.
 */
void
lef_pin_add_port(lef_pin_t *pin, lef_port_t *port) {
	assert(pin && port);
	array_add(&pin->ports, &port);
}

size_t
lef_pin_get_num_ports(lef_pin_t *pin) {
	assert(pin);
	return pin->ports.size;
}

lef_port_t *
lef_pin_get_port(lef_pin_t *pin, size_t idx) {
	assert(pin && idx < pin->ports.size);
	return array_at(pin->ports, lef_port_t*, idx);
}

const char *
lef_pin_get_name(lef_pin_t *pin) {
	assert(pin);
	return pin->name;
}



/**
 * Creates a new port.
 */
lef_port_t *
lef_new_port() {
	lef_port_t *port;
	port = calloc(1, sizeof(*port));
	array_init(&port->geos, sizeof(lef_geo_t*));
	return port;
}

void
lef_free_port(lef_port_t *port) {
	size_t z;
	assert(port);
	for (z = 0; z < port->geos.size; ++z) {
		// lef_free_geo(array_at(port->geos, lef_geo_t*, z));
	}
	array_dispose(&port->geos);
	free(port);
}

enum lef_port_class
lef_port_get_class(lef_port_t *port) {
	assert(port);
	return port->cls;
}

size_t
lef_port_get_num_geos(lef_port_t *port) {
	assert(port);
	return port->geos.size;
}

lef_geo_t *
lef_port_get_geo(lef_port_t *port, size_t idx) {
	assert(port && idx < port->geos.size);
	return array_at(port->geos, lef_geo_t*, idx);
}



/**
 * Create new layer geometry.
 */
lef_geo_layer_t *
lef_new_geo_layer() {
	lef_geo_layer_t *layer;
	layer = calloc(1, sizeof(*layer));
	layer->geo.kind = LEF_GEO_LAYER;
	array_init(&layer->shapes, sizeof(lef_geo_shape_t*));
	return layer;
}

/**
 * Destroy layer geometry.
 */
void
lef_free_geo_layer(lef_geo_layer_t *layer) {
	size_t z;
	assert(layer);
	for (z = 0; z < layer->shapes.size; ++z) {
		lef_free_geo_shape(array_at(layer->shapes, lef_geo_shape_t*, z));
	}
	array_dispose(&layer->shapes);
	if (layer->layer) free(layer->layer);
	free(layer);
}

/**
 * Add a shape to a layer geometry.
 */
void
lef_geo_layer_add_shape(lef_geo_layer_t *layer, lef_geo_shape_t *shape) {
	assert(layer && shape);
	array_add(&layer->shapes, &shape);
}

size_t
lef_geo_layer_get_num_shapes(lef_geo_layer_t *layer) {
	assert(layer);
	return layer->shapes.size;
}

lef_geo_shape_t *
lef_geo_layer_get_shape(lef_geo_layer_t *layer, size_t idx) {
	assert(layer && idx < layer->shapes.size);
	return array_at(layer->shapes, lef_geo_shape_t*, idx);
}

const char *
lef_geo_layer_get_name(lef_geo_layer_t *layer) {
	assert(layer);
	return layer->layer;
}



/**
 * Create a new layer geometry shape.
 */
struct lef_geo_shape *
lef_new_geo_shape(enum lef_geo_shape_kind kind, uint32_t num_points, struct lef_xy *points) {
	size_t sz;
	void *ptr;
	struct lef_geo_shape *shape;

	sz = sizeof(*shape) + num_points * sizeof(struct lef_xy);
	ptr = calloc(1, sz);

	shape = ptr;
	shape->kind = kind;
	shape->num_points = num_points;
	shape->points = ptr + sizeof(*shape);
	memcpy(shape->points, points, num_points * sizeof(struct lef_xy));

	return shape;
}

void
lef_free_geo_shape(lef_geo_shape_t *shape) {
	assert(shape);
	free(shape);
}

uint16_t
lef_geo_shape_get_num_points(lef_geo_shape_t *shape) {
	assert(shape);
	return shape->num_points;
}

lef_xy_t *
lef_geo_shape_get_points(lef_geo_shape_t *shape) {
	assert(shape);
	return shape->points;
}


// IMPORTANT: Keep the following list sorted alphabetically such that a binary
// search can be performed.
static struct lef_kw keywords[] = {
	{"BUMP", LEF_KW_BUMP},
	{"BUSBITCHARS", LEF_KW_BUSBITCHARS},
	{"BY", LEF_KW_BY},
	{"CLASS", LEF_KW_CLASS},
	{"CORE", LEF_KW_CORE},
	{"DIVIDERCHAR", LEF_KW_DIVIDERCHAR},
	{"END", LEF_KW_END},
	{"LAYER", LEF_KW_LAYER},
	{"LIBRARY", LEF_KW_LIBRARY},
	{"MACRO", LEF_KW_MACRO},
	{"NAMESCASESENSITIVE", LEF_KW_NAMESCASESENSITIVE},
	{"NONE", LEF_KW_NONE},
	{"OBS", LEF_KW_OBS},
	{"OFF", LEF_KW_OFF},
	{"ON", LEF_KW_ON},
	{"ORIGIN", LEF_KW_ORIGIN},
	{"PATH", LEF_KW_PATH},
	{"PIN", LEF_KW_PIN},
	{"POLYGON", LEF_KW_POLYGON},
	{"PORT", LEF_KW_PORT},
	{"PROPERTYDEFINITIONS", LEF_KW_PROPERTYDEFINITIONS},
	{"R90", LEF_KW_R90},
	{"RECT", LEF_KW_RECT},
	{"SITE", LEF_KW_SITE},
	{"SIZE", LEF_KW_SIZE},
	{"SYMMETRY", LEF_KW_SYMMETRY},
	{"VERSION", LEF_KW_VERSION},
	{"VIA", LEF_KW_VIA},
	{"WIDTH", LEF_KW_WIDTH},
	{"X", LEF_KW_X},
	{"Y", LEF_KW_Y},
};

static const char *token_names[] = {
	[LEF_EOF] = "end of file",

	// Symbols
	[LEF_LPAREN]    = "(",
	[LEF_RPAREN]    = ")",
	[LEF_SEMICOLON] = ";",

	// Strings
	[LEF_IDENT]                  = "identifier",
	[LEF_STRING]                 = "string",
	[LEF_KW_BUSBITCHARS]         = "BUSBITCHARS",
	[LEF_KW_BY]                  = "BY",
	[LEF_KW_DIVIDERCHAR]         = "DIVIDERCHAR",
	[LEF_KW_END]                 = "END",
	[LEF_KW_LIBRARY]             = "LIBRARY",
	[LEF_KW_MACRO]               = "MACRO",
	[LEF_KW_NAMESCASESENSITIVE]  = "NAMESCASESENSITIVE",
	[LEF_KW_OBS]                 = "OBS",
	[LEF_KW_OFF]                 = "OFF",
	[LEF_KW_ON]                  = "ON",
	[LEF_KW_ORIGIN]              = "ORIGIN",
	[LEF_KW_PIN]                 = "PIN",
	[LEF_KW_PROPERTYDEFINITIONS] = "PROPERTYDEFINITIONS",
	[LEF_KW_R90]                 = "R90",
	[LEF_KW_SITE]                = "SITE",
	[LEF_KW_SIZE]                = "SIZE",
	[LEF_KW_SYMMETRY]            = "SYMMETRY",
	[LEF_KW_VERSION]             = "VERSION",
	[LEF_KW_X]                   = "X",
	[LEF_KW_Y]                   = "Y",
	[LEF_KW_PORT]                = "PORT",
	[LEF_KW_NONE]                = "NONE",
	[LEF_KW_CORE]                = "CORE",
	[LEF_KW_BUMP]                = "BUMP",
	[LEF_KW_LAYER]               = "LAYER",
	[LEF_KW_VIA]                 = "VIA",
	[LEF_KW_CLASS]               = "CLASS",
	[LEF_KW_WIDTH]               = "WIDTH",
	[LEF_KW_PATH]                = "PATH",
	[LEF_KW_RECT]                = "RECT",
	[LEF_KW_POLYGON]             = "POLYGON",
};


static int
compare_key_and_keyword(const void *pa, const void *pb) {
	return strcasecmp(
		pa,
		((const struct lef_kw*)pb)->str
	);
}


static int
is_whitespace(char c) {
	return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static int
is_symbol(char c) {
	return c == '(' || c == ')' || c == ';';
}

static int
is_identifier(char c) {
	return c >= 0x21 && c <= 0x7E && !is_symbol(c);
}


static void
lex_copy_text(struct lef_lexer *lex) {
	size_t len = (lex->tend - lex->tbase);
	if (len >= lex->text_cap) {
		lex->text_cap = len+1;
		lex->text = realloc(lex->text, lex->text_cap);
	}
	memcpy(lex->text, lex->tbase, len);
	lex->text[len] = 0;
}


static void
lex_step(struct lef_lexer *lex) {
	if (*lex->pos == '\n') {
		++lex->line;
		lex->column = 0;
	} else {
		++lex->column;
	}
	++lex->pos;
}


/**
 * Advances the lexer to the next token.
 */
static void
lex_next(struct lef_lexer *lex) {
	assert(lex);

relex:
	// Skip whitespace.
	while (lex->pos < lex->end && is_whitespace(*lex->pos)) {
		lex_step(lex);
	}

	// No need to continue lexing if we've ran past the end of the file.
	if (lex->pos >= lex->end) {
		lex->tkn = LEF_EOF;
		return;
	}

	// Skip comments.
	if (*lex->pos == '#') {
		while (lex->pos < lex->end && *lex->pos != '\n') {
			lex_step(lex);
		}
		goto relex;
	}

	char c = *lex->pos;
	lex->tbase = lex->pos;
	lex->tend = lex->pos;
	lex->text[0] = 0;

	// Symbols
	int tkn = 0;
	switch (c) {
		case '(': tkn = LEF_LPAREN; break;
		case ')': tkn = LEF_LPAREN; break;
		case ';': tkn = LEF_SEMICOLON; break;
		default: break;
	}
	if (tkn) {
		lex->tkn = tkn;
		lex_step(lex);
		lex->tend = lex->pos;
		lex_copy_text(lex);
		return;
	}

	// Strings
	if (c == '"' || c == '\'') {
		char end = c;
		lex_step(lex);
		lex->tkn = LEF_STRING;
		lex->tbase = lex->pos;
		while (lex->pos < lex->end && *lex->pos != end)
			lex_step(lex);
		lex->tend = lex->pos;
		if (lex->pos < lex->end && *lex->pos == end)
			lex_step(lex);
		lex_copy_text(lex);
		return;
	}

	// Identifiers
	if (is_identifier(c)) {
		struct lef_kw *kw;

		lex->tkn = LEF_IDENT;
		c = *(++lex->pos);
		while (is_identifier(c))
			c = *(++lex->pos);
		lex->tend = lex->pos;
		lex_copy_text(lex);

		kw = bsearch(lex->text, keywords, ASIZE(keywords), sizeof(*keywords), compare_key_and_keyword);
		if (kw) lex->tkn = kw->tkn;

		return;
	}

	fprintf(stderr, "Read invalid character '%c' (0x%02x)\n", c, c);
	lex->tkn = LEF_EOF;
}


static void
lex_init(struct lef_lexer *lex, void *ptr, size_t len) {
	assert(lex && ptr);
	memset(lex, 0, sizeof(*lex));
	lex->pos = ptr;
	lex->end = ptr+len;

	// Initialize the text buffer.
	lex->text_cap = 128;
	lex->text = malloc(lex->text_cap);
	lex->text[0] = 0;

	// Prime the lexer.
	lex_next(lex);
}


static void
lex_dispose(struct lef_lexer *lex) {
	assert(lex);
	memset(lex, 0, sizeof(*lex));
}


/**
 * Copies the current token into a newly allocated string.
 */
static int
lex_name(struct lef_lexer *lex, char **out) {
	char *n;
	size_t len;
	assert(lex && out);

	if (lex->tkn < LEF_IDENT) {
		fprintf(stderr, "Expected name\n");
		return PHALANX_ERR_LEF_SYNTAX;
	}

	len = lex->tend - lex->tbase;
	n = malloc(len+1);
	memcpy(n, lex->tbase, len);
	n[len] = 0;
	*out = n;

	lex_next(lex);
	return PHALANX_OK;
}


/**
 * Parses the current token into a double.
 */
static int
lex_real(struct lef_lexer *lex, double *out) {
	assert(lex && out);

	if (lex->tkn < LEF_IDENT) {
		fprintf(stderr, "Expected real number\n");
		return PHALANX_ERR_LEF_SYNTAX;
	}

	errno = 0;
	*out = strtod(lex->text, NULL);
	if (errno != 0) {
		fprintf(stderr, "Not a valid real number; %s\n", strerror(errno));
		return PHALANX_ERR_LEF_SYNTAX;
	}
	lex_next(lex);

	return PHALANX_OK;
}


/**
 * Parses a coordinate pair enclosed in optional parentheses. The language
 * reference manual calls for these parentheses, however in the wild, plenty of
 * LEF files omitting them have been observed. Hence they are optional.
 */
static int
lex_xy(struct lef_lexer *lex, struct lef_xy *out) {
	int err, paren;
	assert(lex && out);

	// Opening parenthesis
	if (lex->tkn == LEF_LPAREN) {
		paren = 1;
		lex_next(lex);
	} else {
		paren = 0;
	}

	// X coordinate
	err = lex_real(lex, &out->x);
	if (err != PHALANX_OK) {
		fprintf(stderr, "  in x coordinate\n");
		return err;
	}

	// Y coordinate
	err = lex_real(lex, &out->y);
	if (err != PHALANX_OK) {
		fprintf(stderr, "  in y coordinate\n");
		return err;
	}

	// Closing parenthesis
	if (paren) {
		if (lex->tkn != LEF_RPAREN) {
			fprintf(stderr, "Expected closing parenthesis ')' after coordinate pair\n");
			return PHALANX_ERR_LEF_SYNTAX;
		}
		lex_next(lex);
	}

	return PHALANX_OK;
}


/**
 * Skips the next statement. This functions is crucial for making the parser
 * future-proof such that new language constructs can be safely ignored without
 * breaking compatibility.
 *
 * Statements may simply be skipped by looking for the trailing semicolon ';'.
 * However, groups (MACRO, PIN, etc.) are much more complicated, since there is
 * no clear syntax on how groups are opened and closed. The function does its
 * best to detect the kind of group at hand to properly skip it.
 */
static int
skip(struct lef_lexer *lex) {
	enum lef_token tkn;
	char *name;
	ssize_t len;
	int err;

	switch (lex->tkn) {

		// Ignore standalone semicolons.
		case LEF_SEMICOLON:
			lex_next(lex);
			return PHALANX_OK;

		// Groups of the form `<group> <name> ... END <name>`
		case LEF_KW_PIN:
		case LEF_KW_MACRO:
		case LEF_KW_SITE:
			lex_next(lex);
			if (lex->tkn < LEF_IDENT) {
				fprintf(stderr, "Expected name\n");
				return PHALANX_ERR_LEF_SYNTAX;
			}
			len = lex->tend-lex->tbase;
			name = malloc(len+1);
			memcpy(name, lex->tbase, len);
			name[len] = 0;
			lex_next(lex);

			// Allow for SITE CORE ; statements
			if (lex->tkn == LEF_SEMICOLON) {
				lex_next(lex);
				free(name);
				return PHALANX_OK;
			}
			while (lex->tkn != LEF_KW_END) {
				if (lex->tkn == LEF_EOF) {
					fprintf(stderr, "Unexpected end of file while looking for 'END' keyword\n");
					free(name);
					return PHALANX_ERR_LEF_SYNTAX;
				}
				err = skip(lex);
				if (err != PHALANX_OK) {
					free(name);
					return err;
				}
			}
			lex_next(lex);

			if (lex->tkn < LEF_IDENT || (lex->tend - lex->tbase) != len || memcmp(name, lex->tbase, len) != 0) {
				fprintf(stderr, "Expected name '%s' after 'END'\n", name);
				free(name);
				return PHALANX_ERR_LEF_SYNTAX;
			}
			lex_next(lex);

			free(name);
			return PHALANX_OK;

		// Groups of the form `<group> ... END <group>`
		case LEF_KW_PROPERTYDEFINITIONS:
			tkn = lex->tkn;
			lex_next(lex);
			while (lex->tkn != LEF_KW_END) {
				if (lex->tkn == LEF_EOF) {
					fprintf(stderr, "Unexpected end of file while looking for 'END' keyword\n");
					return PHALANX_ERR_LEF_SYNTAX;
				}
				err = skip(lex);
				if (err != PHALANX_OK)
					return err;
			}
			lex_next(lex);
			if (lex->tkn != tkn) {
				fprintf(stderr, "Expected token %04x after 'END'\n", tkn);
				return PHALANX_ERR_LEF_SYNTAX;
			}
			lex_next(lex);
			return PHALANX_OK;

		// Groups of the form `<group> ... END`
		case LEF_KW_OBS:
		case LEF_KW_PORT:
			lex_next(lex);
			while (lex->tkn != LEF_KW_END) {
				if (lex->tkn == LEF_EOF) {
					fprintf(stderr, "Unexpected end of file while looking for 'END' keyword\n");
					return PHALANX_ERR_LEF_SYNTAX;
				}
				err = skip(lex);
				if (err != PHALANX_OK)
					return err;
			}
			lex_next(lex); // skip the keyword
			return PHALANX_OK;

		// Regular statements of the form `... ;`
		default:
			while (lex->tkn != LEF_SEMICOLON) {
				if (lex->tkn == LEF_EOF) {
					fprintf(stderr, "Unexpected end of file while looking for ';'\n");
					return PHALANX_ERR_LEF_SYNTAX;
				}
				if (lex->tkn == LEF_KW_END) {
					fprintf(stderr, "Unexpected 'END' while looking for ';'\n");
					return PHALANX_ERR_LEF_SYNTAX;
				}
				lex_next(lex);
			}
			lex_next(lex); // skip the semicolon
			return PHALANX_OK;
	}
}


static int
begin_macro(struct lef_lexer *lex, const char *name, void *into, void **arg) {
	*arg = lef_new_macro(name);
	return PHALANX_OK;
}

static int
end_macro(struct lef_lexer *lex, void *into, void *arg) {
	struct lef_macro *macro = arg;
	array_shrink(&macro->pins);
	array_shrink(&macro->obs);
	lef_add_macro(into, macro);
	return PHALANX_OK;
}


static int
parse_macro_size(struct lef_lexer *lex, const char *name, void *into, void **arg) {
	int err;
	struct lef_macro *macro = into;
	assert(lex && into);

	err = lex_real(lex, &macro->size.x);
	if (err != PHALANX_OK)
		return err;

	if (lex->tkn != LEF_KW_BY) {
		fprintf(stderr, "Expected 'BY' keyword between width and height\n");
		return PHALANX_ERR_LEF_SYNTAX;
	}
	lex_next(lex);

	err = lex_real(lex, &macro->size.y);
	if (err != PHALANX_OK)
		return err;

	return PHALANX_OK;
}

static int
parse_macro_origin(struct lef_lexer *lex, const char *name, void *into, void **arg) {
	struct lef_macro *macro = into;
	return lex_xy(lex, &macro->origin);
}


static int
begin_pin(struct lef_lexer *lex, const char *name, void *into, void **arg) {
	*arg = lef_new_pin(name);
	return PHALANX_OK;
}

static int
end_pin(struct lef_lexer *lex, void *into, void *arg) {
	struct lef_pin *pin = arg;
	array_shrink(&pin->ports);
	lef_macro_add_pin(into, arg);
	return PHALANX_OK;
}


static int
begin_port(struct lef_lexer *lex, const char *name, void *into, void **arg) {
	*arg = lef_new_port();
	return PHALANX_OK;
}

static int
end_port(struct lef_lexer *lex, void *into, void *arg) {
	lef_pin_add_port(into, arg);
	return PHALANX_OK;
}

static int
parse_port_class(struct lef_lexer *lex, const char *name, void *into, void **arg) {
	lef_port_t *port = into;
	assert(lex && into);
	switch (lex->tkn) {
		case LEF_KW_NONE: port->cls = LEF_PORT_CLASS_NONE; break;
		case LEF_KW_CORE: port->cls = LEF_PORT_CLASS_CORE; break;
		case LEF_KW_BUMP: port->cls = LEF_PORT_CLASS_BUMP; break;
		default:
			fprintf(stderr, "Expected port class 'NONE', 'CORE', or 'BUMP'\n");
			return PHALANX_ERR_LEF_SYNTAX;
	}
	lex_next(lex);
	return PHALANX_OK;
}

static int
parse_port_layer(struct lef_lexer *lex, const char *name, void *into, void **arg) {
	lef_port_t *port = into;
	lef_geo_layer_t *layer;

	layer = lef_new_geo_layer();
	layer->layer = dupstr(name);

	array_add(&port->geos, &layer);
	port->last_layer = layer;
	return PHALANX_OK;
}

static int
parse_port_via(struct lef_lexer *lex, const char *name, void *into, void **arg) {
	fprintf(stderr, "'VIA' not implemented\n");
	return PHALANX_ERR_LEF_SYNTAX;
}

static int
parse_port_width(struct lef_lexer *lex, const char *name, void *into, void **arg) {
	lef_port_t *port = into;
	assert(lex && into);

	if (!port->last_layer) {
		fprintf(stderr, "'WIDTH' must follow a 'LAYER' statement\n");
		return PHALANX_ERR_LEF_SYNTAX;
	}

	return lex_real(lex, &port->last_layer->width);
}

static int
parse_port_path(struct lef_lexer *lex, const char *name, void *into, void **arg) {
	fprintf(stderr, "'PATH' not implemented\n");
	return PHALANX_ERR_LEF_SYNTAX;
}

static int
parse_port_rect(struct lef_lexer *lex, const char *name, void *into, void **arg) {
	int err;
	lef_port_t *port = into;
	struct lef_xy p0, p1;

	if (!port->last_layer) {
		fprintf(stderr, "'RECT' must follow a 'LAYER' statement\n");
		return PHALANX_ERR_LEF_SYNTAX;
	}

	err = lex_xy(lex, &p0);
	if (err != PHALANX_OK)
		return err;

	err = lex_xy(lex, &p1);
	if (err != PHALANX_OK)
		return err;

	lef_geo_layer_add_shape(port->last_layer, lef_new_geo_shape(
		LEF_SHAPE_RECT,
		2, (lef_xy_t[]){p0,p1}
	));
	return PHALANX_OK;
}

static int
parse_port_polygon(struct lef_lexer *lex, const char *name, void *into, void **arg) {
	fprintf(stderr, "'POLYGON' not implemented\n");
	return PHALANX_ERR_LEF_SYNTAX;
}


enum rule_type {
	RULE_STMT     = 1 << 0,
	RULE_GRP      = 1 << 1,
	RULE_NAMED    = 1 << 2,
	RULE_END_TKN  = 1 << 3,
	RULE_END_NAME = 1 << 4,
	RULE_NO_SEMI  = 1 << 5,
};

struct rule {
	const char *name;
	enum lef_token tkn;
	uint8_t type;
	uint8_t num_rules;
	const struct rule *rules;
	int (*prefn)(struct lef_lexer*, const char*, void*, void**);
	int (*postfn)(struct lef_lexer*, void*, void*);
	void (*abortfn)(void*);
};

static const struct rule port_rules[] = {
	{ .tkn = LEF_KW_CLASS, .type = RULE_STMT, .prefn = parse_port_class },
	{ .tkn = LEF_KW_LAYER, .type = RULE_STMT|RULE_NAMED, .prefn = parse_port_layer },
	{ .tkn = LEF_KW_VIA, .type = RULE_STMT, .prefn = parse_port_via },
	{ .tkn = LEF_KW_WIDTH, .type = RULE_STMT, .prefn = parse_port_width },
	{ .tkn = LEF_KW_PATH, .type = RULE_STMT, .prefn = parse_port_path },
	{ .tkn = LEF_KW_RECT, .type = RULE_STMT, .prefn = parse_port_rect },
	{ .tkn = LEF_KW_POLYGON, .type = RULE_STMT, .prefn = parse_port_polygon },
};

static const struct rule pin_rules[] = {
	{ .tkn = LEF_KW_PORT, .type = RULE_GRP, .rules = port_rules, .num_rules = ASIZE(port_rules), .prefn = begin_port, .postfn = end_port },
};

static const struct rule obs_rules[] = {
};

static const struct rule macro_rules[] = {
	{ .tkn = LEF_KW_SIZE, .type = RULE_STMT, .prefn = parse_macro_size },
	{ .tkn = LEF_KW_ORIGIN, .type = RULE_STMT, .prefn = parse_macro_origin },
	{ .tkn = LEF_KW_PIN, .type = RULE_GRP|RULE_NAMED|RULE_END_NAME, .rules = pin_rules, .num_rules = ASIZE(pin_rules), .prefn = begin_pin, .postfn = end_pin },
	{ .tkn = LEF_KW_OBS, .type = RULE_GRP, .rules = obs_rules, .num_rules = ASIZE(obs_rules) },
};

static const struct rule root_rules[] = {
	{ .tkn = LEF_KW_MACRO, .type = RULE_GRP|RULE_NAMED|RULE_END_NAME, .rules = macro_rules, .num_rules = ASIZE(macro_rules), .prefn = begin_macro, .postfn = end_macro },
};


/**
 * Parses the next statement by invoking one out of zero or more rules. This
 * calls the various pre, post, and abort functions specified in the rules to
 * process the stream of tokens.
 */
static int
parse_with_rules(struct lef_lexer *lex, void *into, const struct rule *rules, uint8_t num_rules) {
	const struct rule *rule, *rule_end;
	assert(lex && rules);

	// Try to match one of the rules.
	for (rule = rules, rule_end = rules+num_rules; rule != rule_end; ++rule) {
		void *arg = NULL;
		int err;
		char *name;
		ssize_t name_len;
		if (rule->tkn != lex->tkn)
			continue;

		// Skip the token that introduces the statement.
		lex_next(lex);

		// If requested, parse a name.
		if (rule->type & RULE_NAMED) {
			name_len = lex->tend - lex->tbase;
			name = malloc(name_len+1);
			memcpy(name, lex->tbase, name_len);
			name[name_len] = 0;
			lex_next(lex);
		} else {
			name_len = 0;
			name = NULL;
		}

		// Call the pre-parse function.
		if (rule->prefn) {
			err = rule->prefn(lex, name, into, &arg);
			if (err != PHALANX_OK)
				goto finish;
		}

		if (rule->type & RULE_GRP) {
			while (lex->tkn != LEF_KW_END) {
				if (lex->tkn == LEF_EOF) {
					fprintf(stderr, "Unexpected end of file in %04x\n", rule->tkn);
					err = PHALANX_ERR_LEF_SYNTAX;
					goto finish;
				}
				err = rule->rules
					? parse_with_rules(lex, arg, rule->rules, rule->num_rules)
					: skip(lex);
				if (err != PHALANX_OK)
					goto finish;
			}
			lex_next(lex);

			// If requested, check that the group token is repeated, allowing
			// for groups of the form `<group> ... END <group>`.
			if (rule->type & RULE_END_TKN) {
				if (lex->tkn != rule->tkn) {
					fprintf(stderr, "Expected token %04x after 'END'\n", rule->tkn);
					err = PHALANX_ERR_LEF_SYNTAX;
					goto finish;
				}
				lex_next(lex);
			}

			// If requested, check that the group name is repeated, allowing for
			// groups of the form `<group> <name> ... END <name>`.
			if (rule->type & RULE_END_NAME) {
				if (lex->tkn == LEF_EOF || (lex->tend - lex->tbase) != name_len || memcmp(name, lex->tbase, name_len) != 0) {
					fprintf(stderr, "Expected name '%s' after 'END'\n", name);
					err = PHALANX_ERR_LEF_SYNTAX;
					goto finish;
				}
				lex_next(lex);
			}
		}

		if ((rule->type & RULE_STMT) && !(rule->type & RULE_NO_SEMI)) {
			if (lex->tkn != LEF_SEMICOLON) {
				fprintf(stderr, "Expected ';' at the end of %04x statement\n", rule->tkn);
				err = PHALANX_ERR_LEF_SYNTAX;
				goto finish;
			}
			lex_next(lex);
		}

		// Call the post-parse function.
		if (rule->postfn) {
			err = rule->postfn(lex, into, arg);
			if (err != PHALANX_OK)
				goto finish;
		}

	finish:
		if (err != PHALANX_OK) {
			fputs("  in ", stderr);
			fputs(rule->name ? rule->name : token_names[rule->tkn], stderr);
			if (rule->type & RULE_NAMED) {
				fputc(' ', stderr);
				fputs(name, stderr);
			}
			if (rule->type & RULE_STMT) fputs(" statement", stderr);
			if (rule->type & RULE_GRP) fputs(" group", stderr);
			fputc('\n', stderr);
		}
		if (err != PHALANX_OK && arg && rule->abortfn)
			rule->abortfn(arg);
		if (rule->type & RULE_NAMED)
			free(name);
		return err;
	}

	// Skip the statement since no rule matched.
	return skip(lex);
}


/**
 * Parses an entire LEF file.
 */
static int
parse(struct lef_lexer *lex, struct lef *lef) {

	while (lex->tkn != LEF_EOF) {
		int result;

		if (lex->tkn == LEF_KW_END) {
			lex_next(lex);
			if (lex->tkn != LEF_KW_LIBRARY) {
				fprintf(stderr, "Expected 'LIBRARY' after 'END'\n");
				return PHALANX_ERR_LEF_SYNTAX;
			}
			lex_next(lex);
			if (lex->tkn != LEF_EOF) {
				fprintf(stderr, "'END LIBRARY' should be the last keywords in the file\n");
				return PHALANX_ERR_LEF_SYNTAX;
			}
			return PHALANX_OK;
		}

		result = parse_with_rules(lex, lef, root_rules, ASIZE(root_rules));
		if (result != PHALANX_OK)
			return result;
	}

	fprintf(stderr, "Expected 'END LIBRARY' keywords at the end of the file\n");
	return PHALANX_ERR_LEF_SYNTAX;
}


int
read_lef_file(const char *path, lef_t **out) {
	void *ptr;
	size_t len;
	int result = PHALANX_OK, fd, err;
	struct stat sb;
	struct lef_lexer lex;
	struct lef *lef;
	assert(path && out);

	// Open the file for reading.
	fd = open(path, O_RDONLY);
	if (fd == -1) {
		result = -errno;
		goto finish;
	}

	// Determine the file size.
	err = fstat(fd, &sb);
	if (err == -1) {
		result = -errno;
		goto finish_fd;
	}
	len = sb.st_size;

	// Map the file into memory.
	ptr = mmap(0, len, PROT_READ, MAP_PRIVATE, fd, 0);
	if (ptr == MAP_FAILED) {
		result = -errno;
		goto finish_fd;
	}

	// Process the file.
	lex_init(&lex, ptr, len);
	lef = lef_new();
	result = parse(&lex, lef);
	if (result != PHALANX_OK) {
		char *ls, *le, *line;
		fprintf(stderr, "  in %s:%u:%u\n", path, lex.line+1, lex.column+1);

		// Search backwards to the beginning of the line.
		ls = lex.tbase;
		le = lex.tbase;
		while (ls-1 > (char*)ptr && *(ls-1) != '\n') --ls;
		while (le < lex.end && *le != '\n') ++le;

		// Print the line and a marker.
		line = malloc(le-ls+1);
		memcpy(line, ls, le-ls);
		line[le-ls] = 0;
		fputs("\n  ", stderr);
		fputs(line, stderr);
		fputs("\n  ", stderr);
		while (ls != lex.tbase) {
			fputc(' ', stderr);
			++ls;
		}
		while (ls != lex.tend) {
			fputc('^', stderr);
			++ls;
		}
		fputs("\n\n", stderr);
	}
	lex_dispose(&lex);

	// Return a pointer to the created LEF structure, or destroy the structure
	// if an error occurred.
	if (result == PHALANX_OK) {
		*out = lef;
	} else {
		lef_free(lef);
	}

	// Unmap the file from memory.
finish_mmap:
	err = munmap(ptr, len);
	if (err == -1) {
		result = -errno;
		goto finish;
	}

	// Close the file.
finish_fd:
	close(fd);

finish:
	return result;
}
