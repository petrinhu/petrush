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

TEST_LIST = {
    { "parse_simple", test_parse_simple },
    { "parse_quoted_simple", test_parse_quoted_simple },
    { "parse_single_quotes", test_parse_single_quotes },
    { NULL, NULL }
};
