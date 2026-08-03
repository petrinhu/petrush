# Auditoria de Segurança: `pudo` (NEW-05)

**Data**: 2026-07-01 (continuação da sessão)
**Escopo**: Código atual (`src/mid/pudo.c`, `tests/test_pudo.c`, `include/petrush/pudo.h`), integração via `dispatcher.c`, design em `docs/design/pudo.md`.
**Classificação**: Crítico (código que pode levar a execução com privilégios elevados).
**Responsável pela auditoria inicial**: Engineer (com princípios de Narciso/CISO).
**Status**: Auditoria inicial + revisão aprofundada (NEW-05-01). Gate obrigatório antes de helper setuid.

## Resumo Executivo

A arquitetura de alto nível (Fase 0: delegar para `/usr/bin/sudo` real) **está alinhada** com os princípios do design doc e é a abordagem mais segura para uma ferramenta pessoal no momento.

A implementação evoluiu desde a auditoria inicial:
- Separator `--` agora está presente (boa).
- Logging básico via syslog + stderr implementado.
- `load_pudo_config` funcional (carrega linhas, ignora comentários).
- Busca de sudo em múltiplos paths + fallback.

**Ainda assim, NÃO está pronto para Fase 1 (helper setuid próprio)**. Restam problemas críticos de TOCTOU, mutação de ambiente do processo, validações fracas de allow-list, e ausência de re-validação no lado privilegiado.

Risco atual com sudo real: limitado ao que o `sudo` do sistema permite ao usuário atual (boa defesa em profundidade, desde que o sudoers do usuário seja restrito).

Risco ao introduzir helper setuid: **alto** se o helper não for mínimo, validado rigorosamente, com allow-list **enforçada no lado root** e sem confiar no petrush.

**Não está pronto para Fase 1 (helper setuid próprio)** sem correções prévias e auditoria adicional.

Risco atual com sudo real: limitado ao que o `sudo` do sistema permite ao usuário atual (boa defesa em profundidade).

Risco ao introduzir helper setuid: **alto** se o helper não for mínimo, validado rigorosamente e com allow-list enforçada no lado privilegiado.

## Princípios de Design (reafirmados)

- Separação de privilégios (petrush nunca privilegiado).
- Menor privilégio no tempo.
- Superfície mínima no componente com root.
- Auditoria forte.
- Zero parsing complexo no lado privilegiado.

## Threat Model (STRIDE resumido para `pudo`)

| Categoria | Ameaça | Vetor atual | Severidade | Mitigação existente | Mitigação recomendada |
|-----------|--------|-------------|------------|---------------------|-----------------------|
| **Spoofing** | Usuário falsifica identidade | N/A (roda como caller) | Baixa | - | - |
| **Tampering** | Modificar allow-list ou config | Arquivo de config não implementado | Média | Nenhum | Root-owned config, assinatura ou path fixo |
| **Repudiation** | Negar execução de comando privilegiado | Sem logs | Alta | Nenhum | syslog + uid real no helper |
| **Information Disclosure** | Vazamento de env ou segredos via LD_* | Sanitização | Média-Alta | `pudo_sanitize_environment()` (boa) | Aprimorar + passar env limpo explicitamente via execve |
| **Denial of Service** | Comando que trava TTY ou consome recursos | Herda do sudo | Média | - | Limites (ulimit via sudoers ou wrapper) |
| **Elevation of Privilege** | Escalar além do pretendido | 1. Bypass allow-list<br>2. Injeção de args<br>3. Env poisoning<br>4. Bug no parser<br>5. TOCTOU no helper | **Crítica** | Sanitização + parser tokenizado + sudo | 1. Allow-list rígida no helper também<br>2. `--` separator + argv seguro<br>3. Validação absoluta + realpath no helper<br>4. Protocolo mínimo |

Principais vetores clássicos de sudo-like bugs ainda presentes em grau variável.

## Achados Detalhados (Código Atual - Fase 0)

