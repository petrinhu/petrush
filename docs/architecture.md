# Arquitetura petrush - Mapeamento de Camadas

**Tipo:** explanation  
**Audience:** desenvolvedor intermediário (interno)  
**Last-reviewed:** 2026-08-23  
**Owner:** technical-writer (DOC-04; DOC-01 base)  
**Versão do produto:** alinhada ao tree atual (pós-v0.5 tree / AUD-ARCH)

**Decisão (continuação 2026-07)**: Camadas lógicas pragmáticas (Opção 2).

Razão: porte **early** (variante Pipeline-Sprint; marcador `.bigtech-porte`) + anti-over-engineering. A palavra “Solo” em prosa antiga é informal - o piso canônico é early. Evitar churn de mover arquivos agora. Segue os 4 princípios em espírito, sem forçar estrutura física desnecessária para uma ferramenta pequena.

Fecha drift AUD-ARCH F5 / R-I12 e documenta exceções F3/F4 (R-I10 / R-I11).

## Mapeamento

| Camada     | Local físico atual | Responsabilidade |
|------------|--------------------|------------------|
| Front      | `src/main.c` (composition root na raiz de `src/`) + `src/front/complete.c` + `src/front/highlight.c` | REPL, prompt, I/O, sinais de UI, rc load, history display, tab-complete, highlight |
| Mid        | `src/mid/` (lista completa abaixo) | Domínio do shell: parse, expand, builtins/despacho, aliases, dirstack, source, hist expand, prompt string, cliente `pudo` |
| Back       | (ainda thin) - ver `src/back/README.md` | Persistência/config futura (history file dedicado, pudo config de longo prazo) |
| Foundation | `src/foundation/env.c` + `process.c` + `job.c` + `rc_trust.c` | Primitivas do SO: getenv/setenv wrappers, fork/exec/wait/pipeline/redirs, jobs em background (`&`), checagem uid/mode do rc (ARCH-02 / R-I9) |
| Helper     | `src/pudod/` (binário `pudod` separado) | Elevação opcional / allow-list. **Fora** do quadro de 4 camadas; não inclui `petrush/*` nem linenoise |

### Mid completo (`src/mid/`)

| Unidade | Papel |
|---------|-------|
| `parser.c` | Tokenização / AST de comando e pipeline |
| `expand.c` | Expansão de palavras (glob, brace, vars) |
| `dispatcher.c` | Builtins + despacho; bg job (`&`) |
| `alias.c` | Aliases de shell |
| `dirstack.c` | Stack de diretórios (`pushd`/`popd`/…) |
| `source.c` | `source` / `.` de scripts e rc |
| `hist_expand.c` | Expansão de history (`!!`, …) via `ui_port` |
| `ui_port.c` | Porta DIP clear + history get/len (ARCH-03) |
| `prompt.c` | Montagem da string de prompt (`PETRUSH_PS1`) |
| `pudo.c` | Cliente unpriv do helper `pudod` |

### Front detalhado

| Unidade | Papel |
|---------|-------|
| `src/main.c` | Loop REPL / composition root (mapeamento lógico = Front) |
| `src/front/complete.c` | Completion + history autosuggest (linenoise) |
| `src/front/highlight.c` | Colorize mínimo (UX-21) |

### Foundation detalhado

| Unidade | Papel |
|---------|-------|
| `env.c` | Wrappers de ambiente |
| `process.c` | `execute_external` / `execute_pipeline` (+ hook de filho) |
| `job.c` | Tabela de jobs / wait de background (UX-23) |
| `rc_trust.c` | `petrush_rc_stat_ok` (uid/mode do rc; SEC-10). Mid `source.c` inclui `rc_trust.h` (ARCH-02 fechou F2 / R-I9) |

## Stack de linguagens (ADR-CXXASM)

Decisao registada em [`docs/adr/001-c23-cxx-asm-plugins.md`](adr/001-c23-cxx-asm-plugins.md) (ADR-001). Nao reabrir sem o lider. Prosa completa prosa↔pastas = item DOC-ARCH (W24).

| Superficie | Linguagem | Binario |
|------------|-----------|---------|
| Parser / eval OSH | C23 | `petrush` |
| TUI de configuracao | C++23 | `configsh` (alvo CXX-00; sem `libstdc++` no `petrush`) |
| 10 ilhas nomeadas | ASM System V AMD64 (GAS/Clang) | ligadas em `petrush` se `PETRUSH_ASM=ON` |
| Plugins de terceiro | ABI C (`plugins/abi.h`, fatia PLG-ABI) | `.so`; sem `dlopen` no main ate PLG-LOAD |

## Build

