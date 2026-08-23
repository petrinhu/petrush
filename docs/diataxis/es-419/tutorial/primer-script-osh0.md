# Tutorial: tu primer script con petrush (OSH-0)

**Tipo Diátaxis:** tutorial  
**Audiencia:** novato que ya compiló el binario `petrush`  
**Tiempo estimado:** 10 min  
**Prerrequisitos:** binario `petrush` en el `PATH` o ruta absoluta conocida; Linux  
**Lo que tendrás al final:** un archivo ejecutable con shebang que imprime una línea y sale con status 0  
**Item:** DOC-DIA-ES · **Owner:** technical-writer · **Last-reviewed:** 2026-08-23

## 1. Confirmar el binario

```bash
which petrush || ls -l ./build/petrush
```

Si aún no compilaste, usa el build local (ejemplo):

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --target petrush
export PATH="$(pwd)/build:$PATH"
```

Output esperado: el ejecutable `petrush` existe y es ejecutable.

## 2. Crear el script

Crea `hola.sh` (el nombre es libre):

```bash
cat > /var/tmp/hola.sh <<'EOF'
#!/usr/bin/env petrush
echo osh0-hello
EOF
chmod 0755 /var/tmp/hola.sh
```

El shebang (`#!/usr/bin/env petrush`) pide al kernel que arranque el `petrush` del `PATH` y pase el archivo como argumento. Esto es el modo **OSH-0**: script, sin banner, sin `~/.petrushrc`.

## 3. Ejecutar por el shebang

```bash
/var/tmp/hola.sh
echo "exit=$?"
```

Output esperado:

```text
osh0-hello
exit=0
```

No debe aparecer el banner `C23 shell` ni el prompt `petrush>`.

## 4. Ejecutar pasando el archivo al binario

Equivalente sin depender del bit ejecutable:

```bash
petrush /var/tmp/hola.sh
echo "exit=$?"
```

Mismo output. Internamente el `main` llama `petrush_run_script` cuando hay `argv[1]`.

## 5. Ver el status del último comando

```bash
cat > /var/tmp/ultimo.sh <<'EOF'
#!/usr/bin/env petrush
/bin/false
EOF
chmod 0755 /var/tmp/ultimo.sh
/var/tmp/ultimo.sh
echo "exit=$?"
```

Output esperado: `exit=1` (status de `/bin/false`).

## 6. (Opcional) Mirar el hardware y la config

Los builtins también corren en script OSH-0:

```bash
cat > /var/tmp/hw.sh <<'EOF'
#!/usr/bin/env petrush
wai -cpu
EOF
chmod 0755 /var/tmp/hw.sh
/var/tmp/hw.sh | head
```

La config XDG queda en el binario separado `configsh` (no en el REPL):

```bash
configsh --dump | head
```

Si `config.ini` no existe, el `configsh` usa defaults en memoria (`ps1=petrush> `, `history.max=1000`).

## Verificar

- [ ] Script con shebang imprime y sale 0
- [ ] No hay banner REPL en modo script
- [ ] `/bin/false` en el script propaga exit 1
- [ ] (Opcional) `wai` y `configsh --dump` responden

## Resumen

Construiste: un script OSH-0 con shebang.  
Aprendiste:

- `petrush archivo` entra en modo script (sin rc, sin banner)
- el exit del proceso es el del último comando (o de `exit N`)
- OSH-0 **aún no** expone `$1` ni `if` (eso es onda posterior)
- `wai` y `configsh` son superficies aparte (builtin y binario)

## Próximos pasos

- How-to: [Inventariar con `wai`](../how-to/inventariar-con-wai.md)
- How-to: [Usar el `configsh`](../how-to/usar-configsh.md)
- Reference: [OSH-0 / shebang](../reference/osh0-shebang.md)
- Explanation: [OSH-0 no es POSIX completo](../explanation/osh0-no-es-posix-completo.md)