### 1. Críticos / Altos

- **[PARCIALMENTE RESOLVIDO]** `run_via_real_sudo` agora usa `--` separator explicitamente:
  ```c
  sudo_argv[idx++] = "--";   /* CRÍTICO: separa opções do comando do usuário */
  ```
  Isso previne injeção de flags para o sudo. No entanto, o design atual trata *todo* argumento após "pudo" como parte do comando alvo (não há suporte a opções do sudo como `-u`, `-E`). Isso é mais seguro, mas UX diferente do sudo clássico — documentar.

- **[PARCIALMENTE RESOLVIDO]** Config / allow-list:
  - `load_pudo_config` agora implementa leitura de arquivo por usuário (`~/.config/petrush/pudo.conf`).
  - `is_command_allowed` faz strcmp exato.
  - **Problemas remanescentes (críticos para Fase 1)**:
    - Load acontece **antes** da sanitização de ambiente (usa HOME via petrush_getenv sem canonicalizar).
    - Matching é *exato* e sem normalização (realpath, basename, absolutos). `pudo id` vs `/bin/id` vs `./id` diferem.
    - Quando não há config, permite tudo (comportamento Fase 0 permissivo, ok porque sudo real ainda filtra).
    - Nenhum arquivo de exemplo versionado no repositório (design menciona `pudo.conf.example` mas não está no tree).
    - Dois-pass (count + rewind) cria TOCTOU na leitura da config (baixo impacto em Fase 0).

- **[RESOLVIDO]** Logging / auditoria básico implementado via `pudo_log` (stderr + syslog(LOG_AUTH|LOG_INFO)). Ainda rudimentar: não loga argv completo, não inclui tty/pid/session de forma estruturada.

- **Hardcoded paths + buffers fixos (ainda válidos)**:
  - Paths de sudo agora buscados em candidatos (bom).
  - Buffer `char *sudo_argv[128]` (reduzido de 256) — ainda truncamento silencioso se >~125 args.
  - `line[512]` no parser de config, `buf[256]` no log — truncam.

- **execv/execvp sem env explícito + mutação global (NOVO - Crítico)**:
  - `sanitize_environment()` usa `unsetenv` diretamente → **altera o ambiente do processo petrush permanentemente**.
  - Após qualquer `pudo ...`, o shell inteiro perde LD_*, PYTHONPATH etc. para comandos subsequentes.
  - Comentário no código diz "Usa execve com ambiente mínimo", mas na prática usa execv e confia na mutação.
  - Violação de "menor privilégio no tempo" para o shell: o efeito colateral dura além da invocação elevada.
  - Para Fase 1: o pudod **nunca** deve confiar no env herdado; deve construir envp limpo explicitamente. O frontend também deve parar de mutar.

### 2. Médios

- **Testes de segurança são frágeis**:
  - `test_pudo_*` chamam `setenv`/`unsetenv` reais e afetam o processo de teste inteiro (poluição global).
  - Vários testes ainda placeholders (`TEST_CHECK(1)`).
  - Não há teste do caminho de execução real (que envolve sudo externo). Mock difícil sem refator para injeção de dependências.
  - Novos testes necessários para: (a) não-mutação de env (ou mutação controlada), (b) load config com HOME longo/truncado, (c) allow-list matching com paths variantes.

- **free_pudo_config + global state**:
  - `memset` após free OK, mas `g_pudo_config` é global estático. Se load falha no meio (calloc parcial), estado pode ficar inconsistente em re-invocações no mesmo processo shell.

- **Sanitização incompleta + mutação (ver Crítico acima)**:
  - Lista boa, mas faltam: `JAVA_TOOL_OPTIONS`, `GTK_PATH`, `QT_PLUGIN_PATH`, `GIO_LAUNCHED_DESKTOP_FILE`, vars de pager (LESS, PAGER com pipes?), `TMPDIR` manipulado, etc.
  - Não canicaliza HOME/USER antes de usar em paths (load config).

