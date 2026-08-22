# TST-T8 Verificação de Secrets

**Data:** 2026-08-22  
**SHA HEAD (pré-commit do relatório):** `c37dd971a5b6a67a4280404868cf0fbe2fff6f8c`  
**Agent:** qa-engineer  
**Item:** TST-T8 (W14)  
**Veredicto:** **LIMPO** (sem leak encontrado)

## Escopo

Detectar credencial, token ou chave privada commitada no histórico ou presente na working tree, conforme `TESTES.md` (projeto) e T8 do vault (`gitleaks` / `trufflehog`).

**In:** histórico git (`main`), working tree (tracked + untracked sob o source), padrões comuns de secret.  
**Out:** secrets fora do repo (ex.: `~/.config/secrets/`), conteúdo de binários build, CI remoto.

## Ferramentas

| Ferramenta | Status na máquina | Uso nesta fatia |
|---|---|---|
| `gitleaks` 8.30.0 (`/usr/bin/gitleaks`) | instalado | primária (histórico + protect + no-git) |
| `trufflehog` | **ausente** | não executado; gitleaks cobriu o escopo |

Sem config local (`.gitleaks.toml` / allowlist) no repo: regras default do gitleaks.

## Execução

```text
gitleaks detect --source . --no-banner --redact -v
# 61 commits, ~778088 bytes, 144ms → no leaks found → EXIT 0

gitleaks protect --source . --no-banner --redact -v
# staged/unstaged → no leaks found → EXIT 0

gitleaks detect --source . --no-git --no-banner --redact -v
# working tree ~3367854 bytes, 371ms → no leaks found → EXIT 0
```

## Grep complementar (sem falso positivo de teste)

Padrões altos (só match se parecer valor real, não menção documental genérica):

| Padrão | Resultado |
|---|---|
| `-----BEGIN … PRIVATE KEY-----` | 0 |
| `AKIA…` / `ghp_` / `gho_` / `github_pat_` / `xox[baprs]-` / `sk-…` / `AIza…` | 0 |
| `(password\|secret\|api_key\|token\|Bearer)\s*[=:]\s*'…'` em fontes textuais | 0 |
| Arquivos `.env` / `*credentials*` / `*.pem` / `id_rsa*` (exceto `.git`) | 0 |
| `git log --all -S'BEGIN PRIVATE KEY' -S'AKIA' -S'ghp_' -S'github_pat_'` | 0 commits |

Nada em `tests/` exigiu allowlist: zero hits nos padrões acima.

## Achados

Nenhum. Repo e histórico limpos sob gitleaks + grep de padrões comuns.

## Limitações (honesto)

1. `trufflehog` não está instalado; segunda ferramenta do par canônico não rodou.  
2. gitleaks usa regras default (sem allowlist/custom do projeto).  
3. Scan `--no-git` inclui artefatos de build sob a árvore se existirem; nesta passada saiu limpo.  
4. Verificação de **histórico remoto** além do clone local não foi feita (mesmo SHA que `origin/main` no momento do scan).

## Critério de saída TST-T8

- [x] gitleaks no histórico: EXIT 0, no leaks  
- [x] gitleaks na working tree: EXIT 0, no leaks  
- [x] grep de padrões comuns: zero hits reais  
- [x] relatório em `docs/memory/tst-t8-secrets.md`  
- [ ] trufflehog (ferramenta ausente; não bloqueia com gitleaks verde)

**Status sugerido no TODO:** `🔍 Pendente verificação` (impl/execução entregue; ✅ só após onda de auditoria/TST se o orquestrador assim marcar).

## Referências

- Item: `TODO.md` → TST-T8  
- `TESTES.md` (projeto) § TST-T8  
- Vault `TESTES.md` § T8  
