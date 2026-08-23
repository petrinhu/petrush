# Como inventariar hardware com `wai`

**Tipo Diátaxis:** how-to  
**Audiência:** utilizador intermédio no REPL ou em script  
**Item:** DOC-DIA-PT · **Owner:** technical-writer · **Last-reviewed:** 2026-08-23

## Quando usar

Quando quiser listar peças da máquina (disco, vídeo, memória, CPU, …) **sem root**, lendo `/sys` e `/proc`. Não use para ler serial/uuid: o `wai` omite esses campos de propósito.

## Pré-requisitos

- Binário `petrush` com o builtin `wai` (fatia ASM-WAI)
- Linux com sysfs montado (caso normal)
- Sem privilégio especial

## Passos

1. Abra o REPL ou um script OSH-0.
2. Peça todas as secções (sem flags):

```bash
wai
```

3. Ou filtre por peça:

```bash
wai -disk
wai -mem -cpu
wai -battery -thermal
```

4. Para ajuda local:

```bash
wai --help
```

## Verificação

- A saída traz cabeçalhos `# …` por secção pedida.
- Linhas com `serial` / `uuid` no nome **não** aparecem.
- Exit 0 em sucesso; exit 2 se a flag for desconhecida.

## Variações

- Em testes, o código aceita overlay via `petrush_wai_set_root` (API C de teste). O utilizador normal não precisa disso: caminhos absolutos `/sys` e `/proc`.
- Sem flags = `PETRUSH_WAI_ALL` (todas as 12 peças).

## Relacionados

- Reference: [`wai`](../reference/wai.md)
- Explanation: [Por que sysfs](../explanation/por-que-sysfs-wai-netcom.md)
- How-to rede: [`netcom`](inspecionar-rede-com-netcom.md)
