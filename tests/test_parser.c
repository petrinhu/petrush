/*
 * test_parser.c — Testes para o parser de linha de comando (TDD)
 *
 * Fase atual: RED → GREEN
 */

#include "acutest.h"
#include "petrush/parser.h"

#include <string.h>

/* ===================== TESTES ===================== */

void test_parse_simple(void)
{
    petrush_cmd_t cmd = {0};

    TEST_CHECK(petrush_parse("echo hello world", &cmd) == 0);
    TEST_CHECK(cmd.argc == 3);
    TEST_CHECK(cmd.argv != NULL);
    TEST_CHECK(strcmp(cmd.argv[0], "echo") == 0);
    TEST_CHECK(strcmp(cmd.argv[1], "hello") == 0);
    TEST_CHECK(strcmp(cmd.argv[2], "world") == 0);

    petrush_cmd_free(&cmd);
}

void test_parse_quoted_simple(void)
{
    petrush_cmd_t cmd = {0};

    TEST_CHECK(petrush_parse("echo \"hello world\"", &cmd) == 0);
    TEST_CHECK(cmd.argc == 2);
    TEST_CHECK(strcmp(cmd.argv[0], "echo") == 0);
    TEST_CHECK(strcmp(cmd.argv[1], "hello world") == 0);

    petrush_cmd_free(&cmd);
}

void test_parse_single_quotes(void)
{
    petrush_cmd_t cmd = {0};

    TEST_CHECK(petrush_parse("echo 'hello world'", &cmd) == 0);
    TEST_CHECK(cmd.argc == 2);
    TEST_CHECK(strcmp(cmd.argv[0], "echo") == 0);
    TEST_CHECK(strcmp(cmd.argv[1], "hello world") == 0);

    petrush_cmd_free(&cmd);
}

void test_parse_empty(void)
{
    petrush_cmd_t cmd = {0};

    TEST_CHECK(petrush_parse("", &cmd) == 0);
    TEST_CHECK(cmd.argc == 0);
    TEST_CHECK(cmd.argv == NULL);

    petrush_cmd_free(&cmd);
}

void test_parse_whitespace_only(void)
{
    petrush_cmd_t cmd = {0};

    TEST_CHECK(petrush_parse("   \t  ", &cmd) == 0);
    TEST_CHECK(cmd.argc == 0);

    petrush_cmd_free(&cmd);
}

void test_parse_many_args_for_realloc(void)
{
    /* > INITIAL_ARGV_CAPACITY to exercise doubling + final NULL terminator path */
    petrush_cmd_t cmd = {0};
    char input[1024];
    strcpy(input, "cmd a1 a2 a3 a4 a5 a6 a7 a8 a9 a10 a11 a12 a13 a14 a15 a16 a17 a18 a19 a20");

    TEST_CHECK(petrush_parse(input, &cmd) == 0);
    TEST_CHECK(cmd.argc == 21);  /* "cmd" + 20 args */
    TEST_CHECK(cmd.argv != NULL);
    TEST_CHECK(strcmp(cmd.argv[0], "cmd") == 0);
    TEST_CHECK(strcmp(cmd.argv[20], "a20") == 0);
    TEST_CHECK(cmd.argv[21] == NULL); /* terminator guaranteed */

    petrush_cmd_free(&cmd);
}

TEST_LIST = {
    { "parse_simple", test_parse_simple },
    { "parse_quoted_simple", test_parse_quoted_simple },
    { "parse_single_quotes", test_parse_single_quotes },
    { "parse_empty", test_parse_empty },
    { "parse_whitespace_only", test_parse_whitespace_only },
    { "parse_many_args_for_realloc", test_parse_many_args_for_realloc },
    { NULL, NULL }
};
