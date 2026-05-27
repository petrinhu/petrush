# Testes — petrush

Usamos **acutest** (single-header) como framework de testes.

- `acutest.h` — cópia limpa de https://github.com/mity/acutest
- Licença: MIT (ver cabeçalho do arquivo)
- Data do embed: 2026-05-27

**Vantagem para este projeto (S0 + Opção A):**
- Zero dependências externas
- Integração trivial no CMake
- Excelente para TDD em código C puro (parser, dispatcher, builtins)
- Fácil de descartar ou migrar no futuro

Não edite `acutest.h` diretamente. Se precisar de atualização, baixe nova versão e atualize este README.
