# petrush — Planejamento e Pendências

**Shell interativo em C23** (Opção A escolhida em 2026-05-27: "opcao a").

Prompt persistente, execução de comandos externos + builtins, history, rc, sinais. Ferramenta pessoal de alta qualidade, zero deps runtime.

**Escolha de arquitetura final**: Opção A — Shell Interativo REPL clássico.

---

## Tabela de pendências (canônica — formato tab_pendencias)

| ID | Grupo | Descrição Técnica | Prioridade | Pré-requisito | Dificuldade | Status | Estado Auditado |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| PR-01 | SETUP | Criar esqueleto C23 + CMakeLists.txt completo (flags -Wall -Wextra -Wpedantic -Werror, build types Debug/Release/Sanitize/Coverage, targets lint (cppcheck + clang-tidy), hardening PIE+RELRO+FORTIFY+stack-protector). Adaptar de driver_brother_hl_l1222. | Alta | — | Média | ✅ Concluído (avisos relaxados só no vendor linenoise) | — |
| PR-02 | REPL | Estrutura de diretórios 4 camadas (src/front, src/mid, src/back, src/foundation + include/petrush). main.c + stub de REPL que compila e imprime "petrush> " (usar fgets inicialmente). .gitignore + LICENSE.md (PolyForm Noncommercial 1.0.0). | Alta | PR-01 | Baixa | ✅ Concluído | — |
| PR-03 | VENDOR | Embed linenoise (vendor/linenoize/linenoise.c + .h + nota de licença/fonte). Fazer REPL migrar de fgets para linenoise com history básico em memória. | Alta | PR-02 | Média | ✅ Concluído (REPL funcional com linenoise) | — |
| PR-04 | PARSER | Implementar parser/tokenizer simples (suporte a argumentos com aspas simples/duplas básicas). Escrever testes primeiro (TDD). | Alta | PR-02 | Média | 🔄 Em andamento (primeiro teste RED criado com acutest) | — |
| PR-05 | DISPATCH | Dispatcher com tabela de builtins (function pointers). Implementar cd, pwd, echo, exit, help (mínimo). TDD obrigatório. | Alta | PR-04 | Média | 🔄 Em andamento (dispatcher + 5 builtins básicos integrados no REPL) | — |
| PR-06   | PROCESS | Foundation: spawn de processos externos (fork + execvp + waitpid) com tratamento correto de status e sinais. | Alta | PR-02 | Média | ✅ Concluído (básico + job control + 126/127 + termios + testes) | — |
| PR-06.1 | PROCESS | Implementar busca no PATH + fork + execv + waitpid básico | Alta | — | Baixa | ✅ Concluído | — |
| PR-06.2 | PROCESS | Ignorar SIGINT e SIGQUIT no processo pai enquanto o filho executa | Alta | PR-06.1 | Baixa | ✅ Concluído | — |
| PR-06.3 | PROCESS | Colocar o filho em seu próprio process group (setpgid) | Alta | PR-06.1 | Baixa | ✅ Concluído | — |
| PR-06.4 | PROCESS | Gerenciar controle do terminal (tcsetpgrp) ao entregar e devolver o terminal ao shell | Alta | PR-06.3 | Média | ✅ Concluído | — |
| PR-06.5 | PROCESS | Usar WUNTRACED no waitpid para detectar processos parados (Ctrl+Z) | Alta | PR-06.4 | Baixa | ✅ Concluído | — |
| PR-06.6 | PROCESS | Reportar terminação por sinal de forma legível (nomes dos sinais via tabela) | Média | PR-06.2 | Baixa | ✅ Concluído | — |
| PR-06.7 | PROCESS | Retornar status de saída correto no estilo shell (128 + sinal) quando processo morre por sinal | Alta | PR-06.6 | Baixa | ✅ Concluído | — |
| PR-06.8 | PROCESS | Tratar WIFSTOPPED (processos parados) e retornar status adequado | Média | PR-06.5 | Baixa | ✅ Concluído | — |
| PR-06.9 | PROCESS | Bloquear sinais durante a janela crítica do fork (sigprocmask) para evitar race conditions | Média | PR-06.2 | Baixa | ✅ Concluído | — |
| PR-06.10| PROCESS | Salvar e restaurar atributos do terminal (termios) ao manipular foreground process group | Média | PR-06.4 | Média | ✅ Concluído (petrush_init_shell_termios + restore no take_terminal_back + chamada em main.c) | — |
| PR-06.11| PROCESS | Tratar corretamente SIGTTOU / SIGTTIN ao manipular o terminal | Baixa | PR-06.4 | Baixa | ✅ Concluído (ignore temporário em give/take + restauração de handlers; suficiente para S0) | — |
| PR-06.12| PROCESS | Melhorar códigos de erro no exec (126 = permission denied, 127 = not found) | Baixa | PR-06.1 | Baixa | ✅ Concluído (shell_error_code_for + lógica em find + child exec + mensagens distintas) | — |
| PR-06.13| PROCESS | Escrever testes unitários para execute_external (cenários de sucesso, erro, sinal, stop) | Média | PR-06.1 | Média | ✅ Concluído (test_process.c com cenários de erro 126/127, init termios e contratos; sucesso/sinal via smoke) | — |
| PR-07 | ENV+RC | Gerenciamento de variáveis de ambiente + leitura e execução de ~/.petrushrc no startup + history persistente em ~/.petrush_history via linenoise. | Média | PR-05, PR-06 | Média | ⏳ Pendente | — |
| PR-08 | BUILTINS | Completar builtins MVP: export, unset, env, history, clear. Refatorar o que a Rule of 3 indicar. | Média | PR-05 | Baixa | ⏳ Pendente | — |
| PR-09 | SIGNALS | Tratamento robusto de SIGINT (não mata o shell), SIGTERM, etc. Testar com Ctrl-C durante comandos externos. | Média | PR-06 | Baixa | ⏳ Pendente | — |
| PR-10 | QUAL | Gate S0 completo: build em Sanitize limpo, smoke manual de 10+ comandos (builtins + externos), cppcheck + clang-tidy zero erros novos, valgrind (se disponível) no caminho crítico do REPL. | Alta | PR-01 a PR-09 | Alta | ⏳ Pendente | — |
| PR-11 | CI | `.forgejo/workflows/ci.yml` mínimo (compila + executa testes em pelo menos Ubuntu + um container com clang). | Média | PR-10 | Média | ⏳ Pendente | — |
| PR-12 | DOCS | README.md final (pt-br + en), build instructions testadas, exemplos de uso, "não-objetivos do v0.1", atualização do vault TODO.md (V-05). | Média | PR-10 | Baixa | ⏳ Pendente | — |