- **Tratamento de erro fraco**:
  - Muitos caminhos retornam 1 ou 127 sem distinguir.
  - Não verifica existência/permissão de sudo antes de montar/exec (access só em candidatos, mas race possível).
  - Se exec falha, perror mas o código de retorno 127 pode confundir com "command not found".

- **TOCTOU e race conditions potenciais (NOVO)**:
  - Entre `load_pudo_config` (leitura de arquivo por usuário) e `run_via_real_sudo`: o arquivo de config pode mudar.
  - Entre `access(sudo_path, X_OK)` e execv: o sudo binário pode ser trocado (improvável para /usr/bin, mas para fallback "sudo" via PATH sim).
  - No futuro helper: realpath + stat + exec sem fexecve ou open+fd cria janela clássica para substituição de binário alvo.

### 3. Baixos / Estilo de segurança

- Parsing no builtin ainda "muito básico" (conforme comentário). Depende inteiramente do parser do mid (que agora tem o fix de NEW-01).
- Não há suporte a opções do próprio `pudo` (ex: `-n`, `--`, `-E`).
- `pudo_sanitize_environment` é pública para testes — documentado como "não use em produção", mas exposto. Idealmente, mover a versão "real" (que não muta) para interna e expor só wrapper de teste que restaura estado.

## Revisão Adicional de Código + Novas Ameaças (NEW-05-01)

Além dos achados da auditoria inicial, esta revisão aprofundada identificou:

### Novos Achados de Alto Impacto

1. **Mutação persistente de ambiente no processo shell (CWE-15, CWE-732)**  
   `pudo_sanitize_environment` + `unsetenv` em loop modifica `environ` do petrush.  
   Impacto: todos os comandos executados *depois* de um `pudo` herdam env "limpo". Pode quebrar workflows do usuário (python, go, etc.).  
   Mais grave: em um shell interativo de longa duração, o estado de segurança do shell muda de forma não óbvia.  
   **Fix recomendado (Fase 0)**: parar de usar unsetenv global. Construir um `char **clean_envp` local e usar `execve(sudo_path, sudo_argv, clean_envp)` (ou para o pudod futuro). Manter o env do pai intacto.

2. **Policy decision com input não-canonicalizado antes de delegar (CWE-22, CWE-59)**  
   `target_cmd = cmd->argv[arg_start]` usado diretamente em `is_command_allowed` e log.  
   Sem realpath, sem remoção de ./ ../ , sem verificação de que é arquivo regular.  
   Em Fase 0 ainda "seguro" porque sudo real decide, mas quando mover decisão para dentro ou para helper, vira IDOR/path traversal lógico.

3. **Config carregada de diretório controlado pelo usuário sem verificação de ownership**  
   `~/.config/petrush/pudo.conf` — em Fase 0 ok (usuário é o dono).  
   **Nunca** usar path baseado em HOME do caller no lado do pudod setuid. O helper deve usar path fixo root-owned (ex: `/etc/petrush/pudo.allow`).

4. **Buffers fixos + truncamento silencioso**  
   - sudo_argv[128]: se o usuário passa 200 args, alguns são dropados sem erro.  
   - Log buf[256]: mensagens longas truncam (pode esconder parte do comando em auditoria).  
   - Config line[512]: linha muito longa é truncada, pode criar entrada "parcial" que permite algo inesperado.

5. **Uso de access(2) seguido de exec → TOCTOU clássico**  
   No loop de candidatos sudo: access então exec. Janela para substituir o binário (especialmente fallback "sudo" no PATH).

6. **Ausência de verificação de tipo do alvo**  
   Nada impede `pudo /dev/zero` ou `pudo /proc/self/exe` ou fifo. Sudo pode falhar de forma feia ou (em casos raros) se comportar diferente.

### Threat Model Complementar (foco em Fase 1)

Quando o pudod existir:

