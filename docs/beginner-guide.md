# Guia para iniciantes em computação - petrush

> **Tipo Diátaxis:** explanation + quick-start (um documento, audiência única).  
> **Audiência:** iniciante em terminal / C (sem jargão sem definição).  
> **Versão do produto:** pós UX-16..23 (caminho para `v0.5`; tag ainda não publicada nesta fatia).  
> **Owner:** technical-writer · **Last-reviewed:** 2026-08-22 · **Item:** NEW-23 (parcial: só este `.md`; wiki GitHub depois da tag)

**Bem-vindo.** Este guia explica o petrush **do zero**. petrush é um *shell* (interpretador de comandos) escrito em C23: você digita uma linha, ele executa e mostra o resultado. Parece um bash ou zsh **mínimo**, feito para uso pessoal e estudo, **não** um clone POSIX completo.

## O que é um shell / REPL?

- **Shell:** programa que lê o que você digita, executa programas externos (`ls`, `cat`) ou comandos internos (*builtins*: `cd`, `echo`, `jobs`…) e mostra a saída.
- **REPL:** *Read-Eval-Print Loop* - lê uma linha → avalia/executa → imprime → repete.
- Exemplo: você digita `ls`; o shell procura `/bin/ls` (ou outro caminho no `PATH`), cria um processo filho (`fork` + `exec`), espera e mostra o texto.

petrush é um REPL em **C23** (linguagem compilada, sem *garbage collector*).

## Conceitos básicos

- **Compilar:** transformar código `.c` em binário com `cmake` + `cmake --build`.
- **CMake:** lê `CMakeLists.txt` e gera o build (compilador, flags, testes).
- **Hardening:** flags de segurança no compilador (`-fstack-protector`, PIE, `FORTIFY_SOURCE`…) para dificultar exploits.
- **Sanitize (ASan/UBSan):** build que detecta erros de memória em tempo de execução.
- **Lint:** análise estática (`clang-tidy`, `cppcheck`) sem rodar o programa.
- **Valgrind:** ferramenta que procura vazamentos e uso inválido de memória.
- **TDD:** escrever o teste primeiro, depois o mínimo de código que o faz passar.
- **Fork/exec:** como o shell roda externos: `fork()` clona o processo; `exec` troca a imagem pelo programa; `wait` espera terminar.
- **Sinais:** avisos do sistema (ex.: `SIGINT` = Ctrl-C). petrush trata para o prompt não morrer.
- **PATH:** lista de pastas onde o shell procura comandos (`/usr/bin:/bin:…`).
- **rc file:** `~/.petrushrc` - script opcional lido no início (estilo `.bashrc`, com as regras do `source` abaixo).
- **History:** comandos digitados (biblioteca *linenoise* embutida).
- **`pudo`:** nosso “sudo” do projeto. O shell (sem root) chama o helper `pudod`, que só roda o que a allow-list permitir.

## Como o petrush funciona (visão simples)

1. `main()`: banner, estado do terminal, sinais, history e `~/.petrushrc`.
2. Loop REPL: `linenoise` lê a linha (edição, setas, Tab, Ctrl-R, highlight).
3. Antes do próximo prompt: reap de jobs em background e aviso `Done` (UX-23).
4. Parse: quebra a linha em palavras, pipes, redirs, listas (`&&` `||` `;`) e `&`.
5. Expansões: `~`, `$VAR`, glob `*`/`?` em tokens **sem aspas**.
6. Dispatch: builtin no processo atual, ou externo / pipeline / job em background.
7. `pudo`: limpa o ambiente perigoso no filho e chama `pudod` com argv sanitizado.

## Build e testes (do zero)

```bash
# 1. Clone e entre
git clone git@github.com:petrinhu/petrush.git
cd petrush

# 2. Configure + build (Release)
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 3. Rode
./build/petrush

# 4. Gate completo (lint + testes + smoke + valgrind quando aplicável)
cmake --build build --target verify
```

- Só unitários: `cmake --build build --target check`
- Só smoke (binário real via pipe): `cmake --build build --target smoke`
- Limpar e reconfigurar: `cmake --build build --target clean-build` e depois configure de novo

CI no GitHub Actions (Fedora e outras distros). Não precisa de setuid para desenvolver nem para a maior parte dos testes.

## Comandos e recursos que existem hoje

### Builtins úteis

