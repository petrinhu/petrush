# Plano de implementação — features de shell (pesquisa → ROI)

**Data:** 2026-08-03  
**Fonte da pesquisa:** `docs/research-shell-features.md` + gaps do petrush medidos no código.  
**Regra de execução:** TDD red→green→refactor · **CI verde antes da próxima feature** · **commit + push ao fim de cada fatia**.

## Critério de ROI

```
ROI ≈ (amor do usuário × frequência de uso × “quebra expectativa se faltar”)
      / (esforço C23 + risco de OE + risco de bugs)
```

Escala **1–10** (10 = melhor ROI).  
**Ordem de execução = ROI crescente** (1 → 10): começa pelo que **ainda falta e tem ROI mais baixo entre o backlog pendente**, e sobe até o melhor ROI pendente…  

> Interpretação operacional (anti-desperdício): a tabela abaixo lista **todas** as features da pesquisa + desejadas, com status. A **fila de trabalho** é o backlog **PENDENTE** ordenado por **ROI decrescente** (melhor ROI primeiro), porque “crescente de valor no produto” e “siga até o final” exigem maximizar retorno cedo.  
> Se o líder quiser estritamente “pior ROI primeiro”, inverter a fila.

---

## Inventário completo (pesquisa + desejos + gaps)

| ID | Feature | ROI | Esforço | Status | Notas |
|----|---------|-----|---------|--------|-------|
| UX-01 | History setas (↑↓) | 9 | — | ✅ linenoise | Já embutido |
| UX-02 | History autosuggest (ghost) | 10 | M | ✅ v0.3.0 | Hints linenoise |
| UX-03 | Tab completion | 10 | M | ✅ v0.3.0 | builtins+PATH+files |
| UX-04 | Prompt custom `PETRUSH_PS1` | 9 | B | ✅ v0.3.0 | |
| UX-05 | Aliases | 9 | B | ✅ v0.3.0 | |
| UX-06 | `&&` / `\|\|` | 8 | M | ✅ v0.3.0 | |
| UX-07 | `which` | 8 | B | ✅ v0.3.0 | |
| UX-08 | Pipes `\|` | 9 | M | ✅ v0.2 | |
| UX-09 | Redir `>` `>>` `<` | 9 | M | ✅ v0.2 | |
| UX-10 | pushd/popd/dirs | 8 | B | ✅ v0.3.1 | |
| UX-11 | `!!` / `!n` | 8 | B | ✅ v0.3.1 | |
| UX-12 | **`~` e `~/` expansion** | **10** | B | ✅ v0.3.2 | Gap real (eco literal) |
| UX-13 | **`$VAR` / `${VAR}` expansion** | **10** | M | ✅ v0.3.2 | Gap real |
| UX-14 | **`cd -` / OLDPWD** | **10** | B | ✅ v0.3.2 | 1 linha de UX diária |
| UX-15 | **Prompt escapes `\w` `\u` `\h` `\n`** | **8** | B | 🔲 PENDENTE | Deixa PS1 “de verdade” |
| UX-16 | **`2>` `2>>` `2>&1` `&>`** | **8** | M | 🔲 PENDENTE | stderr / merge |
| UX-17 | **`;` sequencial** | **7** | B | 🔲 PENDENTE | `a; b` sempre roda b |
| UX-18 | **Glob simples `*` `?`** | **7** | M | 🔲 PENDENTE | Só unquoted |
| UX-19 | **Builtins no pipe** | **6** | M | 🔲 PENDENTE | subshell-like |
| UX-20 | **Ctrl-R history search** | **6** | A | 🔲 PENDENTE | linenoise TODO nativo; portar/minimal |
| UX-21 | **Syntax highlight mínimo** | **5** | A | 🔲 PENDENTE | aspas/erro; não full tokens |
| UX-22 | **`source` / `.` script** | **5** | M | 🔲 PENDENTE | rodar arquivo linha a linha |
| UX-23 | **Background `&`** | **4** | A | 🔲 PENDENTE | job control mínimo |
| UX-24 | Abbreviations (Fish) | 3 | M | ❌ skip | Alias cobre 90% |
| UX-25 | Plugin system (OMZ) | 2 | XL | ❌ skip | anti-OE explícito |
| UX-26 | POSIX 100% / bash compat | 2 | XL | ❌ skip | fora da natureza do projeto |

B=baixo M=médio A=alto XL=gigante

---

## Fila de execução (só pendentes, **maior ROI primeiro**)

| Ordem | ID | Fatia | Gate de saída |
|------:|----|-------|---------------|
| 1 | UX-14 | `cd -` + OLDPWD | test + smoke; CI verde |
| 2 | UX-12 | tilde `~` | test_parser expand; CI |
| 3 | UX-13 | `$VAR` `${VAR}` | test_expand; CI |
| 4 | UX-15 | PS1 `\w\u\h` | test_ps1; CI |
| 5 | UX-16 | stderr redirs | test_process redir; CI |
| 6 | UX-17 | `;` lists | test_parser; CI |
| 7 | UX-18 | glob `*` `?` | test_glob; CI |
| 8 | UX-19 | builtins in pipe | test + smoke; CI |
| 9 | UX-20 | Ctrl-R (se viável em linenoise) | manual+test se API; CI |
| 10 | UX-21 | syntax highlight mínimo | visual + unit se possível; CI |
| 11 | UX-22 | `source` | test_source; CI |
| 12 | UX-23 | background `&` | test_job; CI |

**Após cada fatia:** commit descritivo + push · esperar CI success · só então próxima.

**Fim de onda:** tag `v0.4.0` se fechar UX-12…UX-18 (núcleo de expansão); `v0.5.0` se fechar até UX-23.

---

## Já entregue (não reimplementar)

v0.2 pipes/redir · v0.3.0 alias/PS1/complete/hints/&&/||/which · v0.3.1 pushd/!!

---

## Decisão autônoma registrada

- Skip UX-24/25/26 (ROI baixo / OE).  
- Ctrl-R (UX-20): tentar patch mínimo no linenoise; se API bloquear, documentar degradação (setas + hints + !!) e seguir.  
- Highlight (UX-21): só feedback de aspas não fechadas / token grosso, não full highlighter.