- **Spoofing do helper**: atacante substitui o binário pudod no PATH ou via symlink antes do exec do petrush.  
  Mitigação: petrush deve usar **caminho absoluto hardcoded** para o pudod (ex: `/usr/local/libexec/petrush-pudod`), nunca "pudod" ou PATH search. Verificar com stat/fstat que é o mesmo inode esperado ou assinado (futuro: cosign).

- **Tampering da allow-list no helper**: se a allow-list do pudod for lida de arquivo editável por não-root, atacante adiciona /bin/sh.  
  Mitigação **obrigatória**: arquivo root:root, 0644 ou mais restrito, parser no helper valida owner/mode antes de confiar no conteúdo.

- **Repudiation**: sem log imutável no pudod (syslog pode ser manipulado por root, mas root já é o objetivo). Usar também append-only file ou journal com tag específica.

- **Elevation via env poisoning do pudod**: mesmo que petrush sanitize, o pudod deve **ignorar completamente** o env herdado para decisões de policy e para o env do filho. Qualquer variável que o pudod use para algo sensível (ex: se usasse HOME para algo) deve ser resetada no início do pudod.

- **DoS no helper**: argv gigante, path gigante → buffer overflow ou lentidão. Usar limites rígidos (PATH_MAX, MAX_ARGS=128 ou 256).

- **Race no realpath + exec**: clássico. Usar fexecve(fd) após open+ fstat para reduzir janela.

## Status vs Design Doc

- Fase 0 esqueleto existe e melhorou, mas vários itens do design ainda pendentes:
  - Parser mais completo de opções do pudo: não.
  - Sistema de allow-list **robusto e canonicalizado**: não (parcial).
  - Logging forte (argv completo, correlação): não.
  - Testes de segurança reais (incluindo não-mutação de env): parciais.
  - **Mudança de env não-mutante**: novo débito descoberto em NEW-05-01.

**Alinhamento com princípios**: ainda viola "menor privilégio no tempo" por causa da mutação global e da ausência de re-validação no lado root (para quando Fase 1 vier).

## Design Concreto do Helper `pudod` (Fase 1) — Proposta para NEW-05-03

**Tamanho alvo**: < 400 linhas totais de C (incluindo headers, comentários, whitespace). Meta realista: 250-320 LOC para ser auditável em <1h por humano experiente.

**Princípio inegociável**: TODO o código com privilégios elevados (root ou caps) vive **exclusivamente** no pudod. petrush (e todo o resto) **nunca** contém setuid, seteuid, setreuid, cap_set, ou exec direto de binários privilegiados sem passar pelo helper.

### Protocolo de Comunicação (escolha: argv)

Recomendação: **argv simples** (confirmada no design original como Opção 2 recomendada).

Formato da invocação (feito pelo petrush não-privilegiado):

```
/usr/local/libexec/petrush-pudod /caminho/absoluto/comando [arg1] [arg2] ...
```

- petrush **NUNCA** passa env perigoso para o pudod. Usa envp mínimo controlado (ex: apenas PATH seguro + TERM se necessário).
- pudod recebe argv[0] = "petrush-pudod" (ou full path), argv[1] = comando, argv[2..] = args do usuário.
- Vantagens:
  - Kernel entrega argv de forma segura (sem parsing de string no lado root).
  - Zero complexidade de protocolo (sem sockets, sem serialização, sem length-prefix).
  - Fácil de auditar: main() olha argc/argv, nada mais.
- Desvantagens: limite de tamanho de argumento do kernel (geralmente 128k-2M). Suficiente para shell pessoal.

**Alternativa rejeitada aqui**: Unix socket + SCM_CREDENTIALS. Mais poderoso (permite askpass interativo, cache de cred, sessões), mas >>400 LOC + mais superfície no helper. Adiar para Fase 2 se necessário.

### Validações OBRIGATÓRIAS no pudod (root side — nunca confiar no petrush)

O pudod deve executar **todas** estas checagens, nesta ordem aproximada, e falhar fechado (fail secure) em qualquer violação:

