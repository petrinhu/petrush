# Design: pudo — Builtin similar ao sudo (com foco extremo em segurança)

**Status**: Em fase de design (2026-05-27)

## Contexto

O usuário deseja um builtin chamado `pudo` que se comporte de forma similar ao `sudo`, ou seja:

- Permitir execução de comandos com privilégios elevados a partir do shell petrush.
- Funcionar como builtin (não como binário externo separado do ponto de vista do usuário).

**Requisito crítico**: "cuidado muito grande com segurança para evitar ataques e subir privilégios pelo atacante".

## Riscos Fundamentais

Implementar algo como sudo **dentro** de um processo de shell é extremamente perigoso. Os principais vetores de ataque clássicos incluem:

- Environment poisoning (LD_PRELOAD, LD_LIBRARY_PATH, etc.)
- Command injection / shell metacharacters
- Abuso de comandos permitidos (ex: permitir `vi`, `less`, `python`, `bash -c`)
- TTY/session hijacking
- Race conditions entre checagem de política e execução
- Vazamento de privilégios através do próprio shell
- Exploração de bugs no parser do builtin

Por isso, **qualquer design que execute lógica complexa com privilégios elevados dentro do processo do petrush é considerado inaceitável**.

## Princípios de Design (Obrigatórios)

1. **Princípio do Menor Privilégio no Tempo**
   - O código com privilégios elevados deve ser o menor possível e rodar o menor tempo possível.

2. **Separação de Privilégios**
   - O processo do petrush (não privilegiado) nunca deve ganhar privilégios permanentes.
   - Toda elevação deve ocorrer em um processo auxiliar separado.

3. **Ataque Surface Mínimo no Componente Privilegiado**
   - O helper privilegiado deve fazer o mínimo possível: validar, executar e logar.
   - Zero parsing complexo no lado privilegiado.

4. **Defesa em Profundidade**
   - Mesmo que o atacante controle o petrush, ele não deve conseguir escalar facilmente.

5. **Auditoria Forte**
   - Toda invocação de `pudo` deve ser registrada de forma difícil de burlar.

## Opções de Arquitetura Avaliadas

### Opção 1: Tudo dentro do petrush (Rejeitada)

- Fazer `setuid` no próprio binário do petrush ou usar `seteuid()` dinamicamente.
- **Por que rejeitada**: Aumenta dramaticamente a superfície de ataque. Qualquer bug no parser, builtins, linenoise, etc. vira uma escalada de privilégio. Viola todos os princípios acima.

### Opção 2: Helper setuid separado + comunicação via argv (Recomendada para S0)

**Modelo**:
- `pudo` é um builtin normal no petrush (roda sem privilégios).
- Existe um binário pequeno e separado chamado `pudod` (ou `pudo-helper`) com `setuid root` (ou capabilities específicas).
- O builtin `pudo` faz toda a política, parsing, logging e sanitização.
- Depois executa o helper passando uma linha de comando extremamente controlada via `argv` + um environment sanitizado.

**Vantagens**:
- O código com root é mínimo (idealmente < 500 linhas).
- Fácil de auditar.
- O shell pode fazer validações pesadas antes de chamar o helper.
- Menor risco de bugs complexos no componente privilegiado.

**Desafios**:
- Precisa de um protocolo muito rígido entre os dois.
- O helper não pode confiar em quase nada que vem do petrush.

### Opção 3: Helper + Unix Socket com autenticação de credenciais (Mais robusta, mais complexa)

- O helper roda como daemon (ou é ativado via socket activation).
- Comunicação via Unix domain socket + `SCM_CREDENTIALS` (para saber exatamente qual UID está falando).
- Permite políticas mais avançadas e sessões.

**Vantagens**: Mais seguro e flexível a longo prazo.
**Desvantagens**: Mais código no helper, mais superfície, mais difícil de implementar corretamente em uma ferramenta pessoal.

### Opção 4: Wrapper fino em cima do sudo real

