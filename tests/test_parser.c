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

/* NEW-20: redirecionamento e pipes */
void test_parse_redir_out(void)
{
    petrush_cmd_t cmd = {0};
    TEST_CHECK(petrush_parse("echo hi > /tmp/out.txt", &cmd) == 0);
    TEST_CHECK(cmd.argc == 2);
    TEST_CHECK(strcmp(cmd.argv[0], "echo") == 0);
    TEST_CHECK(strcmp(cmd.argv[1], "hi") == 0);
    TEST_CHECK(cmd.redir_out != NULL);
    TEST_CHECK(strcmp(cmd.redir_out, "/tmp/out.txt") == 0);
    TEST_CHECK(cmd.redir_append == 0);
    TEST_CHECK(cmd.redir_in == NULL);
    petrush_cmd_free(&cmd);
}

void test_parse_redir_append_and_in(void)
{
    petrush_cmd_t cmd = {0};
    TEST_CHECK(petrush_parse("cat < /tmp/in.txt >> /tmp/out.txt", &cmd) == 0);
    TEST_CHECK(cmd.argc == 1);
    TEST_CHECK(strcmp(cmd.argv[0], "cat") == 0);
    TEST_CHECK(cmd.redir_in && strcmp(cmd.redir_in, "/tmp/in.txt") == 0);
    TEST_CHECK(cmd.redir_out && strcmp(cmd.redir_out, "/tmp/out.txt") == 0);
    TEST_CHECK(cmd.redir_append == 1);
    petrush_cmd_free(&cmd);
}

void test_parse_pipeline_two(void)
{
    petrush_pipeline_t pl = {0};
    TEST_CHECK(petrush_parse_pipeline("echo hello | cat", &pl) == 0);
    TEST_CHECK(pl.ncmds == 2);
    TEST_CHECK(pl.cmds[0].argc == 2);
    TEST_CHECK(strcmp(pl.cmds[0].argv[0], "echo") == 0);
    TEST_CHECK(strcmp(pl.cmds[0].argv[1], "hello") == 0);
    TEST_CHECK(pl.cmds[1].argc == 1);
    TEST_CHECK(strcmp(pl.cmds[1].argv[0], "cat") == 0);
    petrush_pipeline_free(&pl);
}

void test_parse_pipeline_rejects_empty_stage(void)
{
    petrush_pipeline_t pl = {0};
    TEST_CHECK(petrush_parse_pipeline("echo a | | cat", &pl) != 0);
    petrush_pipeline_free(&pl);
}

void test_parse_pipe_fails_single_api(void)
{
    petrush_cmd_t cmd = {0};
    TEST_CHECK(petrush_parse("echo a | cat", &cmd) != 0);
    petrush_cmd_free(&cmd);
}

void test_parse_list_and(void)
{
    petrush_list_t list = {0};
    TEST_CHECK(petrush_parse_list("/bin/true && /bin/true", &list) == 0);
    TEST_CHECK(list.nitems == 2);
    TEST_CHECK(list.items[0].cond == PETRUSH_COND_ALWAYS);
    TEST_CHECK(list.items[1].cond == PETRUSH_COND_AND);
    petrush_list_free(&list);
}

void test_parse_list_or(void)
{
    petrush_list_t list = {0};
    TEST_CHECK(petrush_parse_list("/bin/false || /bin/true", &list) == 0);
    TEST_CHECK(list.nitems == 2);
    TEST_CHECK(list.items[1].cond == PETRUSH_COND_OR);
    petrush_list_free(&list);
}

TEST_LIST = {
    { "parse_simple", test_parse_simple },
    { "parse_quoted_simple", test_parse_quoted_simple },
    { "parse_single_quotes", test_parse_single_quotes },
    { "parse_empty", test_parse_empty },
    { "parse_whitespace_only", test_parse_whitespace_only },
    { "parse_many_args_for_realloc", test_parse_many_args_for_realloc },
    { "parse_redir_out", test_parse_redir_out },
    { "parse_redir_append_and_in", test_parse_redir_append_and_in },
    { "parse_pipeline_two", test_parse_pipeline_two },
    { "parse_pipeline_rejects_empty_stage", test_parse_pipeline_rejects_empty_stage },
    { "parse_pipe_fails_single_api", test_parse_pipe_fails_single_api },
    { "parse_list_and", test_parse_list_and },
    { "parse_list_or", test_parse_list_or },
    { NULL, NULL }
};
