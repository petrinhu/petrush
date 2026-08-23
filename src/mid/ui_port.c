/*
 * ui_port.c - Implementacao da porta DIP Mid <-> UI
 * ARCH-03 / P2.1: defaults no-op; Front faz bind no boot.
 */

#include "petrush/ui_port.h"

#include <stddef.h>

static void noop_clear_screen(void)
{
}

static int noop_history_len(void)
{
    return 0;
}

static const char *noop_history_get(int index)
{
    (void)index;
    return NULL;
}

static petrush_ui_port_t g_port = {
    .clear_screen = noop_clear_screen,
    .history_len = noop_history_len,
    .history_get = noop_history_get,
};

void petrush_ui_clear_screen(void)
{
    if (g_port.clear_screen) {
        g_port.clear_screen();
    }
}

int petrush_history_len(void)
{
    if (g_port.history_len) {
        return g_port.history_len();
    }
    return 0;
}

const char *petrush_history_get(int index)
{
    if (g_port.history_get) {
        return g_port.history_get(index);
    }
    return NULL;
}

void petrush_ui_port_bind(const petrush_ui_port_t *port)
{
    if (!port) {
        g_port.clear_screen = noop_clear_screen;
        g_port.history_len = noop_history_len;
        g_port.history_get = noop_history_get;
        return;
    }
    g_port.clear_screen = port->clear_screen ? port->clear_screen : noop_clear_screen;
    g_port.history_len = port->history_len ? port->history_len : noop_history_len;
    g_port.history_get = port->history_get ? port->history_get : noop_history_get;
}
