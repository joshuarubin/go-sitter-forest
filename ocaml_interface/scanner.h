#ifndef TREE_SITTER_OCAML_SCANNER_H_
#define TREE_SITTER_OCAML_SCANNER_H_

#include <assert.h>
#include <string.h>
#include <wctype.h>

#include "alloc.h"
#include "parser.h"

enum TokenType {
  COMMENT,
  LEFT_QUOTED_STRING_DELIM,
  RIGHT_QUOTED_STRING_DELIM,
  STRING_DELIM,
  LINE_NUMBER_DIRECTIVE,
  NULL_CHARACTER
};

typedef struct {
  bool in_string;

  size_t quoted_string_id_length;
  size_t quoted_string_id_capacity;
  int32_t *quoted_string_id;
} Scanner;

static inline void quoted_string_id_clear(Scanner *scanner) {
  scanner->quoted_string_id_length = 0;
}

static inline void quoted_string_id_resize(Scanner *scanner,
                                           size_t min_capacity) {
  size_t capacity = scanner->quoted_string_id_capacity;

  if (capacity >= min_capacity) return;

  if (capacity < 16) capacity = 16;
  while (capacity < min_capacity) capacity <<= 1;

  scanner->quoted_string_id_capacity = capacity;
  scanner->quoted_string_id =
      ts_realloc(scanner->quoted_string_id, capacity * sizeof(int32_t));
}

static inline void quoted_string_id_assign(Scanner *scanner, const char *buffer,
                                           size_t length) {
  if (length > 0) {
    quoted_string_id_resize(scanner, length);
    memcpy(scanner->quoted_string_id, buffer, length);
  }
  scanner->quoted_string_id_length = length;
}

static inline size_t quoted_string_id_copy(Scanner *scanner, char *buffer) {
  size_t length = scanner->quoted_string_id_length;
  if (length > 0) memcpy(buffer, scanner->quoted_string_id, length);
  return length;
}

static inline void quoted_string_id_push(Scanner *scanner, int32_t c) {
  quoted_string_id_resize(scanner, scanner->quoted_string_id_length + 1);
  scanner->quoted_string_id[scanner->quoted_string_id_length++] = c;
}

static inline void advance_ocaml_interface(TSLexer *lexer) { lexer->advance_ocaml_interface(lexer, false); }

static inline void skip_ocaml_interface(TSLexer *lexer) { lexer->advance_ocaml_interface(lexer, true); }

static inline bool eof(TSLexer *lexer) { return lexer->eof(lexer); }

static void scan_string(TSLexer *lexer) {
  for (;;) {
    switch (lexer->lookahead) {
      case '\\':
        advance_ocaml_interface(lexer);
        advance_ocaml_interface(lexer);
        break;
      case '"':
        advance_ocaml_interface(lexer);
        return;
      case '\0':
        if (eof(lexer)) return;
        advance_ocaml_interface(lexer);
        break;
      default:
        advance_ocaml_interface(lexer);
    }
  }
}

static bool is_quoted_string_delim(int32_t c) {
    return iswlower(c) || c == '_' || c >= 128;
}

static bool scan_left_quoted_string_delimiter(Scanner *scanner,
                                              TSLexer *lexer) {
  quoted_string_id_clear(scanner);

  while (is_quoted_string_delim(lexer->lookahead)) {
    quoted_string_id_push(scanner, lexer->lookahead);
    advance_ocaml_interface(lexer);
  }

  if (lexer->lookahead != '|') return false;

  advance_ocaml_interface(lexer);
  scanner->in_string = true;
  return true;
}

static bool scan_right_quoted_string_delimiter(Scanner *scanner,
                                               TSLexer *lexer) {
  for (size_t i = 0; i < scanner->quoted_string_id_length; i++) {
    if (lexer->lookahead != scanner->quoted_string_id[i]) return false;
    advance_ocaml_interface(lexer);
  }

  if (lexer->lookahead != '}') return false;

  scanner->in_string = false;
  return true;
}

