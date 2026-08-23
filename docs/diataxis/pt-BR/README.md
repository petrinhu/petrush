# Documentação Diátaxis (pt-BR)

> **Tipo:** hub de navegação (não é um dos quatro tipos Diátaxis).
> **Audiência:** utilizador do petrush (novato a intermédio) e contribuidores.
> **Idioma canónico desta árvore:** pt-BR.
> **Item:** DOC-DIA-PT · **Owner:** technical-writer · **Last-reviewed:** 2026-08-23
> **Versão do produto:** pós OSH-0 / CXX-TUI / ASM-WAI / ASM-NET (sem tag de release nesta fatia).

Mapa Diátaxis do petrush em português. Cada página tem **um tipo**, **uma audiência** e cobre só o que o código faz hoje. Traduções: DOC-DIA-EN e DOC-DIA-ES (ondas seguintes).

## Aviso de escopo

- OSH-0 **não** é POSIX completo. É modo script + shebang + exit do último comando. Posicionais (`$1`), `if`/`while` e o resto do trilho OSH vêm depois.
- Sem produção nesta máquina: testes em Docker; sem `4755`; `netcom -up`/`-down` exige `CAP_NET_ADMIN` (senão EPERM claro).
- Fontes de verdade: código em `src/`, contrato em [`docs/architecture.md`](../../architecture.md) e ADR-001.

## Tutorial (aprender fazendo)

| Página | O que você constrói |
|--------|---------------------|
| [Primeiro script OSH-0](tutorial/primeiro-script-osh0.md) | Script executável com shebang, sem REPL |

## How-to (resolver um objetivo)

| Página | Objetivo |
|--------|----------|
| [Inventariar hardware com `wai`](how-to/inventariar-com-wai.md) | Listar disco, CPU, memória, etc. sem root |
| [Inspecionar rede com `netcom`](how-to/inspecionar-rede-com-netcom.md) | Scan wifi/eth/bt; entender EPERM no `-up` |
| [Usar o `configsh`](how-to/usar-configsh.md) | Dump, check e TUI da config INI |
| [Localizar paths XDG](how-to/localizar-paths-xdg.md) | Achar `config.ini`, plugins e allow-list |

## Reference (consultar)

| Página | Superfície |
|--------|------------|
| [`wai`](reference/wai.md) | Builtin de inventário sysfs/proc |
| [`netcom`](reference/netcom.md) | Builtin de scan / link up-down |
| [`configsh`](reference/configsh.md) | Binário C++23 de configuração |
| [OSH-0 / shebang](reference/osh0-shebang.md) | Modo `petrush arquivo` |
| [Paths XDG](reference/xdg.md) | Variáveis e ficheiros sob XDG |

## Explanation (entender)

| Página | Pergunta |
|--------|----------|
| [OSH-0 não é POSIX completo](explanation/osh0-nao-e-posix-completo.md) | Por que o trilho começa pelo shebang |
| [Por que `wai` e `netcom` leem sysfs](explanation/por-que-sysfs-wai-netcom.md) | Modelo mental e limites de privilégio |
| [Layout XDG do petrush](explanation/layout-xdg-petrush.md) | Por que config e plugins vivem em sítios diferentes |

## Relacionados (fora desta árvore)

- Guia iniciante (mistura rápida): [`docs/beginner-guide.md`](../../beginner-guide.md)
- Arquitetura / stack tripla: [`docs/architecture.md`](../../architecture.md)
- Threat model de plugins: [`docs/security/plugins-threat.md`](../../security/plugins-threat.md)
- Plano OSH: [`docs/plano-shell-avancado.md`](../../plano-shell-avancado.md)
