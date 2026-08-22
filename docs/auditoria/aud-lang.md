# AUD-LANG - Idioms C23 (petrush)

| Campo | Valor |
|-------|-------|
| **ID** | AUD-LANG |
| **Data** | 2026-08-22 |
| **SHA HEAD (pré-relatório)** | `bb8cec7` |
| **Auditor** | backend-engineer |
| **Escopo** | idioms C23 em `src/` + `include/petrush/` (+ cruzamento tests/CMake); vendor linenoise fora do julgamento de estilo |
| **Pré-reqs cruzados** | TST-T2 (`docs/memory/tst-t2-estatica.md`; cppcheck `--std=c23` + clang-tidy) |
| **Manuais** | `Projects/petrush/AUDITORIAS.md` § AUD-LANG; vault CONTRACT (preferência linguagem moderna); `docs/standards.md` (C23 seguro) |
| **Porte** | early (Pipeline-Sprint) |
| **Código de produto** | não alterado (só relatório + status TODO) |
| **Push** | não |

## 1. Método

1. Confirmar flags C23 no CMake e o que o build realmente passa (`compile_commands.json`, `flags.make`).
2. Provar `__STDC_VERSION__` com o mesmo dialeto do projeto (`-std=gnu2x`) e com `-std=c23` estrito.
3. Inventariar: `nullptr` vs `NULL`, `typeof`, `_Generic`, `static_assert`, `bool`/`_Bool`, enums, VLA/`alloca`, atributos, `_BitInt`, headers C23 (`stdckdint` etc.).
4. Cruzar TST-T2 / `.clang-tidy` (há checks `modernize`?).
5. Classificar achados; recomendar patches **sem** aplicar código de produto.

**Ferramentas:** `rg`, inspeção CMake/`compile_commands.json`, probe `gcc -std=gnu2x`/`-std=c23`, contagem Python de VLA. Logs brutos: `/var/tmp/petrush-c23-probe*`, `/var/tmp/petrush-c23-idioms*`.

## 2. CMake / toolchain C23

| Item | Evidência | Leitura |
|------|-----------|---------|
| `CMAKE_C_STANDARD 23` | `CMakeLists.txt:4` | Correto |
| `CMAKE_C_STANDARD_REQUIRED ON` | `CMakeLists.txt:5` | Correto (falha se toolchain não cobrir) |
| `CMAKE_C_EXTENSIONS` | **não** setado (default ON) | Produz **`-std=gnu2x`**, não `-std=c23` |
| Flag real (`build/…/flags.make`) | `-std=gnu2x` em `petrush` e `pudod` (118/118 entradas em `compile_commands.json`) | Dialeto GNU C2x/C23 |
| `__STDC_VERSION__` sob `gnu2x` | `202311L` | **É C23** |
| cppcheck | `--std=c23` (`CMakeLists.txt` target `cppcheck`) | Alinhado ao manual |
| clang-tidy target | usa `compile_commands` (herda `gnu2x`) | Sem checks `modernize-*` no `.clang-tidy` |
| Probe idioms | `nullptr`, `bool`/`true`/`false`, `typeof`, `static_assert` compilam em `gnu2x` e `c23` no host (gcc 16) | Toolchain **permite** os idioms; o código quase não os usa |
| `stdckdint.h` | disponível no host | Não referenciado no produto |

**Nota:** `gnu2x` = C23 + extensões GNU. `pudod` já define `_GNU_SOURCE` (necessário para APIs GNU). Desligar extensões no binário principal é opcional e não é bloqueio de AUD-LANG.

## 3. Inventário de idioms

Escopo de contagem: `src/` + `include/` (exceto vendor). Tests citados à parte.

