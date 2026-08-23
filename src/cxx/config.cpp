/*
 * configsh XDG INI load/dump/check (CXX-TUI).
 */

#include "config.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace configsh {
namespace {

bool is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

void trim_inplace(char *s)
{
    if (!s) {
        return;
    }
    char *start = s;
    while (*start && is_space(*start)) {
        ++start;
    }
    if (start != s) {
        std::memmove(s, start, std::strlen(start) + 1);
    }
    std::size_t n = std::strlen(s);
    while (n > 0 && is_space(s[n - 1])) {
        s[--n] = '\0';
    }
}

bool streq(const char *a, const char *b)
{
    return a && b && std::strcmp(a, b) == 0;
}

int copy_str(char *dst, std::size_t cap, const char *src)
{
    if (!dst || cap == 0 || !src) {
        return -1;
    }
    std::size_t n = std::strlen(src);
    if (n + 1 > cap) {
        return -1;
    }
    std::memcpy(dst, src, n + 1);
    return 0;
}

Section *add_section(Config *cfg, const char *name)
{
    if (!cfg || !name || !*name) {
        return nullptr;
    }
    if (cfg->n >= kMaxSections) {
        std::fputs("configsh: too many sections\n", stderr);
        return nullptr;
    }
    Section *s = &cfg->sections[cfg->n++];
    *s = Section{};
    if (copy_str(s->name, sizeof(s->name), name) != 0) {
        --cfg->n;
        std::fputs("configsh: section name too long\n", stderr);
        return nullptr;
    }
    return s;
}

int add_entry(Section *sec, const char *key, const char *val)
{
    if (!sec || !key || !*key || !val) {
        return -1;
    }
    for (std::size_t i = 0; i < sec->n; ++i) {
        if (streq(sec->entries[i].key, key)) {
            std::fprintf(stderr, "configsh: duplicate key '%s' in [%s]\n", key, sec->name);
            return -1;
        }
    }
    if (sec->n >= kMaxEntries) {
        std::fprintf(stderr, "configsh: too many keys in [%s]\n", sec->name);
        return -1;
    }
    Entry *e = &sec->entries[sec->n];
    if (copy_str(e->key, sizeof(e->key), key) != 0) {
        std::fputs("configsh: key too long\n", stderr);
        return -1;
    }
    if (copy_str(e->val, sizeof(e->val), val) != 0) {
        std::fputs("configsh: value too long\n", stderr);
        return -1;
    }
    ++sec->n;
    return 0;
}

bool parse_u64(const char *s, std::uint64_t *out)
{
    if (!s || !*s || !out) {
        return false;
    }
    char *end = nullptr;
    errno = 0;
    unsigned long long v = std::strtoull(s, &end, 10);
    if (errno != 0 || end == s || (end && *end != '\0')) {
        return false;
    }
    *out = static_cast<std::uint64_t>(v);
    return true;
}

} // namespace

bool is_known_section(const char *name)
{
    return streq(name, "prompt") || streq(name, "aliases") || streq(name, "env")
        || streq(name, "history") || streq(name, "general");
}

const Section *find_section(const Config *cfg, const char *name)
{
    if (!cfg || !name) {
        return nullptr;
    }
    for (std::size_t i = 0; i < cfg->n; ++i) {
        if (streq(cfg->sections[i].name, name)) {
            return &cfg->sections[i];
        }
    }
    return nullptr;
}

Section *find_section_mut(Config *cfg, const char *name)
{
    return const_cast<Section *>(find_section(cfg, name));
}

int resolve_config_path(char *out, std::size_t out_cap)
{
    if (!out || out_cap < 16) {
        return -1;
    }
    if (const char *forced = std::getenv("PETRUSH_CONFIG"); forced && *forced) {
        return copy_str(out, out_cap, forced);
    }
    const char *xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg && *xdg) {
        int n = std::snprintf(out, out_cap, "%s/petrush/config.ini", xdg);
        return (n > 0 && static_cast<std::size_t>(n) < out_cap) ? 0 : -1;
    }
    const char *home = std::getenv("HOME");
    if (home && *home) {
        int n = std::snprintf(out, out_cap, "%s/.config/petrush/config.ini", home);
        return (n > 0 && static_cast<std::size_t>(n) < out_cap) ? 0 : -1;
    }
    return copy_str(out, out_cap, "./.config/petrush/config.ini");
}

void load_defaults(Config *cfg)
{
    if (!cfg) {
        return;
    }
    *cfg = Config{};
    Section *prompt = add_section(cfg, "prompt");
    Section *history = add_section(cfg, "history");
    if (prompt) {
        (void)add_entry(prompt, "ps1", "petrush> ");
    }
    if (history) {
        (void)add_entry(history, "max", "1000");
    }
}

