/*
 * asm.h - Declaracoes C dos 10 atomos ASM (System V AMD64).
 *
 * Corpos: fatias ASM-* (GAS via Clang). Esta fatia (ASM-ABI) so o contrato.
 * wai/netcom: stubs de declaracao; corpos em ASM-WAI / ASM-NET.
 *
 * Convencao de retorno (salvo notas):
 *   0  = sucesso / igual
 *   1  = diferem (memeq_ct); demais simbolos: ver comentario
 *  -1  = erro (ver comentario por simbolo)
 *  Negativo estilo -errno onde o atomo e syscall wrapper (job_setpgid).
 */

#ifndef PETRUSH_ASM_H
#define PETRUSH_ASM_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- flags wai (sysfs inventory; corpo em ASM-WAI) ---- */
#define PETRUSH_WAI_DISK     (1u << 0)
#define PETRUSH_WAI_VIDEO    (1u << 1)
#define PETRUSH_WAI_MEM      (1u << 2)
#define PETRUSH_WAI_AUDIO    (1u << 3)
#define PETRUSH_WAI_CAMERA   (1u << 4)
#define PETRUSH_WAI_KEYBOARD (1u << 5)
#define PETRUSH_WAI_USB      (1u << 6)
#define PETRUSH_WAI_PCI      (1u << 7)
#define PETRUSH_WAI_BATTERY  (1u << 8)
#define PETRUSH_WAI_THERMAL  (1u << 9)
#define PETRUSH_WAI_CPU      (1u << 10)
#define PETRUSH_WAI_BOARD    (1u << 11)
#define PETRUSH_WAI_ALL      ((1u << 12) - 1u)

/* ---- flags netcom (corpo em ASM-NET) ---- */
#define PETRUSH_NETCOM_WIFI  (1u << 0)
#define PETRUSH_NETCOM_ETH   (1u << 1)
#define PETRUSH_NETCOM_BT    (1u << 2)

typedef enum {
    PETRUSH_TTY_COOKED = 0,
    PETRUSH_TTY_RAW = 1
} petrush_tty_mode_t;

/*
 * Glob POSIX simples: so metacaracteres * e ? (sem [] / **).
 * Retorna 1 se casa, 0 se nao. pat/str NUL-terminated.
 */
int petrush_glob_match(const char *pat, const char *str);

/*
 * Largura de exibicao (colunas) de um prefixo UTF-8 de len bytes.
 * Subset UAX#11. Retorna >=0 colunas, ou -1 se sequencia invalida.
 */
int petrush_utf8_width(const char *s, size_t len);

/*
 * Parse decimal signed 64-bit sem overflow UB.
 * Aceita opcional '+' / '-'. Escreve *out so em sucesso.
 * Retorna 0 sucesso; -1 entrada vazia/invalida/overflow.
 */
int petrush_parse_i64(const char *s, size_t len, int64_t *out);

/*
 * CRC-32 IEEE (polinomio 0xEDB88320), forma incremental.
 * crc tipico de arranque: 0xFFFFFFFFu; XOR final 0xFFFFFFFFu a cargo do caller
 * se quiser o digest "padrao" de ficheiro. Nao autentica .so (ver plugins).
 */
uint32_t petrush_crc32(uint32_t crc, const void *buf, size_t len);

/*
 * Comparacao em tempo constante (independente do conteudo, so de n).
 * Retorna 0 se iguais, 1 se diferem. a/b podem ser NULL so se n==0.
 */
int petrush_memeq_ct(const void *a, const void *b, size_t n);

/*
 * Alterna modo do tty em fd: COOKED (canonical+echo) ou RAW (TUI).
 * Retorna 0 sucesso; -errno em falha (sem tocar errno TLS).
 * Nao-TTY -> -ENOTTY; modo fora de COOKED/RAW -> -EINVAL.
 */
int petrush_tty_mode(int fd, petrush_tty_mode_t mode);

/*
 * FNV-1a 64-bit da string path (len bytes; nao exige NUL).
 * Semente oficial FNV-1a 64: 14695981039346656037.
 */
uint64_t petrush_hash_path(const char *path, size_t len);

/*
 * Syscall setpgid(pid, pgid) via System V AMD64.
 * Retorna 0 sucesso; -errno em falha (sem tocar errno TLS).
 */
int petrush_job_setpgid(pid_t pid, pid_t pgid);

/*
 * Prefixo de overlay para testes (sysfs/proc sob root).
 * NULL ou "" = caminhos absolutos /sys e /proc. Sem root/setuid.
 */
void petrush_wai_set_root(const char *root);

/*
 * ASM-WAI: inventaria sysfs/proc conforme flags (0 = PETRUSH_WAI_ALL).
 * Sem serial/uuid. Escreve texto em out (NUL-terminated se cap>0).
 * Retorna bytes escritos (sem contar NUL) ou -1 erro / -ENOSPC.
 * Entrada ASM (wai_scan.S); corpo I/O em C (ASan) via petrush_wai_scan_impl.
 */
int petrush_wai_scan(unsigned flags, char *out, size_t out_cap);
int petrush_wai_scan_impl(unsigned flags, char *out, size_t out_cap);

/*
 * Prefixo de overlay para testes (sysfs sob root).
 * NULL ou "" = caminhos absolutos /sys. Sem root/setuid.
 */
void petrush_netcom_set_root(const char *root);

/*
 * ASM-NET: scan -wifi/-eth/-bt via sysfs + netlink GET (read-only).
 * flags 0 = PETRUSH_NETCOM_WIFI|ETH|BT. Escreve texto em out (NUL se cap>0).
 * Retorna bytes escritos (sem NUL) ou -1 / -ENOSPC.
 * Entrada ASM (netcom_scan.S); I/O em C (ASan) via petrush_netcom_scan_impl.
 * -up/-down NAO entram neste simbolo (ver petrush_netcom_link_set).
 */
int petrush_netcom_scan(unsigned flags, char *out, size_t out_cap);
int petrush_netcom_scan_impl(unsigned flags, char *out, size_t out_cap);

/*
 * 1 se CapEff tem CAP_NET_ADMIN; 0 caso contrario. Sem libcap.
 */
int petrush_netcom_have_cap_net_admin(void);

/*
 * -up/-down via helpers C (ip / iw / iwd / bluetoothctl se existirem).
 * Sem CAP_NET_ADMIN: retorna -EPERM de imediato (sem hang, sem spawn).
 * Com CAP: exec com timeout curto; -ETIMEDOUT se helper pendurar;
 * -ENOENT se nenhum helper no PATH. up!=0 sobe; up==0 desce.
 */
int petrush_netcom_link_set(const char *iface, int up);

#ifdef __cplusplus
}
#endif

#endif /* PETRUSH_ASM_H */