| Idiom / superfície | Contagem / estado | Veredito |
|--------------------|-------------------|----------|
| **`nullptr`** | **0** em src/include/tests | Não adotado |
| **`NULL`** | **~180** src+include; **~244** tests (acutest/fuzz excluídos do sumário fino); **424** total no inventário desta passada | Idiom C clássico dominante |
| **`typeof` / `typeof_unqual`** | **0** | Não usado (opcional) |
| **`_Generic`** | **0** | Não usado (sem macro type-generic óbvia no domínio) |
| **`static_assert` / `_Static_assert`** | **0** | Lacuna: limites (`PETRUSH_JOB_MAX`, `PUDOD_MAX_ARGS`, …) só em `#define` + checks runtime |
| **`bool` / `<stdbool.h>` / `_Bool`** | **0** includes; tipo não aparece nas APIs públicas | Flags booleanas como `int` (0/1) |
| **Keywords C23 `true`/`false`** | builtins shell `"true"`/`"false"` e `builtin_true`/`builtin_false` | Sem conflito com keywords (strings e identificadores compostos) |
| **Enums** | `tok_kind_t` (parser interno), `petrush_run_cond_t`, `petrush_job_state_t`, `enum petrush_hl_kind` | Bom uso onde há conjunto fechado |
| **VLA** | **0** candidatos (script); **52** buffers de pilha com tamanho constante/`#define` | Conforme (sem VLA perigoso) |
| **`alloca`** | **0** | OK |
| **`_BitInt`** | **0** | N/A para shell CLI (sem aritmética de largura fixa exótica) |
| **Atributos C23 `[[…]]`** | **0** | Não usado |
| **`__attribute__((format…))`** | 2 (`pudod.c`, `pudo.c`) | GNU attribute pontual; útil |
| **`size_t`** | **126** ocorrências | Uso saudável em lengths/índices |
| **Headers próprios** | 16 em `include/petrush/`; include guards consistentes | OK |

### 3.1 `nullptr` vs `NULL`

C23 oferece `nullptr` / `nullptr_t` (probe OK no mesmo dialeto do build). O produto e os testes continuam 100% em `NULL` (sentinel de argv, checagens de ponteiro, `sigaction` oldact, etc.).

Não é bug de correção: `NULL` continua válido. É **dívida de modernização** frente ao contrato verbal “C23” e ao espelho do CONTRACT C++ (`nullptr` em vez de `NULL`). Migração big-bang (~400 sites) é risco de ruído sem ganho funcional; preferir política incremental.

### 3.2 `bool` vs `int` flags

Campos/APIs com semântica booleana tipados como `int`:

- `petrush_cmd_t`: `redir_append`, `redir_err_append`, `redir_err_to_out`
- `petrush_list_item_t`: `background`
- `petrush_job_t`: `notified`
- `petrush_source_file(..., int missing_ok)`, `petrush_setenv(..., int overwrite)`
- slots internos `g_used[]` / `g_aliases[].used`

Expressividade fraca: o tipo não documenta o domínio {0,1}. Trocar para `bool` é mecânico e seguro sob C23, mas toca headers públicos e testes; **fora desta fatia**.

### 3.3 Enumerations

Pontos fortes:

- Tokens do lexer (`TOK_*`) como enum tipado (não `#define` solto).
- Condições `&&` / `||` / `;` como `petrush_run_cond_t`.
- Estado de job e kind de highlight como enum.

Residual cosmético: vários “modos” ainda são `int` com comentário (ex.: flags de redir). Não há enum falso via macro de status além de limites numéricos legítimos (`PETRUSH_JOB_MAX`, `PUDOD_MAX_ARGS`).

### 3.4 VLA / pilha

Nenhum VLA detectado. Padrão do projeto: `char buf[N]` com `N` literal ou macro (`PATH_MAX`, `4096`, `PUDOD_MAX_ARGS + 2`, `LOG_BUF_SIZE`). Fail-closed em estouro de argc (SEC-04 / `PUDOD_MAX_ARGS`) em vez de VLA dimensionada pelo input. **Alinhado a “sem VLA perigoso” do AUDITORIAS.md.**

### 3.5 `static_assert`, `_Generic`, `typeof`

| Recurso | Oportunidade concreta (não aplicada) |
|---------|--------------------------------------|
| `static_assert` | `PUDOD_MAX_ARGS == 128` coerente com buffer; `sizeof(g_jobs)/sizeof(g_jobs[0]) == PETRUSH_JOB_MAX`; `PETRUSH_SOURCE_MAX_DEPTH > 0`; capacidade `out_cap >= PUDOD_MAX_ARGS + 2` em contratos de teste |
| `_Generic` | Pouca superfície: não há família de funções overloaded óbvia; custo > benefício no porte early |
| `typeof` | Útil em macros de container; o código prefere tipos explícitos (legível sob `-Wconversion`) |

