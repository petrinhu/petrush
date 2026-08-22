# Arquitetura petrush — Mapeamento de Camadas

**Tipo:** explanation  
**Audience:** desenvolvedor intermediário (interno)  
**Last-reviewed:** 2026-08-22  
**Owner:** technical-writer (DOC-01)  
**Versão do produto:** alinhada ao tree atual (pós-v0.1)

**Decisão (continuação 2026-07)**: Camadas lógicas pragmáticas (Opção 2).

Razão: porte **early** (variante Pipeline-Sprint; marcador `.bigtech-porte`) + anti-over-engineering. A palavra “Solo” em prosa antiga é informal — o piso canônico é early. Evitar churn de mover arquivos agora. Segue os 4 princípios em espírito, sem forçar estrutura física desnecessária para uma ferramenta pequena.

## Mapeamento

| Camada     | Local físico atual                  | Responsabilidade |
|------------|-------------------------------------|------------------|
| Front      | `src/main.c` + `src/front/complete.c` (linenoise completion/hints) + loop REPL | Apresentação, prompt, I/O, sinais de UI, rc load, history display, tab-complete |
| Mid        | `src/mid/` (parser, dispatcher, pudo, expand, prompt, alias, …) | Lógica de aplicação: parsing, despacho de comandos/builtins, orquestração |
| Back       | (ainda thin) — ver `src/back/README.md` | Persistência/config futura (history file, pudo config) |
| Foundation | `src/foundation/` (env.c, process.c) | Primitivas do SO: getenv/setenv wrappers, fork/exec/wait, termios, job signals |

## Build

Todo listado explicitamente em `CMakeLists.txt` (inclui `src/front/complete.c`).

## Estado físico das pastas

- `src/front/` : código real — `complete.c` (completion + history autosuggest via linenoise). `main.c` permanece na raiz de `src/` por simplicidade (mapeamento lógico = Front).
- `src/back/` : placeholder (README explica intenção).
- `src/mid/` e `src/foundation/` : código pragmático ativo.

## Futuro

Quando o projeto crescer ou após revisão de porte (Cosimo), podemos materializar as camadas físicas movendo arquivos (ex.: `main.c` → `src/front/`) e atualizando includes/CMake. Até lá, o mapeamento lógico acima é a fonte de verdade.

Ver também:
- `CLAUDE.md` (regras do projeto)
- `.bigtech-porte` (porte=early, variante=Pipeline-Sprint)
- `docs/design/pudo.md`
- `TODO.md` (NEW-03, DOC-01)

## Notas de qualidade

- 0 deps runtime além de libc + linenoise (embutido)
- Hardening + ASan/UBSan + cppcheck + clang-tidy tuned
- TDD com acutest nas camadas mid/foundation (e `tests/test_complete.c` para Front)
