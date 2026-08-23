/*
 * ui_port.h - Porta DIP Mid <-> UI (clear + history)
 * ARCH-03 / P2.1 / R-I8: Mid não inclui linenoise.h.
 * Adapter Front liga linenoise em petrush_setup_linenoise_ux().
 */

#ifndef PETRUSH_UI_PORT_H
#define PETRUSH_UI_PORT_H

typedef struct petrush_ui_port {
    void (*clear_screen)(void);
    int (*history_len)(void);
    const char *(*history_get)(int index);
} petrush_ui_port_t;

/* Default: no-op / len=0 / get=NULL até bind. */
void petrush_ui_clear_screen(void);
int petrush_history_len(void);
const char *petrush_history_get(int index);

/* NULL = reset para no-op. */
void petrush_ui_port_bind(const petrush_ui_port_t *port);

#endif /* PETRUSH_UI_PORT_H */
