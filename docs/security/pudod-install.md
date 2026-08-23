# Instalação Segura do pudod (NEW-05)

**AVISO CRÍTICO**: O `pudod` é um binário que **pode** rodar com privilégios de root via setuid ou capabilities. Qualquer erro aqui pode comprometer o sistema inteiro.

Siga esta documentação **apenas após**:

- Revisar completamente `src/pudod/pudod.c`
- Ler `docs/security/pudo-audit.md`
- Entender os riscos (TOCTOU, allow-list bypass, env poisoning, etc.)
- Ter aprovação explícita para uso em produção.

## 1. Build

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --target pudod
```

Recomendado: build sem sanitize (ASan/UBSan incompatíveis com setuid).

## 2. Instalação dos binários (sem privilégios ainda)

```bash
cmake --install build --prefix /usr/local
```

Isso instala:
- `petrush` em `/usr/local/bin/`
- `petrush-pudod` em `/usr/local/libexec/`
- Exemplo: `/usr/local/etc/petrush/pudo.allow.example` (ou similar dependendo do prefix)

## 3. Configuração da Allow-list (obrigatório)

Copie e proteja o arquivo de allow-list:

```bash
sudo mkdir -p /etc/petrush
sudo cp /usr/local/etc/petrush/pudo.allow.example /etc/petrush/pudo.allow
sudo chown root:root /etc/petrush/pudo.allow
sudo chmod 644 /etc/petrush/pudo.allow   # ou 600 se quiser mais restrito
```

Edite `/etc/petrush/pudo.allow` com **caminhos absolutos** de comandos permitidos (um por linha, sem shell).

O pudod verifica:
- O arquivo existe
- Dono = root
- Sem permissões de escrita para group/other

Se falhar, **nega tudo** (fail secure).

Shells genéricos são recusados na carga (SEC-12 / R-C3): depois do `realpath`, se o basename canônico for `sh`, `bash`, `dash`, `ash` ou `busybox`, a linha é ignorada com WARNING no log. Allow-listar um shell reabre o modelo POSIX completo já como root. Se todas as linhas forem puladas (ou o arquivo ficar só com shells), a lista fica vazia e o comportamento é deny-all. O example oficial fica em `id` / `whoami` / `true`.

## 4. Aplicação de Privilégios (manual e perigoso)

**NUNCA** faça isso em CI ou sem revisão.

Opção A - setuid root (clássico):

```bash
sudo chown root:root /usr/local/libexec/petrush-pudod
sudo chmod 4755 /usr/local/libexec/petrush-pudod
```

Opção B - Capabilities (preferível, menor privilégio):

```bash
sudo setcap cap_setuid,cap_setgid+ep /usr/local/libexec/petrush-pudod
```

Verifique:

```bash
ls -l /usr/local/libexec/petrush-pudod
getcap /usr/local/libexec/petrush-pudod
```

## 5. Hardening Adicional Recomendado

- Use um allow-list **mínima** (princípio do menor privilégio).
- Considere SELinux/AppArmor policy para o pudod.
- Monitore logs via syslog (LOG_AUTH).
- Faça auditoria periódica do arquivo de allow-list e do binário.
- Evite adicionar comandos como `bash`, `python`, `vi` que permitem escape.

## 6. Testes

- `cmake --build build --target test_pudo` (frontend + recusa sem root)
- `cmake --build build --target pudod-valgrind` (valgrind nos paths de negação/allow-list)
- Teste manual e com setuid após revisão.
- Sempre rode valgrind e static analysis antes de qualquer instalação privilegiada.

## 7. Desinstalação / Remoção de Privilégios

```bash
sudo chmod 755 /usr/local/libexec/petrush-pudod
sudo chown root:staff /usr/local/libexec/petrush-pudod   # ou seu grupo
sudo rm -f /etc/petrush/pudo.allow
```

## Ver também

- `src/pudod/README.md`
- `docs/design/pudo.md`
- `docs/security/pudo-audit.md`
- Código fonte de `pudod.c` (mantenha < 400 LOC para auditabilidade)

**Regra do projeto**: Qualquer mudança no pudod exige re-auditoria e atualização deste documento.