| Comando | O que faz (resumo) |
|---|---|
| `cd` / `pwd` | muda / mostra diretório (`cd -` volta ao anterior) |
| `echo` / `export` / `env` | imprime; define variável; lista ambiente |
| `history` / `help` / `exit` | histórico; ajuda; sai |
| `alias` / `unalias` / `which` | atalhos; desfaz; “onde está este comando?” |
| `pushd` / `popd` / `dirs` | pilha de diretórios |
| `source` / `.` | roda um arquivo **neste** processo (ver limites) |
| `jobs` | lista jobs em background |
| `true` / `false` / `:` | status 0 / 1 / 0 (silenciosos) |
| `umask` / `read` / `test` / `[` | máscara octal; lê 1 linha → 1 var; testes curtos de arquivo/string |
| `pudo` | elevação via `pudod` + allow-list |
| `info` | diagnóstico leve do shell |

Externos do `PATH` (`ls`, `cat`, `gcc`…) funcionam como em qualquer shell.

### Listas, pipes e redirecionamento

- **Pipe** `|` - liga stdout de um estágio à stdin do próximo.  
  Em pipe com **2+** estágios, builtins também rodam em processo filho: `cd`/`export`/`exit` **no pipe não mudam** o shell pai (UX-19).
- **Listas**  
  - `&&` / `||` - curto-circuito (só roda o próximo se o anterior passou / falhou).  
  - `;` - sempre roda o próximo (UX-17).
- **Stdout / stdin** - `>` cria/trunca (com noclobber: falha se o arquivo já existe), `>>` anexa, `<` lê de arquivo.
- **Stderr (UX-16)** - `2>` e `2>>` para arquivo; `2>&1` junta stderr no stdout; `&>` manda os dois para o mesmo arquivo.  
  **Atenção:** `&>` é **redirecionamento**, não background.

### Glob simples (UX-18)

- Em tokens **sem aspas**, `*` e `?` expandem para nomes de arquivo (depois de `~` e `$VAR`).
- Entre aspas (`"*.c"`) o padrão fica **literal**.
- Zero matches: o padrão permanece (não some).
- **Não há** classes `[…]`, nem `**` recursivo, nem glob no caminho de redirecionamento.
- Teto de matches: se passar do limite interno, falha de forma fechada (não explode a linha).

### `source` / `.` (UX-22)

```bash
source ./meus-aliases.sh
. ./meus-aliases.sh
```

- Roda o arquivo **linha a linha no processo atual** (variáveis e `cd` ficam).
- Caminho **explícito** (relativo ou absoluto). **Não** procura no `PATH`.
- Exige exatamente um argumento (o arquivo). Sem `$1`…`$n`, sem `return` de função.
- Teto de profundidade aninhada: **8**. Arquivo ausente → erro.
- `~/.petrushrc` usa o mesmo motor (arquivo opcional: sumir não é erro).

### Background e `jobs` (UX-23)

```bash
/bin/sleep 30 &
jobs
```

- Sufixo `&` manda o item para background; o prompt volta na hora.
- Builtin `jobs` lista o que ainda está na tabela (`Running` / `Done`).
- Ao voltar ao prompt, jobs terminados podem mostrar algo como `[N]+ Done  …`.
- Teto: **16** jobs. Stdin de job em TTY sem `<` vai para `/dev/null`.
- **Ainda não há:** `fg`, `bg`, Ctrl-Z, `%n`, builtin `wait`. Job control é **mínimo** de propósito.

### Edição no prompt (UX-20 / UX-21 e onda UX anterior)

- **Setas** e edição de linha (linenoise).
- **Tab** - completa builtins, PATH e arquivos.
- **History hints** - “fantasma” do comando antigo enquanto você digita um prefixo.
- **Ctrl-R** - busca reversa no histórico (substring, do mais novo ao mais velho). Enter aceita; ESC cancela. Sem Ctrl-S, sem regex, sem modo vi.
- **Highlight mínimo** - cores CSI para aspas (fechadas / abertas) e tokens grossos (comando vs operador). `NO_COLOR` desliga. Não é highlighter completo (sem keywords, sem `$VAR` colorido, sem `#` comentário).

### Prompt customizável

```bash
export PETRUSH_PS1='\u@\h:\w\$ '
```

Escapes: `\w` (cwd), `\u`, `\h`, `\n`, `\$`, `\\`. Default continua `petrush> `.

### History bangs

- `!!` e `!n` - repetem evento.
- `!$` / `!^` - último / primeiro argumento do último evento.
- Sem `!str`, sem modifiers (`:h`, `:t`).

## O que **não** é (honestidade POSIX)

