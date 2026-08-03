# src/front — Camada de Apresentação (UI / REPL)

Reservada para código de interface com o usuário:

- Loop REPL principal
- Integração com linenoise (history, prompt)
- Tratamento de sinais no nível de UI
- Carregamento de ~/.petrushrc
- Impressão de banners, help interativo, etc.

**Estado atual (2026)**: main.c está na raiz de src/ por simplicidade (porte Solo).  
Mapeamento lógico para "front": o conteúdo de main.c + interações de I/O.

Quando a separação física for feita (após decisão NEW-03), mover partes relevantes para cá.

Ver docs/ e CLAUDE.md para princípios de 4 camadas.