---

## Decisões registradas (2026-05-27)

- **Opção A** (Shell Interativo REPL) escolhida explicitamente.
- Regra de processo: sempre apresentar opções de design/arquitetura antes de decidir (ver CLAUDE.md do projeto + memória feedback-regra-design-opcoes).
- Repo: git@codeberg.org:petrinhu/petrush.git (Forgejo). SSH configurada.
- Stack: C23 + CMake + hardening completo + análise estática + TDD para mid/back + 4 camadas estritas.
- **Licença**: PolyForm Noncommercial License 1.0.0
  - Link oficial: https://polyformproject.org/licenses/noncommercial/1.0.0/
  - Arquivo: `LICENSE.md` na raiz do projeto
  - Definição de "Noncommercial":
    - Qualquer uso que **não** seja primariamente direcionado a vantagem comercial ou compensação monetária.
    - Inclui proibição de uso em produtos/serviços vendidos ou oferecidos gratuitamente com objetivo de gerar receita ou benefício comercial.
    - Uso pessoal, educacional, pesquisa e projetos open source sem monetização são permitidos.
  - Escolha justificada por: desejo de manter o projeto como ferramenta pessoal/comunitária sem risco de apropriação comercial.
- Linenoize embutido (sem libreadline por enquanto — zero deps + licença amigável).

## Anti-over-engineering (válido para todo o projeto)

- Sem pipes |, redirecionamento >, background &, globbing avançado, job control completo (fg/bg/jobs), scripting no v0.1. (Job control *básico* para Ctrl-C/Ctrl-Z seguro foi implementado em PR-06 como requisito mínimo.)
- Parser simples (Rule of 3 antes de parser mais sofisticado).
- Framework de testes: acutest.h (decidido após análise de ROI — 2026-05-27).
- Nenhum builtin diagnóstico "petrush" no MVP (só shell básico primeiro).

---

**Próximo passo imediato**: PR-06 (process execution + job control básico) concluído. Avançar para PR-07 (ENV + ~/.petrushrc + history persistente) ou PR-09 (sinais robustos no REPL principal). Rodar `make check` + smoke manual com Sanitize antes de PR-10.
