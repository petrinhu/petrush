/*
 * highlight.c — Scanner + colorize mínimo (UX-21)
 * Não chama o parser. Sem PATH/keywords/$VAR/#.
 */

#include "petrush/highlight.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define HL_MAX_SPANS 64

static int is_space(char c)
{
    return isspace((unsigned char)c) != 0;
}

/* Longest-match OP at token boundary. Returns byte length or 0. */
static size_t match_op(const char *p, size_t remain)
{
    if (remain >= 4 && p[0] == '2' && p[1] == '>' && p[2] == '&' && p[3] == '1') {
        char after = remain > 4 ? p[4] : '\0';
        if (after == '\0' || is_space(after) || after == '|' || after == '<' ||
            after == '>' || after == '&' || after == ';')
            return 4;
    }
    if (remain >= 3 && p[0] == '2' && p[1] == '>' && p[2] == '>')
        return 3;
    if (remain >= 2 && p[0] == '2' && p[1] == '>')
        return 2;
    if (remain >= 2 && p[0] == '&' && p[1] == '>')
        return 2;
    if (remain >= 2 && p[0] == '>' && p[1] == '>')
        return 2;
    if (remain >= 2 && p[0] == '|' && p[1] == '|')
        return 2;
    if (remain >= 2 && p[0] == '&' && p[1] == '&')
        return 2;
    if (remain >= 1 && (p[0] == '|' || p[0] == ';' || p[0] == '<' || p[0] == '>'))
        return 1;
    return 0;
}

static int op_resets_cmd(const char *p, size_t oplen)
{
    if (oplen == 1 && (p[0] == '|' || p[0] == ';'))
        return 1;
    if (oplen == 2 && ((p[0] == '|' && p[1] == '|') ||
                       (p[0] == '&' && p[1] == '&')))
        return 1;
    return 0;
}

/* Break WORD mid-token on these; `2>` only matches at token start. */
static int word_break_here(const char *p, size_t remain)
{
    if (remain >= 1 && (p[0] == '|' || p[0] == ';' || p[0] == '<' || p[0] == '>'))
        return 1;
    if (remain >= 2 && p[0] == '&' && (p[1] == '>' || p[1] == '&'))
        return 1;
    return 0;
}

static int push_span(petrush_hl_span_t *out, int *n, int max_spans,
                     size_t start, size_t len, enum petrush_hl_kind kind)
{
    if (len == 0) return 1;
    if (*n >= max_spans) return 0;
    out[*n].start = start;
    out[*n].len = len;
    out[*n].kind = kind;
    (*n)++;
    return 1;
}

int petrush_hl_scan(const char *buf, size_t len,
                    petrush_hl_span_t *out, int max_spans)
{
    if (!buf || !out || max_spans <= 0 || len == 0)
        return 0;

    int n = 0;
    size_t i = 0;
    int expect_cmd = 1;

    while (i < len) {
        size_t ws = i;
        while (i < len && is_space(buf[i])) i++;
        if (i > ws) {
            if (!push_span(out, &n, max_spans, ws, i - ws, PETRUSH_HL_PLAIN))
                break;
        }
        if (i >= len) break;

        char c = buf[i];
        if (c == '"' || c == '\'') {
            char q = c;
            size_t start = i;
            i++;
            while (i < len && buf[i] != q) i++;
            enum petrush_hl_kind kind;
            if (i < len && buf[i] == q) {
                i++;
                kind = PETRUSH_HL_STR;
            } else {
                kind = PETRUSH_HL_UNCLOSED;
            }
            if (!push_span(out, &n, max_spans, start, i - start, kind))
                break;
            expect_cmd = 0;
            continue;
        }

        size_t oplen = match_op(buf + i, len - i);
        if (oplen > 0) {
            int reset = op_resets_cmd(buf + i, oplen);
            if (!push_span(out, &n, max_spans, i, oplen, PETRUSH_HL_OP))
                break;
            i += oplen;
            expect_cmd = reset ? 1 : 0;
            continue;
        }

        size_t start = i;
        while (i < len) {
            if (is_space(buf[i])) break;
            if (buf[i] == '"' || buf[i] == '\'') break;
            if (word_break_here(buf + i, len - i)) break;
            /* Lone '&' stays in the word (UX-23 ainda não). */
            i++;
        }
        enum petrush_hl_kind kind = expect_cmd ? PETRUSH_HL_CMD : PETRUSH_HL_PLAIN;
        if (!push_span(out, &n, max_spans, start, i - start, kind))
            break;
        expect_cmd = 0;
    }

    return n;
}

static const char *kind_csi(enum petrush_hl_kind kind)
{
    switch (kind) {
    case PETRUSH_HL_CMD:      return "\033[1;32m";
    case PETRUSH_HL_STR:      return "\033[33m";
    case PETRUSH_HL_UNCLOSED: return "\033[1;31m";
    case PETRUSH_HL_OP:       return "\033[34m";
    case PETRUSH_HL_PLAIN:
    default:                  return "\033[0m";
    }
}

char *petrush_hl_colorize(const char *buf, size_t len)
{
    if (!buf) return NULL;

    petrush_hl_span_t spans[HL_MAX_SPANS];
    int n = petrush_hl_scan(buf, len, spans, HL_MAX_SPANS);

    /* Worst case: every byte own span + CSI(~8) + reset. */
    size_t cap = len * 10 + 16;
    if (cap < 16) cap = 16;
    char *out = malloc(cap);
    if (!out) return NULL;

    size_t o = 0;
    size_t cursor = 0;

    for (int i = 0; i < n; i++) {
        size_t s = spans[i].start;
        size_t sl = spans[i].len;
        if (s > len) break;
        if (s + sl > len) sl = len - s;

        if (s > cursor) {
            /* Gap: PLAIN (fail-soft overflow / inconsistency). */
            const char *csi = kind_csi(PETRUSH_HL_PLAIN);
            size_t cl = strlen(csi);
            size_t gap = s - cursor;
            if (o + cl + gap + 1 > cap) {
                size_t ncap = (o + cl + gap + 1) * 2;
                char *tmp = realloc(out, ncap);
                if (!tmp) { free(out); return NULL; }
                out = tmp;
                cap = ncap;
            }
            memcpy(out + o, csi, cl);
            o += cl;
            memcpy(out + o, buf + cursor, gap);
            o += gap;
            cursor = s;
        }

        const char *csi = kind_csi(spans[i].kind);
        size_t cl = strlen(csi);
        if (o + cl + sl + 1 > cap) {
            size_t ncap = (o + cl + sl + 1) * 2;
            char *tmp = realloc(out, ncap);
            if (!tmp) { free(out); return NULL; }
            out = tmp;
            cap = ncap;
        }
        memcpy(out + o, csi, cl);
        o += cl;
        memcpy(out + o, buf + s, sl);
        o += sl;
        cursor = s + sl;
    }

    if (cursor < len) {
        const char *csi = kind_csi(PETRUSH_HL_PLAIN);
        size_t cl = strlen(csi);
        size_t gap = len - cursor;
        if (o + cl + gap + 1 > cap) {
            size_t ncap = o + cl + gap + 1;
            char *tmp = realloc(out, ncap);
            if (!tmp) { free(out); return NULL; }
            out = tmp;
            cap = ncap;
        }
        memcpy(out + o, csi, cl);
        o += cl;
        memcpy(out + o, buf + cursor, gap);
        o += gap;
    }

    out[o] = '\0';
    return out;
}
