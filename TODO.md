# petrush — Planejamento e Pendências

**Shell interativo em C23** (Opção A escolhida em 2026-05-27: "opcao a").

Prompt persistente, execução de comandos externos + builtins, history, rc, sinais. Ferramenta pessoal de alta qualidade, zero deps runtime.

**Escolha de arquitetura final**: Opção A — Shell Interativo REPL clássico.

---

## Tabela de pendências (canônica — formato tab_pendencias)

| ID | Grupo | Descrição Técnica | Prioridade | Pré-requisito | Dificuldade | Status | Estado Auditado |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| PR-01 | SETUP | Criar esqueleto C23 + CMakeLists.txt completo (flags -Wall -Wextra -Wpedantic -Werror, build types Debug/Release/Sanitize/Coverage, targets lint (cppcheck + clang-tidy), hardening PIE+RELRO+FORTIFY+stack-protector). Adaptar de driver_brother_hl_l1222. | Alta | — | Média | ✅ Concluído (avisos relaxados só no vendor linenoise) | — |
| PR-02 | REPL | Estrutura de diretórios 4 camadas (src/front, src/mid, src/back, src/foundation + include/petrush). main.c + stub de REPL que compila e imprime "petrush> " (usar fgets inicialmente). .gitignore + LICENSE (MIT). | Alta | PR-01 | Baixa | ✅ Concluído | — |
| PR-03 | VENDOR | Embed linenoise (vendor/linenoize/linenoise.c + .h + nota de licença/fonte). Fazer REPL migrar de fgets para linenoise com history básico em memória. | Alta | PR-02 | Média | ✅ Concluído (REPL funcional com linenoise) | — |
| PR-04 | PARSER | Implementar parser/tokenizer simples (suporte a argumentos com aspas simples/duplas básicas). Escrever testes primeiro (TDD). | Alta | PR-02 | Média | 🔄 Em andamento (primeiro teste RED criado com acutest) | — |
| PR-05 | DISPATCH | Dispatcher com tabela de builtins (function pointers). Implementar cd, pwd, echo, exit, help (mínimo). TDD obrigatório. | Alta | PR-04 | Média | 🔄 Em andamento (dispatcher + 5 builtins básicos integrados no REPL) | — |
| PR-06 | PROCESS | Foundation: spawn de processos externos (fork + execvp + waitpid) com tratamento correto de status e sinais. | Alta | PR-02 | Média | ⏳ Pendente | — |
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
- Linenoize embutido (sem libreadline por enquanto — zero deps + licença amigável).

## Anti-over-engineering (válido para todo o projeto)

- Sem pipes |, redirecionamento >, background &, globbing avançado, job control, scripting completo no v0.1.
- Parser simples (Rule of 3 antes de parser mais sofisticado).
- Framework de testes: acutest.h (decidido após análise de ROI — 2026-05-27).
- Nenhum builtin diagnóstico "petrush" no MVP (só shell básico primeiro).

---

**Próximo passo imediato**: Escrever testes para o dispatcher + refatorar/mover builtins para src/back/builtins.c (seguindo a arquitetura de 4 camadas).
