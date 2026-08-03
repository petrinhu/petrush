# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - 2026-07-01 - S0 Gate

### Added
- Complete Onda 1 (Core MVP + Gate S0)
- `pudo` secure helper (`pudod`) with root-owned allow-list, environment sanitization, and minimal privileged code
- Automated gate via `cmake --build build --target verify` (build + lint + smoke + valgrind)
- Smoke tests for integration (≥13 commands including `pudo`)
- Unit tests expanded (env, info/diagnostics)
- `info` builtin (Onda 3 placeholder for diagnostics)
- Robust build targets: `clean-build`, `smoke`, `pudod-valgrind`
- Beginner documentation (`docs/beginner-guide.md`) explaining all jargon for newcomers
- CI workflow (`.github/workflows/ci.yml`) with matrix, lint, smoke, valgrind
- `docs/roadmap.md` and `docs/architecture.md` for layers and future
- `docs/security/` with audit and install guides for `pudo`

### Changed
- Pragmatic 4-layer architecture documented (no physical split for solo/anti-OE)
- README updated with current status, automated gate, examples
- TODO.md updated with all waves planning and statuses
- Anti-over-engineering reinforced: no pipes, redirection, scripting in v0.1
- Build system: isolated `pudod` from sanitize builds, FORTIFY, hardening

### Fixed
- Parser realloc OOB risk (NEW-01)
- Clang-tidy noise tuned with documented suppressions (NEW-02)
- Environment mutation avoided in `pudo` path (clean envp passed to pudod)
- Various small issues in tests, docs, CMake

### Notes
- Onda 1 gate achieved: Sanitize builds, tests, lint, smoke, valgrind all passing/automated
- Onda 2 (CI, docs, polish): Complete
- Onda 3 (future): Planning complete; implementation only on clear demand + ROI. No advanced features added (pipes etc. deferred)
- Version bumped to 0.1.0 for S0
- `pudo` setuid still requires manual review/approval (per security rules)
- License: GNU Affero General Public License v3.0 (AGPL-3.0)
- Host: GitHub (`petrinhu/petrush`); Codeberg/Forgejo deprecado

[0.1.0]: https://github.com/petrinhu/petrush/releases/tag/v0.1.0
