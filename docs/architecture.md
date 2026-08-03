# Arquitetura petrush — Mapeamento de Camadas

**Decisão (continuação 2026-07)**: Camadas lógicas pragmáticas (Opção 2).

Razão: porte Solo + anti-over-engineering. Evitar churn de mover arquivos agora. Segue os 4 princípios em espírito, sem forçar estrutura física desnecessária para uma ferramenta pessoal.

## Mapeamento

| Camada     | Local físico atual                  | Responsabilidade |
|------------|-------------------------------------|------------------|
| Front      | `src/main.c` + interações REPL (linenoise no loop) | Apresentação, prompt, I/O, sinais de UI, rc load, history display |
| Mid        | `src/mid/` (parser.c, dispatcher.c, pudo.c) | Lógica de aplicação: parsing, despacho de comandos/builtins, orquestração |
| Back       | (ainda thin) — ver `src/back/README.md` | Persistência/config futura (history file, pudo config) |
| Foundation | `src/foundation/` (env.c, process.c) | Primitivas do SO: getenv/setenv wrappers, fork/exec/wait, termios, job signals |

## Build

Todo listado explicitamente em `CMakeLists.txt`.

## Futuro

Quando o projeto crescer ou após revisão de porte (Cosimo), podemos materializar as camadas físicas movendo arquivos e atualizando includes/CMake.

Atualmente:
- `src/front/` : placeholder (README explica intenção).
- `src/back/` : placeholder.
- Código pragmático em mid/foundation + main.c para manter simples (anti-OE para solo).

Ver também:
- `CLAUDE.md` (regras do projeto)
- `docs/design/pudo.md`
- `TODO.md` (NEW-03)

## Notas de qualidade

- 0 deps runtime além de libc + linenoise (embutido)
- Hardening + ASan/UBSan + cppcheck + clang-tidy tuned
- TDD com acutest nas camadas mid/foundation
