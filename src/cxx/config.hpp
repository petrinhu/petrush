/*
 * configsh config model (CXX-TUI): XDG INI + dump/check.
 * C++23, -fno-exceptions -fno-rtti. Sem STL que lance.
 */
#pragma once

#include <cstddef>
#include <cstdint>

namespace configsh {

inline constexpr std::size_t kMaxPath = 512;
inline constexpr std::size_t kMaxSectionName = 32;
inline constexpr std::size_t kMaxKey = 64;
inline constexpr std::size_t kMaxVal = 256;
inline constexpr std::size_t kMaxEntries = 64;
inline constexpr std::size_t kMaxSections = 8;
inline constexpr std::size_t kMaxLine = 512;

struct Entry {
    char key[kMaxKey]{};
    char val[kMaxVal]{};
};

struct Section {
    char name[kMaxSectionName]{};
    Entry entries[kMaxEntries]{};
    std::size_t n = 0;
};

struct Config {
    Section sections[kMaxSections]{};
    std::size_t n = 0;
    char path[kMaxPath]{};
    bool loaded_from_disk = false;
};

/* Resolve XDG path into out (NUL-terminated). Returns 0 or -1. */
int resolve_config_path(char *out, std::size_t out_cap);

/* Load INI from path; missing file => defaults (return 0, loaded_from_disk=false).
 * Malformed => -1 and message on stderr. */
int load_config(Config *cfg, const char *path);

/* Fill built-in defaults (prompt + history). */
void load_defaults(Config *cfg);

/* Dump whole config or one section (section==nullptr => all) to stdout. */
int dump_config(const Config *cfg, const char *section);

/* Validate cfg; section!=nullptr limits to that section existence.
 * Returns 0 ok, 1 error (stderr detail). */
int check_config(const Config *cfg, const char *section);

/* Find section by name; nullptr if absent. */
const Section *find_section(const Config *cfg, const char *name);
Section *find_section_mut(Config *cfg, const char *name);

/* Known section names for --check strictness. */
bool is_known_section(const char *name);

} // namespace configsh
