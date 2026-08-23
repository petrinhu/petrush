/*
 * ASM-ABI: TU de compilacao (so -c). Prova que asm.h declara os 10 simbolos.
 * Nao linka corpos (fatias ASM-* seguintes).
 */
#include "petrush/asm.h"

#include <stddef.h>
#include <stdint.h>

int main(void)
{
    int (*f_glob)(const char *, const char *) = petrush_glob_match;
    int (*f_utf8)(const char *, size_t) = petrush_utf8_width;
    int (*f_i64)(const char *, size_t, int64_t *) = petrush_parse_i64;
    uint32_t (*f_crc)(uint32_t, const void *, size_t) = petrush_crc32;
    int (*f_memeq)(const void *, const void *, size_t) = petrush_memeq_ct;
    int (*f_tty)(int, petrush_tty_mode_t) = petrush_tty_mode;
    uint64_t (*f_hash)(const char *, size_t) = petrush_hash_path;
    int (*f_pgid)(pid_t, pid_t) = petrush_job_setpgid;
    int (*f_wai)(unsigned, char *, size_t) = petrush_wai_scan;
    int (*f_net)(unsigned, char *, size_t) = petrush_netcom_scan;

    (void)f_glob;
    (void)f_utf8;
    (void)f_i64;
    (void)f_crc;
    (void)f_memeq;
    (void)f_tty;
    (void)f_hash;
    (void)f_pgid;
    (void)f_wai;
    (void)f_net;
    (void)PETRUSH_TTY_COOKED;
    (void)PETRUSH_TTY_RAW;
    (void)PETRUSH_WAI_DISK;
    (void)PETRUSH_NETCOM_WIFI;
    return 0;
}