1. **Verificar que está rodando com euid root** (geteuid() == 0). Se não, abortar (binário não instalado corretamente).
2. **Sanitizar imediatamente seu próprio ambiente**: unset de todas LD_*, *_PATH, *_LIB etc. Não usar nenhuma variável de ambiente para decisões de segurança.
3. **Validar argc >= 2**. argv[1] é obrigatório (o comando).
4. **Exigir que argv[1] seja caminho absoluto** (começa com '/'). Rejeitar qualquer coisa sem '/'. (Evita cwd attacks e "PATH confusion".)
5. **Resolver com realpath(3)**: `realpath(argv[1], resolved)`. Se falhar (ENOENT, ELOOP, etc.), negar.
6. **Verificar allow-list no lado privilegiado**:
   - Abrir arquivo de config **fixo e root-owned**: `/etc/petrush/pudo.allow` (ou `/usr/local/etc/petrush/pudo.allow`).
   - fstat ou stat + verificação: st_uid == 0 && (st_mode & 022) == 0 (não world-writable).
   - Ler linhas, ignorar # e vazias, comparar `resolved` com cada entrada canonical (também realpath das entradas da lista ou armazenar já canônicas).
   - Se nenhuma match exata, deny + log.
7. **Verificar o alvo no filesystem (após realpath)**:
   - stat(resolved, &st) ou melhor: int fd = open(resolved, O_RDONLY | O_CLOEXEC | O_NOFOLLOW?); fstat(fd, &st).
   - S_ISREG(st.st_mode) — deve ser arquivo regular.
   - st.st_uid == 0 ou pelo menos não writable por outros (depende de política).
   - Permissões de execução: pelo menos um bit X para root.
8. **Mitigar TOCTOU**:
   - Preferir `fexecve(fd, child_argv, child_envp)` após as checagens no fd (Linux).
   - Se não disponível: close(fd) então execve(resolved, ...). Aceitar janela pequena + documentar. Diretório /usr/bin etc. são geralmente seguros.
9. **Construir argv para o filho**:
   - child_argv[0] = resolved (ou basename(resolved) se quiser); 
   - child_argv[1..] = argv[2..] do pudod.
   - Terminar com NULL. Limitar número de args (ex: 256).
10. **Construir envp mínimo explícito para o filho** (nunca herdar do pudod/petrush):
    ```c
    char *child_envp[] = {
        "PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin",
        "TERM=xterm-256color",   /* ou omitir */
        "HOME=/root",
        "USER=root",
        "LOGNAME=root",
        "SHELL=/bin/sh",
        NULL
    };
    ```
    Remover qualquer coisa que o petrush poderia ter injetado.
11. **Logar com UID real**:
    - `syslog(LOG_AUTH | LOG_INFO, "pudod: real_uid=%d pid=%d ppid=%d cmd=%s args=%d", (int)getuid(), getpid(), getppid(), resolved, ...);`
    - getuid() == o usuário original (porque setuid binary preserva real uid).
    - geteuid() == 0.
12. **Executar**:
    - `fexecve(fd, child_argv, child_envp);` ou `execve(...)`
    - Se falhar: log + _exit(127) (nunca return para main).
13. **Nunca** executar o comando se qualquer checagem anterior falhou. Fail-closed.

Adicionalmente:
- Fechar fds extras (0,1,2 devem ficar; outros close-on-exec ou explicit close).
- Não usar malloc para coisas críticas se possível, ou com checagens. Para simplicidade, buffers fixos + limites.
- Sem getopt complexo, sem regex, sem shell parsing no root.

### setuid root vs Linux Capabilities

**Opção A — setuid root (recomendada para Fase 1 inicial)**:
- `chown root:root pudod && chmod 4755 pudod`
- Simples, funciona em qualquer Linux.
- Risco: se bug no pudod, atacante com execução via petrush ganha root full.
- Mitigado por: tamanho mínimo + allow-list rígida + revalidação + logging.
- sudo clássico usa exatamente isso.