Todo listado explicitamente em `CMakeLists.txt` (inclui `src/front/{complete,highlight}.c`, Mid completo, `src/foundation/{env,process,job,rc_trust}.c`, e o alvo separado `pudod`). Linguagens do `project()`: C, CXX, ASM (ASM-00). Alvo `configsh` ainda nao existe (CXX-00).

## Estado físico das pastas

- `src/front/` : código real (`complete.c`, `highlight.c`). `main.c` permanece na raiz de `src/` por simplicidade (porte early).
- `src/mid/` : núcleo ativo do shell (tabela acima).
- `src/foundation/` : `env.c`, `process.c`, `job.c`, `rc_trust.c`.
- `src/back/` : placeholder (README explica intenção).
- `src/pudod/` : binário helper isolado (`pudod.c` + allow/resolve/open). Ver `docs/security/pudod-install.md`. Setuid **não** endossado nesta passada.

## Exceções de fronteira aceitas no early

CONTRACT §5 e o checklist AUD-ARCH pedem que só Foundation/platform chame libc de processo, e que Foundation não conheça AST de aplicação. No porte early estas duas exceções estão **documentadas e aceitas** (não são bugs silenciosos):

### F3 / R-I10 - Mid chama `fork` / `exec*` (pudo + jobs)

| Onde | Por quê |
|------|---------|
| `src/mid/dispatcher.c` (`dispatch_pipeline_background`: `fork`, `open("/dev/null")`) | Job `&` (UX-23) fica **fora** de `execute_external` / `execute_pipeline` de propósito (não empurrar bg para o caminho síncrono). |
| `src/mid/pudo.c` (`execve` / caminho do helper) | Cliente unpriv dispara o binário `pudod` após sanitização; desenho de segurança (helper separado). |

Mitigação futura opcional: `petrush_spawn_background()` em Foundation. Não bloqueia early.

### F4 / R-I11 - `process.h` inclui `parser.h`

`include/petrush/process.h` inclui `petrush/parser.h` para tipar `petrush_cmd_t` / pipeline (DTO do shell). Foundation **não** inclui `dispatcher.h` (comentário no header). Aceito no early; alternativa futura = extrair `petrush/cmd.h` neutro sem lógica Mid.

### Outras dívidas de fronteira (não fechadas por DOC-04)

| ID | Tema | Estado |
|----|------|--------|
| F1 / R-I8 | Mid importa linenoise (`dispatcher` clear; `hist_expand`) | **Fechado** por ARCH-03 (`ui_port` DIP; adapter em `complete.c`) |
| F2 / R-I9 | `rc_trust` físico Front, usado pelo Mid | **Fechado** por ARCH-02 (`src/foundation/rc_trust.c`) |

## Direção de includes (alvo early)

```
Front (main, complete, highlight)
  → Mid → Foundation (env, process, job, rc_trust)
Back: thin / placeholder
pudod: headers locais apenas (0 petrush/*, 0 linenoise)
```

Vendor linenoise: path canônico de UI = Front. Mid acessa clear/history só via `petrush/ui_port.h` (ARCH-03 / F1 fechado).

## Futuro

Quando o projeto crescer ou após revisão de porte (Cosimo), podemos materializar as camadas físicas movendo arquivos (ex.: `main.c` → `src/front/`) e atualizando includes/CMake. `rc_trust.c` já está em Foundation (ARCH-02). Até lá, o mapeamento lógico acima é a fonte de verdade.

Ver também:
- [`docs/adr/001-c23-cxx-asm-plugins.md`](adr/001-c23-cxx-asm-plugins.md) (ADR-CXXASM / ADR-001: C23 no parser OSH, C++23 so em `configsh`, 10 ilhas ASM, plugins ABI C)
- `CLAUDE.md` (regras do projeto)
- `.bigtech-porte` (porte=early, variante=Pipeline-Sprint)
- `src/front/README.md` / `src/back/README.md` / `src/pudod/README.md`
- `docs/design/pudo.md` e `docs/security/`
- `docs/auditoria/aud-arch.md` (F1-F8)
- `TODO.md` (DOC-01, DOC-04, NEW-03, ADR-CXXASM, DOC-ARCH)

## Notas de qualidade

- `petrush`: 0 deps runtime alem de libc + linenoise (embutido; atribuicao em `NOTICE`) + ASM opcional. Sem `libstdc++` (ADR-CXXASM / CXX-00).
- `configsh` (futuro CXX-00): C++23, binario separado. Ver ADR-001.
- Hardening + ASan/UBSan + cppcheck + clang-tidy tuned
- TDD com acutest nas camadas mid/foundation (inclui `tests/test_rc_trust.c`; Front: `tests/test_complete.c` / `tests/test_highlight.c`)
