# Guia para Iniciantes em Computação — petrush

**Bem-vindo!** Este guia explica **tudo do zero**, sem assumir conhecimento prévio. petrush é um "shell" (interpretador de comandos) escrito em C23, como um "bash" ou "zsh" mínimo e seguro feito do zero para uso pessoal.

## O que é um Shell / REPL?

- **Shell**: Programa que lê comandos que você digita, executa programas externos (como `ls`, `cat`) ou comandos internos ("builtins" como `cd`, `echo`), e mostra o resultado.
- **REPL**: Read-Eval-Print Loop — loop infinito: lê linha → avalia/executa → imprime resultado → repete.
- Exemplo: você digita `ls`, o shell acha o programa `/bin/ls` no PATH, executa via fork+exec, espera, mostra output.

petrush é um REPL em C23 (linguagem de baixo nível, compilada, sem "garbage collector").

## Conceitos Básicos (explicados)

- **Compilar**: Transformar código fonte (.c) em binário executável usando `cmake` + `make` (ou `cmake --build`).
- **CMake**: Ferramenta que gera arquivos de build (Makefile) a partir de `CMakeLists.txt`. Configura compilador, flags, testes.
- **Hardening**: Flags de segurança no compilador ( -fstack-protector, -pie, FORTIFY_SOURCE ) para dificultar exploits.
- **Sanitize (ASan/UBSan)**: Build especial que detecta erros de memória (use-after-free, buffer overflow) em tempo de execução.
- **Lint**: Análise estática (clang-tidy, cppcheck) que aponta bugs/estilo sem rodar o programa.
- **Valgrind**: Ferramenta que detecta vazamentos de memória e uso inválido.
- **TDD**: Test-Driven Development — escrever teste primeiro, depois código que faz passar.
- **Fork/Exec**: Como shell executa comandos: fork() clona processo, exec() substitui imagem por novo programa, wait() espera terminar.
- **Sinais (signals)**: Notificações do SO (SIGINT = Ctrl-C). petrush trata para não quebrar o prompt.
- **PATH**: Variável de ambiente com diretórios onde o shell procura comandos ( /usr/bin:/bin:... ).
- **rc file**: Arquivo de configuração (~/.petrushrc) executado no início (como .bashrc).
- **History**: Lembrete de comandos digitados (usando linenoise library embutida).
- **`pudo`**: Nosso "sudo" seguro. petrush (sem privilégios) chama `pudod` (helper mínimo setuid) que re-valida tudo e executa só comandos permitidos em /etc/petrush/pudo.allow.

## Como o petrush funciona (passo a passo)

1. `main()` : imprime banner, salva termios (estado do terminal), configura sinais, carrega history e rc.
2. Loop REPL: `linenoise("petrush> ")` lê linha (com edição).
3. Parse: `petrush_parse()` quebra em argv (lida com aspas simples/duplas).
4. Dispatch: `dispatch_command()` vê se builtin (cd, echo...) ou externo.
   - Builtins: executam dentro do processo (cd usa chdir).
   - Externo: `execute_external()` faz fork, dá terminal para filho, execv, espera, restaura terminal.
5. `pudo`: builtin especial que sanitiza env perigoso (LD_PRELOAD etc), checa allow-list client, chama pudod com argv limpo.
6. pudod (helper): roda como root (setuid), re-checa tudo (realpath, fstat, allow-list root), constrói env mínimo, fexecve o comando alvo.

## Build e Testes (do zero)

```bash
# 1. Clone e entre
git clone ... petrush
cd petrush

# 2. Configure + build (usa Release por padrão)
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 3. Rode
./build/petrush

# 4. Gate completo (lint + smoke + valgrind) — TUDO automatizado
cmake --build build --target verify
```

- `clean-build`: `cmake --build build --target clean-build` (depois re-configure).
- Testes unitários: parte do verify ou `cmake --build build --target check`.

## Comandos comuns no petrush

- `cd /caminho` — muda diretório (builtin)
- `pwd` — mostra diretório atual
- `echo texto` — imprime
- `export VAR=valor` — define variável de ambiente
- `env` — lista variáveis
- `history` — mostra histórico
- `pudo comando` — executa com privilégios (via pudod)
- Qualquer outro: executa binário do PATH (ex: `ls`, `cat`, `gcc`)

## Segurança e `pudo` (explicado simples)

`pudo` é como sudo, mas mais seguro para este projeto:
- petrush nunca roda como root.
- Chama `pudod` (programa pequeno separado).
- `pudod` só permite comandos listados em `/etc/petrush/pudo.allow` (arquivo root-only).
- Sanitiza variáveis perigosas (LD_PRELOAD etc) que podem ser usadas para hacks.

**Nunca** dê chmod 4755 no pudod sem ler os docs de segurança primeiro!

## Dicas de troubleshooting

- Build falha? Rode com Debug: `-DCMAKE_BUILD_TYPE=Debug`
- Erros de memória? Use build Sanitize + valgrind.
- `pudo` nega comando? Adicione o caminho absoluto no allow-list (como root).
- Terminal bugado após editor? petrush restaura termios automaticamente.

## Por que C23 + CMake + hardening?

- C23: moderno, seguro.
- CMake: fácil build, testes, multi-plataforma.
- Hardening + Sanitize + Valgrind + Lint: código de alta qualidade, menos bugs de segurança.

Se você é iniciante total:
- Aprenda primeiro: o que é terminal, PATH, fork/exec (procure "unix fork exec tutorial" em pt).
- Depois leia o código: comece por `src/main.c`.

Boa sorte! Este projeto foi feito para ser simples e educativo.

(Conteúdo para wiki + docs iniciante — NEW-13. Publique na wiki do GitHub copiando este guia + links para código.)
- Smoke: `cmake --build build --target smoke` (roda comandos reais via pipe, checa output).

## Segurança e `pudo`

Nunca rode `sudo chown root:root build/pudod; sudo chmod 4755 ...` sem ler:

- `docs/security/pudod-install.md`
- `docs/security/pudo-audit.md`

O pudod é **mínimo** (<300 LOC efetivo) para ser auditável. Allow-list é em arquivo root-only.

## Jargão comum (glossário rápido)

- **argv/argc**: array de strings + contador de argumentos (padrão C para main e exec).
- **fork/exec/wait**: trio clássico Unix para rodar programas filhos.
- **termios**: estrutura que controla modo do terminal (raw vs cooked, echo on/off).
- **setpgid / tcsetpgrp**: controle de jobs (quem tem o terminal).
- **ASan/UBSan**: AddressSanitizer / UndefinedBehaviorSanitizer (detectam bugs de memória).
- **FORTIFY_SOURCE / stack-protector / PIE / relro**: mitigações contra exploits.
- **acutest.h**: framework mínimo de testes em C (header-only).
- **linenoise**: biblioteca pequena para leitura de linha com history/edição (embutida).

## Próximos passos (se quiser contribuir ou entender mais)

- Leia código em ordem: main.c → mid/dispatcher.c → foundation/process.c
- Rode com sanitize: `cmake -B build-sanitize -S . -DCMAKE_BUILD_TYPE=Sanitize ; cmake --build build-sanitize --target verify`
- Veja smoke script: `tests/smoke/pudo-smoke.sh`
- Para CI: `.github/workflows/ci.yml`

Se algo não fizer sentido, pergunte! O projeto é feito para ser simples e educativo.

Boa sorte explorando o seu primeiro shell "do zero"!

(Conteúdo gerado como parte da documentação obrigatória para iniciantes — NEW-13)