static bool scan_quoted_string(Scanner *scanner, TSLexer *lexer) {
  if (!scan_left_quoted_string_delimiter(scanner, lexer)) return false;

  for (;;) {
    switch (lexer->lookahead) {
      case '|':
        advance_ocaml_interface(lexer);
        if (scan_right_quoted_string_delimiter(scanner, lexer)) return true;
        break;
      case '\0':
        if (eof(lexer)) return false;
        advance_ocaml_interface(lexer);
        break;
      default:
        advance_ocaml_interface(lexer);
    }
  }
}

static int32_t scan_character(TSLexer *lexer) {
  int32_t last = 0;

  switch (lexer->lookahead) {
    case '\\':
      advance_ocaml_interface(lexer);
      if (iswdigit(lexer->lookahead)) {
        advance_ocaml_interface(lexer);
        for (size_t i = 0; i < 2; i++) {
          if (!iswdigit(lexer->lookahead)) return 0;
          advance_ocaml_interface(lexer);
        }
      } else {
        switch (lexer->lookahead) {
          case 'x':
            advance_ocaml_interface(lexer);
            for (size_t i = 0; i < 2; i++) {
              if (!iswdigit(lexer->lookahead) &&
                  (towupper(lexer->lookahead) < 'A' ||
                   towupper(lexer->lookahead) > 'F')) {
                return 0;
              }
              advance_ocaml_interface(lexer);
            }
            break;
          case 'o':
            advance_ocaml_interface(lexer);
            for (size_t i = 0; i < 3; i++) {
              if (!iswdigit(lexer->lookahead) || lexer->lookahead > '7') {
                return 0;
              }
              advance_ocaml_interface(lexer);
            }
            break;
          case '\'':
          case '"':
          case '\\':
          case 'n':
          case 't':
          case 'b':
          case 'r':
          case ' ':
            last = lexer->lookahead;
            advance_ocaml_interface(lexer);
            break;
          default:
            return 0;
        }
      }
      break;
    case '\'':
      break;
    case '\0':
      if (eof(lexer)) return 0;
      advance_ocaml_interface(lexer);
      break;
    default:
      last = lexer->lookahead;
      advance_ocaml_interface(lexer);
  }

  if (lexer->lookahead == '\'') {
    advance_ocaml_interface(lexer);
    return 0;
  }
  return last;
}

static bool scan_identifier(TSLexer *lexer) {
  if (iswalpha(lexer->lookahead) || lexer->lookahead == '_') {
    advance_ocaml_interface(lexer);
    while (iswalnum(lexer->lookahead) || lexer->lookahead == '_' ||
           lexer->lookahead == '\'') {
      advance_ocaml_interface(lexer);
    }
    return true;
  }
  return false;
}

static bool scan_extattrident(TSLexer *lexer) {
  while (scan_identifier(lexer)) {
    if (lexer->lookahead != '.') return true;
    advance_ocaml_interface(lexer);
  }
  return false;
}

static bool scan_comment(Scanner *scanner, TSLexer *lexer) {
  int32_t last = 0;

  if (lexer->lookahead != '*') return false;
  advance_ocaml_interface(lexer);

  for (;;) {
    switch (last ? last : lexer->lookahead) {
      case '(':
        if (last) {
          last = 0;
        } else {
          advance_ocaml_interface(lexer);
        }
        scan_comment(scanner, lexer);
        break;
      case '*':
        if (last) {
          last = 0;
        } else {
          advance_ocaml_interface(lexer);
        }
        if (lexer->lookahead == ')') {
          advance_ocaml_interface(lexer);
          return true;
        }
        break;
      case '\'':
        if (last) {
          last = 0;
        } else {
          advance_ocaml_interface(lexer);
        }
        last = scan_character(lexer);
        break;
      case '"':
        if (last) {
          last = 0;
        } else {
          advance_ocaml_interface(lexer);
        }
        scan_string(lexer);
        break;
      case '{':
        if (last) {
          last = 0;
        } else {
          advance_ocaml_interface(lexer);
        }
        if (lexer->lookahead == '%') {
          advance_ocaml_interface(lexer);
          if (lexer->lookahead == '%') advance_ocaml_interface(lexer);
          if (scan_extattrident(lexer)) {
            while (iswspace(lexer->lookahead)) advance_ocaml_interface(lexer);
          } else {
            break;
          }
        }
        if (scan_quoted_string(scanner, lexer)) advance_ocaml_interface(lexer);
        break;
      case '\0':
        if (eof(lexer)) return false;
        if (last) {
          last = 0;
        } else {
          advance_ocaml_interface(lexer);
        }
        break;
      default:
        if (scan_identifier(lexer) || last) {
          last = 0;
        } else {
          advance_ocaml_interface(lexer);
        }
    }
  }
}