petrush **não** promete “POSIX 100%”. Em especial, **ainda não** (ou de propósito nunca neste recorte):

| Tema | Situação no petrush |
|---|---|
| `fg` / `bg` / Ctrl-Z / `%n` / `wait` | fora do escopo UX-23 |
| Glob `[a-z]` / `**` / glob em redir | fora (só `*` `?` unquoted) |
| `source` via PATH, `$1`, `return` | fora |
| `[[ … ]]`, `-a`/`-o`/`!` no `test` | fora (só primaries curtos) |
| `set -C` / `set -o noclobber` | noclobber de `>`/`2>` já está sempre ligado |
| `printf` builtin, command substitution, funções, `if`/`for` | fora deste produto mínimo |
| lastpipe / `pipefail` / `PIPESTATUS` | fora ou só no backlog |

Use bash/zsh quando precisar do dialeto completo. petrush é shell **educativo e pessoal**, com superfície pequena e auditável.

## Segurança e `pudo`

`pudo` é como um sudo **do projeto**, mais estreito:

1. petrush **nunca** precisa rodar como root.
2. Chama o helper `pudod` (binário pequeno e separado).
3. `pudod` só executa caminhos listados na allow-list root-only (ex.: `/etc/petrush/pudo.allow`).
4. Ambiente perigoso (`LD_PRELOAD` etc.) é limpo no caminho privilegiado, não “desligado” no seu shell pai.

**Nunca** faça `chown root` + `chmod 4755` no `pudod` sem ler antes:

- [`docs/security/pudod-install.md`](security/pudod-install.md)
- [`docs/security/pudo-audit.md`](security/pudo-audit.md)

E sem seguir o `DEPLOY_CHECKLIST` do vault + aprovação do líder. Setuid em sistema é operação irreversível de segurança.

## Dicas de troubleshooting

- Build falha? Tente Debug: `-DCMAKE_BUILD_TYPE=Debug`.
- Suspeita de memória? Build Sanitize + target `verify` / valgrind.
- `pudo` nega? Só entra caminho absoluto na allow-list (como root), e o binário precisa existir e ser executável.
- Cores estranhas no prompt? Exporte `NO_COLOR=1`.
- Terminal “quebrado” depois de um editor? petrush restaura *termios* ao voltar.
- `>` falhou em arquivo que já existe? É o noclobber sempre ligado; use `>>` ou outro nome.

## Por que C23 + CMake + hardening?

- **C23:** C moderno, previsível, próximo do sistema.
- **CMake:** um jeito só de buildar e testar.
- **Hardening + Sanitize + Valgrind + Lint:** menos bugs de memória e superfície menor para auditoria (incluindo o caminho `pudo`/`pudod`).

Se você é iniciante total:

1. Aprenda terminal, `PATH` e a ideia fork/exec (procure “unix fork exec” em pt).
2. Depois leia o código nesta ordem: `src/main.c` → dispatcher → `process` / `job` → `pudo`/`pudod`.

## Glossário rápido

- **argv / argc:** lista de argumentos + quantos são (padrão C).
- **fork / exec / wait:** trio clássico Unix para filhos.
- **termios:** modo do terminal (eco, raw vs cooked).
- **job:** processo (ou grupo) que o shell acompanha; aqui, só background mínimo.
- **ASan / UBSan:** sanitizers de endereço / comportamento indefinido.
- **FORTIFY_SOURCE / stack-protector / PIE / relro:** mitigações de binário.
- **acutest:** framework mínimo de testes em C (header-only).
- **linenoise:** leitura de linha com history/edição (embutida; patch Ctrl-R / highlight).

## Próximos passos

- Código: comece por `src/main.c`.
- Sanitize local:  
  `cmake -B build-sanitize -S . -DCMAKE_BUILD_TYPE=Sanitize && cmake --build build-sanitize --target verify`
- Smoke: `tests/smoke/pudo-smoke.sh` (também via target `smoke`).
- CI: `.github/workflows/ci.yml`
- Padrões da casa: [`docs/standards.md`](standards.md) (aponta CONTRACT / TESTES / AUDITORIAS…).
- Wiki GitHub: atualização **depois** da tag `v0.5` (mesma fatia NEW-23; este arquivo é a fonte).

Se algo não fizer sentido, pergunte. O projeto foi feito para ser simples e educativo.

---

*NEW-23 parcial - só `docs/beginner-guide.md`. Wiki nativa do GitHub fica para o orquestrador após a tag. Sem em-dash. Sem inventar POSIX completo.*
