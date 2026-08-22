/*
 * highlight.h — Syntax highlight mínimo no REPL (UX-21)
 * Scanner Front testável; não chama o parser.
 */

#ifndef PETRUSH_HIGHLIGHT_H
#define PETRUSH_HIGHLIGHT_H

#include <stddef.h>

enum petrush_hl_kind {
    PETRUSH_HL_PLAIN = 0,
    PETRUSH_HL_CMD,
    PETRUSH_HL_STR,
    PETRUSH_HL_UNCLOSED,
    PETRUSH_HL_OP
};

typedef struct {
    size_t start;
    size_t len;
    enum petrush_hl_kind kind;
} petrush_hl_span_t;

/* Preenche out[0..max_spans). Retorna número de spans escritos (0..max_spans). */
int petrush_hl_scan(const char *buf, size_t len,
                    petrush_hl_span_t *out, int max_spans);

/* malloc CSI-colorizado; caller free. NULL se buf == NULL. */
char *petrush_hl_colorize(const char *buf, size_t len);

#endif /* PETRUSH_HIGHLIGHT_H */