static Scanner *create() {
  Scanner *scanner = ts_calloc(1, sizeof(Scanner));
  return scanner;
}

static void destroy(Scanner *scanner) {
  ts_free(scanner->quoted_string_id);
  ts_free(scanner);
}

static unsigned serialize_ocaml_interface(Scanner *scanner, char *buffer) {
  buffer[0] = scanner->in_string;
  if (scanner->quoted_string_id_length >=
      TREE_SITTER_SERIALIZATION_BUFFER_SIZE) {
    return 1;
  }
  return quoted_string_id_copy(scanner, buffer + 1) + 1;
}

static void deserialize_ocaml_interface(Scanner *scanner, const char *buffer, unsigned length) {
  if (length > 0) {
    scanner->in_string = buffer[0];
    quoted_string_id_assign(scanner, buffer + 1, length - 1);
  } else {
    scanner->in_string = false;
    quoted_string_id_clear(scanner);
  }
}

static bool scan_ocaml_interface(Scanner *scanner, TSLexer *lexer, const bool *valid_symbols) {
  if (valid_symbols[LEFT_QUOTED_STRING_DELIM] &&
      (is_quoted_string_delim(lexer->lookahead) || lexer->lookahead == '|')) {
    lexer->result_symbol = LEFT_QUOTED_STRING_DELIM;
    return scan_left_quoted_string_delimiter(scanner, lexer);
  }
  if (valid_symbols[RIGHT_QUOTED_STRING_DELIM] && (lexer->lookahead == '|')) {
    advance_ocaml_interface(lexer);
    lexer->result_symbol = RIGHT_QUOTED_STRING_DELIM;
    return scan_right_quoted_string_delimiter(scanner, lexer);
  }
  if (scanner->in_string && valid_symbols[STRING_DELIM] &&
      lexer->lookahead == '"') {
    advance_ocaml_interface(lexer);
    scanner->in_string = false;
    lexer->result_symbol = STRING_DELIM;
    return true;
  }

  while (iswspace(lexer->lookahead)) {
    skip_ocaml_interface(lexer);
  }

  if (!scanner->in_string && lexer->lookahead == '#' &&
      lexer->get_column(lexer) == 0) {
    advance_ocaml_interface(lexer);

    while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
      advance_ocaml_interface(lexer);
    }

    if (!iswdigit(lexer->lookahead)) return false;
    while (iswdigit(lexer->lookahead)) advance_ocaml_interface(lexer);

    while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
      advance_ocaml_interface(lexer);
    }

    if (lexer->lookahead != '"') return false;
    advance_ocaml_interface(lexer);

    while (lexer->lookahead != '\n' && lexer->lookahead != '\r' &&
           lexer->lookahead != '"' && !eof(lexer)) {
      advance_ocaml_interface(lexer);
    }

    if (lexer->lookahead != '"') return false;
    advance_ocaml_interface(lexer);

    while (lexer->lookahead != '\n' && lexer->lookahead != '\r' &&
           !eof(lexer)) {
      advance_ocaml_interface(lexer);
    }

    lexer->result_symbol = LINE_NUMBER_DIRECTIVE;
    return true;
  }
  if (!scanner->in_string && lexer->lookahead == '(') {
    advance_ocaml_interface(lexer);
    lexer->result_symbol = COMMENT;
    return scan_comment(scanner, lexer);
  }
  if (!scanner->in_string && valid_symbols[STRING_DELIM] &&
      lexer->lookahead == '"') {
    advance_ocaml_interface(lexer);
    scanner->in_string = true;
    lexer->result_symbol = STRING_DELIM;
    return true;
  }
  if (valid_symbols[NULL_CHARACTER] && lexer->lookahead == '\0' &&
      !eof(lexer)) {
    advance_ocaml_interface(lexer);
    lexer->result_symbol = NULL_CHARACTER;
    return true;
  }

  return false;
}

#endif  // TREE_SITTER_OCAML_SCANNER_H_
