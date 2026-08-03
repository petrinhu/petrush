# src/back — Camada de Dados / Persistência / Serviços

Reservada para:

- Gerenciamento persistente de histórico (além do linenoise)
- Leitura/escrita de arquivos de config (além do rc simples)
- Possível logging / auditoria de longo prazo
- Backing stores futuros (ex: config de `pudo`)

**Estado atual (2026)**: Não há código separado ainda. 
Lógica de env/history/rc está espalhada em foundation + main.

Ver camada Foundation para primitivas e Mid para orquestração.

Placeholder para respeitar a arquitetura de 4 camadas sem inflar o projeto pequeno.