### 3.6 clang-tidy / TST-T2

`.clang-tidy` cobre `bugprone-*`, `performance-*`, `readability-*`, `misc-*`, analyzers. **Não** habilita `modernize-*` (e de qualquer modo o catálogo modernize é majoritariamente C++).

Consequência: **TST-T2 verde não prova adoção de idioms C23**; só prova ausência de classes de bug cobertas pelos checks atuais. Gap de tooling = higiene, severidade IMPORTANTE baixa (não bloqueia release early).

## 4. Achados

Severidade vault: **CRÍTICO** / **IMPORTANTE** / **COSMÉTICO**.

### L1 - IMPORTANTE - `nullptr` ausente; `NULL` dominante

| | |
|--|--|
| **Onde** | Todo `src/` / `include/` / `tests/` (0 `nullptr`; centenas de `NULL`) |
| **Regra** | AUD-LANG: idioms C23; toolchain já aceita `nullptr` |
| **Impacto** | Dívida de estilo/modernidade; sem impacto funcional/segurança medido |
| **Mitigação** | Política: código novo e linhas tocadas preferem `nullptr`; conversão mecânica só com suíte verde e commit próprio (não misturar com feature) |

### L2 - IMPORTANTE - `bool` não usado para flags booleanas

| | |
|--|--|
| **Onde** | `parser.h` (redir_*), `job.h` (`notified`), `source.h` / `env.h` (params), aliases/jobs `used` |
| **Impacto** | Tipos menos expressivos; risco cosmético de atribuir valores fora de {0,1} sem aviso do tipo |
| **Mitigação** | Fatia dedicada: headers → impl → tests; manter ABI de retorno `int` de status (0/-1) **distinta** de flags `bool` |

### L3 - IMPORTANTE - zero `static_assert` em invariantes de limite

| | |
|--|--|
| **Onde** | Limites `#define` em `job.h`, `expand.h`, `source.h`, `pudo.h`, `pudod/child_argv.h` |
| **Impacto** | Drift entre macro, tamanho de array estático e check runtime só aparece em teste/runtime |
| **Mitigação** | `static_assert` ao lado dos arrays/`#define` (baixo risco, alto sinal) |

### L4 - IMPORTANTE (tooling) - gate lint não olha modernidade C23

| | |
|--|--|
| **Onde** | `.clang-tidy` sem política de idiom; cppcheck `--std=c23` não força `nullptr`/`bool` |
| **Impacto** | Regressão de estilo silenciosa; AUD-LANG precisa ser manual |
| **Mitigação** | Doc de convenção em `docs/standards.md` ou `CLAUDE.md`; opcional script `rg` de CI que falha se arquivo **novo** introduzir `NULL` sem justificativa (só se o líder quiser gate duro) |

### L5 - COSMÉTICO - dialeto `gnu2x` em vez de `c23` estrito

| | |
|--|--|
| **Onde** | Default `CMAKE_C_EXTENSIONS` ON |
| **Impacto** | Nenhum na prática (`__STDC_VERSION__` 202311); `pudod` precisa GNU |
| **Mitigação** | Documentar; opcional `CMAKE_C_EXTENSIONS OFF` só no target `petrush` se um dia quiser pureza ISO |

### L6 - COSMÉTICO - `typeof` / `_Generic` / `[[nodiscard]]` / `_BitInt` ausentes

Sem caso de uso urgente no domínio shell. `_BitInt` fora de escopo. `[[nodiscard]]` em APIs que devolvem status poderia ajudar, mas o projeto já trata retornos com disciplina razoável sob testes.

### L7 - OK - sem VLA perigoso / sem `alloca`

Buffers de pilha dimensionados por constante; estouro de argc fail-closed.

### L8 - OK - enums nos núcleos parser/job/highlight

Conjuntos fechados modelados com tipo enum, não strings mágicas.

### L9 - OK - CMake declara C23 e o compilador entrega C23

