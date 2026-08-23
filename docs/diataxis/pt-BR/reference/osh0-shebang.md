# OSH-0: modo script e shebang

**Tipo Diátaxis:** reference  
**Audiência:** utilizador e contribuidores  
**Item:** DOC-DIA-PT · **Owner:** technical-writer · **Last-reviewed:** 2026-08-23

## Sinopse

```text
petrush <arquivo>
#!/usr/bin/env petrush
```

## Descrição

Quando `argc >= 2` e `argv[1]` não é vazio, o `main` **não** entra no REPL: chama `petrush_run_script(argv[1])` e termina. Isto cobre shebang (`env` passa o path do script como `argv[1]`).

OSH-0 **não** é o dialecto OSH completo nem um claim de conformidade POSIX 100%. É a fatia mínima: ficheiro regular, linha a linha, exit = último status.

## Comportamento garantido nesta fatia

| Comportamento | Detalhe |
|---------------|---------|
| Sem banner | Não imprime `C23 shell` / prompt |
| Sem `~/.petrushrc` | Rc do REPL não corre |
| Exit = último comando | Ex.: `/bin/false` → exit 1 |
| `exit N` no script | Encerra com N; linhas seguintes não correm |
| Ficheiro ausente | Exit 127 |
| Não regular (ex.: diretório) | Recusa; exit ≠ 0 e ≠ 127 |
| Comentários / linhas vazias | Ignorados (`#` …) |
| `argv[2+]` | Ignorados (sem `$1` nesta fatia) |
| Mode group-writable | Aceite no script mode (sem regra SEC-10 `mode&0022` do `source`) |

## Shebang recomendado

```text
#!/usr/bin/env petrush
```

Exige `petrush` no `PATH` do processo que executa o ficheiro. Alternativa: path absoluto no shebang, se instalado (instalação de sistema **não** é meta nesta máquina).

## O que OSH-0 ainda **não** faz

Lista honestamente incompleta face a IEEE 1003.1-2017 XCU cap. 2:

- parâmetros posicionais (`$1` … `$n`, `$@`)
- `if` / `while` / `for` / `case` / funções
- `$( )`, `$(( ))`, here-doc
- modo `--posix` estrito
- carregar rc no script mode

Esses itens estão no trilho OSH-1+ do plano; não os invente na documentação como se já existissem.

## Smoke

`tests/smoke/osh0-script.sh` (Docker Fedora 44 / clang no DoD do TODO).

## Ver também

- Tutorial: [Primeiro script](../tutorial/primeiro-script-osh0.md)
- Explanation: [OSH-0 não é POSIX completo](../explanation/osh0-nao-e-posix-completo.md)
- Plano: [`docs/plano-shell-avancado.md`](../../../plano-shell-avancado.md)