**Opção B — file capabilities (mais moderna, preferível se possível)**:
- `setcap cap_dac_override,cap_setuid,cap_setgid+ep pudod` ou caps mais finos.
- O binário não precisa de setuid bit; ganha apenas as caps necessárias no momento de execução.
- Problemas para caso de uso "executar comando arbitrário como root":
  - Para o filho herdar "rootness", ainda precisa de cap_setuid ou o exec de um binário setuid.
  - Muitos comandos esperam euid==0.
  - Ambient capabilities + seccomp podem complicar.
- Recomendação: avaliar depois de ter pudod funcional com setuid. Para a maioria dos casos de "quero rodar apt sem digitar sudo", setuid é aceitável se o binário for mínimo e auditado.

**Híbrido**: usar sudo real mesmo na Fase 1 para o "ask password" e policy, e pudod só para casos offline. Mas isso complica.

**Conclusão conservadora**: Começar com setuid root + documentar explicitamente o risco. Oferecer `setcap` como alternativa experimental documentada. Nunca habilitar sem o usuário (petrus) aprovar após ler os riscos.

### Esqueleto de Código

Ver arquivo complementar: `docs/security/pudod-minimal-skeleton.c`

Este esqueleto é intencionalmente mínimo, sem features avançadas, pronto para revisão manual linha a linha. Não compila sozinho (falta CMake target), serve como base para implementação.

### Instalação e Hardening (recomendações)

- Instalar em `/usr/local/libexec/petrush-pudod` (ou `/usr/libexec/petrush-pudod`).
- Nunca no PATH do usuário comum.
- petrush deve ter o path **hardcoded** em uma constante (não configurável pelo usuário).
- Após chown/chmod 4755, o binário deve ser listado em qualquer SBOM / inventário.
- Considerar remoção de setuid bit em upgrades (require re-instalação manual).

## Recomendações (priorizadas) — atualizado NEW-05

1. **Antes de qualquer helper setuid (NEW-05-02)**:
   - Corrigir mutação de ambiente: construir envp limpo + execve explícito. Parar de unsetenv global.
   - Tornar load de config mais robusto (verificar tamanho, uma passagem, melhor tratamento de erro).
   - Canonicalizar / normalizar o target_cmd antes de allow-list (ou remover a checagem do frontend e deixar 100% pro helper).
   - Adicionar verificação de tipo (regular executable) antes de chamar sudo.
   - Aumentar buffers ou usar alocação dinâmica com limites.
   - Expandir sanitização (mais vars).
   - Melhorar testes: restaurar env após sanitize em testes, testar load com arquivos temporários, testar paths variantes.

2. **Para Fase 1 (helper) — NEW-05-03**:
   - Seguir estritamente o design acima.
   - Helper <400 LOC, sem parsing complexo.
   - Protocolo argv.
   - Re-validação completa + realpath + allow-list root-only + env limpo explícito + logging com getuid().
   - Escolher setuid root (com nota de risco) ou capabilities após avaliação.

3. **Geral (NEW-05-05)**:
   - Adicionar target CMake separado para pudod (com flags mais duras, possivelmente -D_FORTIFY_SOURCE=3, sem asan/ubsan no binário final setuid).
   - Documentar instalação + riscos no README e em docs/security/.
   - Considerar manter sudo real como opção mesmo após pudod (para askpass e sudoers maduro).
   - Mover/versão design doc.
   - Adicionar smoke test que exercita `pudo` (NEW-14) só após hardening Fase 0.

## Próximos Passos Imediatos para NEW-05

Sub-itens sugeridos para TODO.md (NEW-05-0x):

- NEW-05-01: Auditoria aprofundada + atualização deste documento (concluída nesta iteração).
- NEW-05-02: Hardening Fase 0 — env não-mutante, allow-list + canonicalização, buffers, testes, verificações de tipo. Gate antes de pudod.
- NEW-05-03: Implementar pudod mínimo (a partir do esqueleto) + target build separado.
- NEW-05-04: Testes de segurança específicos para pudod (incl. matriz de falhas, simulação de uid real, checagem de logs).
- NEW-05-05: Documentação de instalação, riscos, política de allow-list + instruções para o usuário (petrus) revisar e aprovar antes de qualquer setuid.