int load_config(Config *cfg, const char *path)
{
    if (!cfg || !path) {
        return -1;
    }
    *cfg = Config{};
    if (copy_str(cfg->path, sizeof(cfg->path), path) != 0) {
        std::fputs("configsh: path too long\n", stderr);
        return -1;
    }

    std::FILE *fp = std::fopen(path, "r");
    if (!fp) {
        load_defaults(cfg);
        (void)copy_str(cfg->path, sizeof(cfg->path), path);
        cfg->loaded_from_disk = false;
        return 0;
    }

    Section *cur = nullptr;
    char line[kMaxLine];
    unsigned lineno = 0;
    while (std::fgets(line, static_cast<int>(sizeof(line)), fp)) {
        ++lineno;
        /* Reject truncated lines (no newline and buffer full). */
        std::size_t len = std::strlen(line);
        if (len + 1 == sizeof(line) && line[len - 1] != '\n') {
            std::fprintf(stderr, "configsh: line %u too long\n", lineno);
            std::fclose(fp);
            return -1;
        }
        trim_inplace(line);
        if (line[0] == '\0' || line[0] == '#' || line[0] == ';') {
            continue;
        }
        if (line[0] == '[') {
            char *end = std::strchr(line, ']');
            if (!end || end == line + 1) {
                std::fprintf(stderr, "configsh: line %u: bad section header\n", lineno);
                std::fclose(fp);
                return -1;
            }
            *end = '\0';
            const char *name = line + 1;
            if (!is_known_section(name)) {
                std::fprintf(stderr, "configsh: line %u: unknown section [%s]\n", lineno, name);
                std::fclose(fp);
                return -1;
            }
            if (find_section(cfg, name)) {
                std::fprintf(stderr, "configsh: line %u: duplicate section [%s]\n", lineno, name);
                std::fclose(fp);
                return -1;
            }
            cur = add_section(cfg, name);
            if (!cur) {
                std::fclose(fp);
                return -1;
            }
            continue;
        }
        if (!cur) {
            std::fprintf(stderr, "configsh: line %u: key outside section\n", lineno);
            std::fclose(fp);
            return -1;
        }
        char *eq = std::strchr(line, '=');
        if (!eq) {
            std::fprintf(stderr, "configsh: line %u: expected key=value\n", lineno);
            std::fclose(fp);
            return -1;
        }
        *eq = '\0';
        char *key = line;
        char *val = eq + 1;
        trim_inplace(key);
        trim_inplace(val);
        if (!*key) {
            std::fprintf(stderr, "configsh: line %u: empty key\n", lineno);
            std::fclose(fp);
            return -1;
        }
        if (add_entry(cur, key, val) != 0) {
            std::fclose(fp);
            return -1;
        }
    }
    std::fclose(fp);
    cfg->loaded_from_disk = true;
    return 0;
}

static void dump_section(const Section *sec)
{
    std::printf("[%s]\n", sec->name);
    for (std::size_t i = 0; i < sec->n; ++i) {
        std::printf("%s=%s\n", sec->entries[i].key, sec->entries[i].val);
    }
}

int dump_config(const Config *cfg, const char *section)
{
    if (!cfg) {
        return -1;
    }
    if (section && *section) {
        const Section *sec = find_section(cfg, section);
        if (!sec) {
            std::fprintf(stderr, "configsh: section [%s] not found\n", section);
            return 1;
        }
        dump_section(sec);
        return 0;
    }
    for (std::size_t i = 0; i < cfg->n; ++i) {
        if (i > 0) {
            std::fputc('\n', stdout);
        }
        dump_section(&cfg->sections[i]);
    }
    return 0;
}

int check_config(const Config *cfg, const char *section)
{
    if (!cfg) {
        return 1;
    }
    if (section && *section) {
        if (!is_known_section(section)) {
            std::fprintf(stderr, "configsh: unknown section [%s]\n", section);
            return 1;
        }
        if (!find_section(cfg, section) && cfg->loaded_from_disk) {
            std::fprintf(stderr, "configsh: section [%s] not found\n", section);
            return 1;
        }
    }

    for (std::size_t si = 0; si < cfg->n; ++si) {
        const Section *sec = &cfg->sections[si];
        if (section && *section && !streq(sec->name, section)) {
            continue;
        }
        if (!is_known_section(sec->name)) {
            std::fprintf(stderr, "configsh: unknown section [%s]\n", sec->name);
            return 1;
        }
        for (std::size_t ei = 0; ei < sec->n; ++ei) {
            const Entry *e = &sec->entries[ei];
            if (!e->key[0]) {
                std::fprintf(stderr, "configsh: empty key in [%s]\n", sec->name);
                return 1;
            }
            if (streq(sec->name, "history") && streq(e->key, "max")) {
                std::uint64_t v = 0;
                if (!parse_u64(e->val, &v)) {
                    std::fprintf(stderr, "configsh: history.max must be unsigned integer\n");
                    return 1;
                }
            }
            if (streq(sec->name, "prompt") && streq(e->key, "ps1")) {
                if (std::strlen(e->val) > 200) {
                    std::fputs("configsh: prompt.ps1 too long (max 200)\n", stderr);
                    return 1;
                }
            }
        }
    }
    return 0;
}

} // namespace configsh