- `pudo` é basicamente um frontend + logger + policy engine.
- No final ele chama o `/usr/bin/sudo` real com argumentos extremamente restritos.
- O usuário ainda se beneficia de toda a maturidade e hardening do sudo do sistema.

**Vantagens**: Muito mais seguro para uma ferramenta pessoal.
**Desvantagens**: Menos "nativo" e depende do sudo do sistema existir e estar configurado.

## Recomendação de Abordagem (Faseada e Segura)

Dado o requisito explícito de **"cuidado muito grande com segurança"**, a abordagem mais responsável é **faseada**:

### Fase 0 – MVP Seguro (Recomendado para começar)
- `pudo` como builtin no petrush.
- O builtin faz parsing, política, logging e sanitização.
- No final, ele invoca o **`/usr/bin/sudo` real do sistema** de forma muito controlada (com allow-list, environment limpo, etc.).
- Benefício: O usuário ganha imediatamente a experiência de builtin + auditoria, sem introduzir novo código privilegiado perigoso.

### Fase 1 – Helper Próprio (quando Fase 0 estiver madura)
- Desenvolver um helper mínimo (`pudod` ou `pudo-helper`) setuid/setcap.
- Trocar a chamada do sudo real pela chamada ao helper próprio.
- O helper deve ser o menor e mais auditável possível.

Essa abordagem permite entregar valor rápido enquanto se investe o tempo necessário na parte mais crítica (o código que roda com privilégios elevados).

**Qualquer tentativa de implementar lógica de elevação de privilégios diretamente dentro do processo do petrush é considerada inaceitável por razões de segurança.**

---

## Status da Implementação (Fase 0)

- [x] Esqueleto do builtin `pudo` criado (`src/mid/pudo.c`)
- [x] Registro do builtin no dispatcher
- [x] Logging básico (syslog + stderr)
- [x] Separator `--` robusto + busca de sudo em múltiplos paths
- [~] Sistema de allow-list (implementado mas ingênuo — sem canonicalização, carrega antes de sanitize)
- [ ] Parser mais completo de argumentos (suporte a opções do sudo)
- [ ] Sanitização forte de ambiente **sem mutação global** (débito descoberto em auditoria)
- [ ] Suporte a pedir senha
- [ ] Testes de segurança reais (incl. não-mutação de env)
- [ ] Verificação de tipo de alvo (regular executable)

**Status atual (NEW-05)**: 
- Fase 1 completa: `pudod` é executável separado com allow-list de `/etc/petrush/pudo.allow` (verif. dono root + perms no carregamento).
- petrush resolve e invoca `pudod <abs> [...]`.
- CMake install com warnings fortes + example config.
- Fallback para sudo se pudod ausente.
- Build: `cmake --build build --target pudod`
- Instalação privilegiada: manual + revisão obrigatória.

A implementação atual delega a execução real para o `pudod`.

**Atenção**: ver `docs/security/pudo-audit.md` e `src/pudod/README.md`. Instalação com privilégios exige revisão + aprovação explícita.

## Perguntas em Aberto para o Usuário

1. Qual o modelo de confiança desejado?
   - Allow-list rígida de comandos permitidos? (recomendado)
   - Ou algo mais parecido com sudoers (mais flexível, mais perigoso)?

2. O `pudo` deve suportar pedir senha (como sudo)?
   - Se sim, como fazer isso de forma segura (PAM? `sudo` real por baixo? TTY handling)?

3. Qual o nível de auditoria necessário?
   - Só logar no syslog/journal?
   - Logar em arquivo próprio com hash/integridade?
   - Enviar para algum sistema externo?

4. Prefere começar com a Opção 2 (helper próprio) ou com a Opção 4 (wrapper fino sobre `/usr/bin/sudo`)?

5. O helper deve rodar com root completo ou tentar usar Linux Capabilities (mais moderno e menos perigoso)?

---

**Próximo passo**: Aguardar respostas acima para definir a arquitetura final antes de escrever qualquer código.