**Decisões que exigem aprovação explícita do líder supremo (petrus) antes de qualquer código setuid**:
- Avançar de Fase 0 para Fase 1 (helper próprio)?

---

## Status Final da Implementação (após iterações NEW-05)

**Data da revisão**: 2026-07-01 (incluindo subagente security-engineer + ações recomendadas).

**Implementação atual**:
- pudod implementado em `src/pudod/pudod.c` (< 300 LOC efetivos).
- Allow-list carregada de `/etc/petrush/pudo.allow` com verificação de dono (root) e permissões (sem write para outros).
- Caminhos da allow-list canonicalizados com realpath no load (melhor comparação após resolve do alvo).
- petrush (unpriv) usa `build_clean_envp()` + resolve para passar argv limpo + abs para pudod.
- Fallback para sudo real se pudod não disponível (com warning).
- Sanitização agressiva de env no pudod + no frontend (sem mutação persistente do shell via clean envp).
- Build separado no CMake com hardening (sem ASan/UBSan).
- Target `pudod-valgrind` para verificação de memória.
- Teste `pudod_refuses_without_privs` em `test_pudo` (roda sem root, valida recusa + allow-list fail).
- Documentação: `docs/security/pudod-install.md`, `src/pudod/README.md`, atualizações em design e audit.

**Achados da auditoria inicial mitigados**:
- Mutação de env: mitigado com envp explícito.
- Não-canonicalizado: pudod usa realpath + canonical na lista.
- TOCTOU: realpath + (fexecve quando possível), checks em fd.
- Falta de verificação de tipo: fstat + S_ISREG.
- Buffers: limites rígidos (MAX_ARGS, PATH_MAX).
- Logging: syslog + uid real + recusa clara.

**Riscos residuais aceitos (documentados)**:
- Janela TOCTOU pequena se não usar fexecve (fallback execve após close).
- Dependência de /etc ser seguro (root controlado).
- Usuário deve revisar e aprovar manualmente o setuid/chmod/setcap.
- Allow-list é exata por path canonical (não glob, não sudoers complexo).

**Gate para setuid**:
- Valgrind limpo nos paths de negação.
- Lint (clang-tidy + cppcheck) limpo.
- Testes unitários passando.
- **Recomendação**: Antes de aplicar 4755 ou setcap, petrus deve:
  1. Ler esta audit completa.
  2. Revisar manualmente o pudod.c linha a linha.
  3. Testar em ambiente isolado.
  4. Confirmar allow-list mínima.
  5. Manter backup do binário anterior.

Este documento atualizado serve como base para a revisão final por Narciso/CISO ou equivalente.

**Próximo recomendado (se aprovado)**: Adicionar smoke tests integrados que exercitem `pudo <comando-da-allow-list>` (NEW-14), e possivelmente target de install que apenas imprime os comandos de setuid (nunca executa).
- Modelo de allow-list para pudod: lista estática de paths absolutos hardcoded, ou arquivo /etc root-only editável por admin?
- setuid root (4755) ou tentar capabilities primeiro?
- Local de instalação do pudod e do arquivo de allow-list.
- Se manter dependência de sudo real mesmo com pudod (recomendado para askpass).

**Regra conservadora**: NENHUM código com set*uid, cap ou exec de binário root será adicionado ao petrush ou ao pudod sem:
1. Esta auditoria atualizada.
2. Esqueleto revisado.
3. Aprovação explícita via AskUserQuestion ou decisão registrada.
4. Testes que cubram os vetores principais.

---
*Este documento deve ser atualizado a cada iteração de NEW-05. Última atualização: 2026-07-01 (revisão aprofundada + proposta pudod).*