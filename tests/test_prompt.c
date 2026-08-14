/*
 * test_prompt.c — TDD UX-15 PS1 escapes
 */

#include "acutest.h"
#include "petrush/prompt.h"
#include "petrush/env.h"

#include <string.h>
#include <unistd.h>
#include <limits.h>

void test_prompt_default(void)
{
    char buf[256];
    prompt_render(NULL, buf, sizeof(buf));
    TEST_CHECK(strstr(buf, "petrush") != NULL);
}

void test_prompt_literal(void)
{
    char buf[256];
    prompt_render("hi> ", buf, sizeof(buf));
    TEST_CHECK(strcmp(buf, "hi> ") == 0);
}

void test_prompt_backslash_w(void)
{
    char buf[PATH_MAX + 32];
    prompt_render("\\w$ ", buf, sizeof(buf));
    char cwd[PATH_MAX];
    TEST_CHECK(getcwd(cwd, sizeof(cwd)) != NULL);
    TEST_CHECK(strstr(buf, cwd) != NULL);
    TEST_CHECK(strchr(buf, '$') != NULL || strchr(buf, '#') != NULL);
}

void test_prompt_user(void)
{
    petrush_setenv("USER", "alice_test", 1);
    char buf[256];
    prompt_render("\\u>", buf, sizeof(buf));
    TEST_CHECK(strcmp(buf, "alice_test>") == 0);
}

void test_prompt_host(void)
{
    char expect[256];
    char buf[256];
    if (gethostname(expect, sizeof(expect)) != 0) {
        prompt_render("\\h", buf, sizeof(buf));
        TEST_CHECK(strcmp(buf, "?") == 0);
        return;
    }
    expect[sizeof(expect) - 1] = '\0';
    char *dot = strchr(expect, '.');
    if (dot) *dot = '\0';
    prompt_render("\\h", buf, sizeof(buf));
    TEST_CHECK(strcmp(buf, expect) == 0);
}

void test_prompt_newline(void)
{
    char buf[16];
    prompt_render("a\\nb", buf, sizeof(buf));
    TEST_CHECK(strcmp(buf, "a\nb") == 0);
}

void test_prompt_dollar(void)
{
    char buf[8];
    prompt_render("\\$", buf, sizeof(buf));
    if (geteuid() == 0) {
        TEST_CHECK(strcmp(buf, "#") == 0);
    } else {
        TEST_CHECK(strcmp(buf, "$") == 0);
    }
}

void test_prompt_backslash(void)
{
    char buf[8];
    prompt_render("\\\\", buf, sizeof(buf));
    TEST_CHECK(strcmp(buf, "\\") == 0);
}

void test_prompt_unknown_escape(void)
{
    char buf[16];
    prompt_render("\\q", buf, sizeof(buf));
    TEST_CHECK(strcmp(buf, "\\q") == 0);
}

void test_prompt_empty_out(void)
{
    TEST_CHECK(prompt_render("x", NULL, 8) == NULL);
    char buf[1] = { 'Z' };
    TEST_CHECK(prompt_render("x", buf, 0) == buf);
    TEST_CHECK(buf[0] == 'Z');
}

TEST_LIST = {
    { "prompt_default", test_prompt_default },
    { "prompt_literal", test_prompt_literal },
    { "prompt_backslash_w", test_prompt_backslash_w },
    { "prompt_user", test_prompt_user },
    { "prompt_host", test_prompt_host },
    { "prompt_newline", test_prompt_newline },
    { "prompt_dollar", test_prompt_dollar },
    { "prompt_backslash", test_prompt_backslash },
    { "prompt_unknown_escape", test_prompt_unknown_escape },
    { "prompt_empty_out", test_prompt_empty_out },
    { NULL, NULL }
};