`CMAKE_C_STANDARD 23` + `REQUIRED ON` + `gnu2x` com `202311L` + cppcheck `--std=c23`.

## 5. Score e veredito

| Dimensão | Nota (0-20) | Nota |
|----------|-------------|------|
| Toolchain / flags C23 | 18 | STANDARD 23 + REQUIRED; `gnu2x` ≡ C23; cppcheck c23 |
| Segurança de arrays (anti-VLA) | 19 | 0 VLA; 0 alloca; limites fail-closed |
| Enums / tipos de domínio | 15 | Enums certos no parser/job/hl; flags ainda `int` |
| Idioms C23 (`nullptr`, `bool`, `static_assert`, attrs) | 8 | Toolchain ok; adoção quase nula |
| Tooling que reforça idiom | 11 | TST-T2 não cobre modernidade; sem modernize C |
| **Total** | **71 / 100** | |

**Veredito:** **APROVADO COM RESSALVAS** para porte early.

O projeto **compila e lintá como C23**, evita VLA, e usa enums onde importa. Não está **escrito** no idioma C23 moderno (`nullptr` / `bool` / `static_assert`). Residual é modernização incremental, não defeito de correção bloqueante.

**Não marca ✅** (só 🔍). ✅ fica para julgamento do orquestrador / AUD-REPORT.

## 6. Patches recomendados (ordem; não aplicados)

1. **`static_assert` nos limites** (baixo risco): `PETRUSH_JOB_MAX`, `PUDOD_MAX_ARGS`, `PETRUSH_GLOB_MAX`, `PETRUSH_SOURCE_MAX_DEPTH` vs arrays estáticos.
2. **Política `nullptr` incremental**: código novo / linhas tocadas; sem big-bang nesta onda.
3. **`bool` em flags de struct/params** (fatia própria + TDD): `redir_*`, `background`, `notified`, `missing_ok`, `overwrite`, `used`.
4. **Doc**: 1 parágrafo em `docs/standards.md` “C23 idioms neste repo” (nullptr, bool, static_assert, anti-VLA, gnu2x consciente).
5. **Opcional**: `CMAKE_C_EXTENSIONS OFF` no target `petrush` (manter GNU em `pudod`); só com gate de build verde.

## 7. Relação com outros itens

| ID | Relação |
|----|---------|
| **TST-T2** | Pré-req; prova lint C23, **não** prova idioms |
| **AUD-QUALITY** | Encaminhou cognitive/NOLINT; LANG não reabre CCN |
| **AUD-SEC** | Anti-VLA reforça postura de memory safety |
| **AUD-DEPS** | Vendor linenoise fora do estilo C23 do produto |
| **AUD-REPORT** | Consolidar score 71 + patches 1-5 |

## 8. Evidências

- `CMakeLists.txt`: `CMAKE_C_STANDARD 23`, `REQUIRED ON`, cppcheck `--std=c23`
- `build/CMakeFiles/petrush.dir/flags.make` e `pudod.dir/flags.make`: `-std=gnu2x`
- `build/compile_commands.json`: 118/118 com `-std=gnu2x`
- Probe: `__STDC_VERSION__=202311L`; `nullptr`/`bool`/`typeof`/`static_assert` OK
- Contagens: `rg` `nullptr`=0; `NULL`≈424 (src+include+tests filtrados); VLA=0; `static_assert`=0; `typeof`=0; `_Generic`=0; `stdbool`=0
- Enums: `include/petrush/{parser,job,highlight}.h`, `src/mid/parser.c` (`tok_kind_t`)
- Critério: `Projects/petrush/AUDITORIAS.md` § AUD-LANG

## 9. Checklist de saída AUD-LANG

- [x] Flags C23 no CMake + dialeto real do build
- [x] Inventário nullptr/NULL, typeof, _Generic, static_assert, bool, enum, VLA
- [x] Cruzamento TST-T2 / clang-tidy
- [x] Achados classificados + score
- [x] Relatório em `docs/auditoria/aud-lang.md`
- [ ] Status TODO `🔍` (este commit)
- [ ] ✅ só após julgamento do orquestrador / fechamento AUD

*Fim AUD-LANG. Sem alteração de código de produto. Sem push. Sem em-dash.*
