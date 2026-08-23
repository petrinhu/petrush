# Cómo inspeccionar red con `netcom`

**Tipo Diátaxis:** how-to  
**Audiencia:** usuario intermedio  
**Item:** DOC-DIA-ES · **Owner:** technical-writer · **Last-reviewed:** 2026-08-23

## Cuándo usar

Cuando quieras **localizar** interfaces wifi, ethernet o Bluetooth (scan de solo lectura) o, con el privilegio correcto, pedir `-up`/`-down` vía helpers del sistema.

## Prerrequisitos

- Builtin `netcom` en el `petrush` (rebanada ASM-NET)
- Scan: sin `CAP_NET_ADMIN`
- `-up` / `-down`: CapEff con `CAP_NET_ADMIN`; helpers en el `PATH` (`ip`, `iw`, `iwd` o `bluetoothctl`) si existen

## Pasos (scan)

1. Inventario completo (wifi + eth + bt):

```bash
netcom
```

2. Solo una familia:

```bash
netcom -wifi
netcom -eth
netcom -bt
```

3. Ayuda:

```bash
netcom --help
```

## Pasos (subir / bajar link)

1. Confirma si tienes la capability (el propio builtin falla con mensaje claro si no la tienes).
2. No mezcles flags de scan con `-up`/`-down`.
3. Ejemplos:

```bash
netcom -up wlan0
netcom -down eth0
```

Sin `CAP_NET_ADMIN`, espera stderr con `EPERM` y exit 1. **No** es no-op silencioso. En esta máquina de desarrollo, `-up` con root/CAP sigue fuera del pedido del líder hasta autorización explícita.

## Verificación

| Acción | Esperado |
|--------|----------|
| `netcom -wifi` | texto de sección wifi; exit 0 |
| `netcom -up IFACE` sin CAP | mensaje EPERM; exit 1; sin hang |
| flag desconocida | exit 2 |
| `-up` y `-down` juntos | exit 2 |

## Variaciones

- Overlay de sysfs para pruebas: API `petrush_netcom_set_root` (no es flag CLI).
- Si ningún helper está en el PATH con CAP presente: error `-ENOENT` mapeado a exit 1.

## Relacionados

- Reference: [`netcom`](../reference/netcom.md)
- Explanation: [Por qué sysfs / CAP](../explanation/por-que-sysfs-wai-netcom.md)
