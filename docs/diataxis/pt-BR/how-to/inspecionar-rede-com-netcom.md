# Como inspecionar rede com `netcom`

**Tipo Diátaxis:** how-to  
**Audiência:** utilizador intermédio  
**Item:** DOC-DIA-PT · **Owner:** technical-writer · **Last-reviewed:** 2026-08-23

## Quando usar

Quando quiser **localizar** interfaces wifi, ethernet ou Bluetooth (scan read-only) ou, com privilégio certo, pedir `-up`/`-down` via helpers do sistema.

## Pré-requisitos

- Builtin `netcom` no `petrush` (fatia ASM-NET)
- Scan: sem `CAP_NET_ADMIN`
- `-up` / `-down`: CapEff com `CAP_NET_ADMIN`; helpers no `PATH` (`ip`, `iw`, `iwd` ou `bluetoothctl`) se existirem

## Passos (scan)

1. Inventário completo (wifi + eth + bt):

```bash
netcom
```

2. Só uma família:

```bash
netcom -wifi
netcom -eth
netcom -bt
```

3. Ajuda:

```bash
netcom --help
```

## Passos (subir / descer link)

1. Confirme se tem a capability (o próprio builtin falha com mensagem clara se não tiver).
2. Não misture flags de scan com `-up`/`-down`.
3. Exemplos:

```bash
netcom -up wlan0
netcom -down eth0
```

Sem `CAP_NET_ADMIN`, espere stderr com `EPERM` e exit 1. **Não** é no-op silencioso. Nesta máquina de desenvolvimento, `-up` com root/CAP continua fora do pedido do líder até autorização explícita.

## Verificação

| Ação | Esperado |
|------|----------|
| `netcom -wifi` | texto de secção wifi; exit 0 |
| `netcom -up IFACE` sem CAP | mensagem EPERM; exit 1; sem hang |
| flag desconhecida | exit 2 |
| `-up` e `-down` juntos | exit 2 |

## Variações

- Overlay de sysfs para testes: API `petrush_netcom_set_root` (não é flag CLI).
- Se nenhum helper estiver no PATH com CAP presente: erro `-ENOENT` mapeado para exit 1.

## Relacionados

- Reference: [`netcom`](../reference/netcom.md)
- Explanation: [Por que sysfs / CAP](../explanation/por-que-sysfs-wai-netcom.md)
