/*
 * test_parser.c — Testes para o parser de linha de comando (TDD)
 *
 * Fase atual: RED → GREEN
 */

#include "acutest.h"
#include "petrush/parser.h"

#include <string.h>
#include <stdlib.h>

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

/* UX-17: sequential lists with ';' (PETRUSH_COND_ALWAYS) */
void test_parse_list_seq_spaced(void)
{
    petrush_list_t list = {0};
    TEST_CHECK(petrush_parse_list("echo a; echo b", &list) == 0);
    TEST_CHECK(list.nitems == 2);
    TEST_CHECK(list.items[0].cond == PETRUSH_COND_ALWAYS);
    TEST_CHECK(list.items[1].cond == PETRUSH_COND_ALWAYS);
    petrush_list_free(&list);
}

void test_parse_list_seq_glued(void)
{
    /* len=1: must not eat the next command's first char */
    petrush_list_t list = {0};
    TEST_CHECK(petrush_parse_list("echo a;echo b", &list) == 0);
    TEST_CHECK(list.nitems == 2);
    TEST_CHECK(list.items[0].cond == PETRUSH_COND_ALWAYS);
    TEST_CHECK(list.items[1].cond == PETRUSH_COND_ALWAYS);
    TEST_CHECK(list.items[1].pl.ncmds == 1);
    TEST_CHECK(list.items[1].pl.cmds[0].argc >= 1);
    TEST_CHECK(strcmp(list.items[1].pl.cmds[0].argv[0], "echo") == 0);
    petrush_list_free(&list);
}

void test_parse_list_seq_three(void)
{
    petrush_list_t list = {0};
    TEST_CHECK(petrush_parse_list("echo a; echo b; echo c", &list) == 0);
    TEST_CHECK(list.nitems == 3);
    TEST_CHECK(list.items[0].cond == PETRUSH_COND_ALWAYS);
    TEST_CHECK(list.items[1].cond == PETRUSH_COND_ALWAYS);
    TEST_CHECK(list.items[2].cond == PETRUSH_COND_ALWAYS);
    petrush_list_free(&list);
}

void test_parse_list_mix_and_seq(void)
{
    petrush_list_t list = {0};
    TEST_CHECK(petrush_parse_list("/bin/false && echo no ; echo yes", &list) == 0);
    TEST_CHECK(list.nitems == 3);
    TEST_CHECK(list.items[0].cond == PETRUSH_COND_ALWAYS);
    TEST_CHECK(list.items[1].cond == PETRUSH_COND_AND);
    TEST_CHECK(list.items[2].cond == PETRUSH_COND_ALWAYS);
    petrush_list_free(&list);
}

void test_parse_list_mix_seq_and(void)
{
    petrush_list_t list = {0};
    TEST_CHECK(petrush_parse_list("echo a ; /bin/true && echo b", &list) == 0);
    TEST_CHECK(list.nitems == 3);
    TEST_CHECK(list.items[0].cond == PETRUSH_COND_ALWAYS);
    TEST_CHECK(list.items[1].cond == PETRUSH_COND_ALWAYS);
    TEST_CHECK(list.items[2].cond == PETRUSH_COND_AND);
    petrush_list_free(&list);
}

void test_parse_list_seq_quoted_literal(void)
{
    petrush_list_t list = {0};
    TEST_CHECK(petrush_parse_list("echo 'a; b'", &list) == 0);
    TEST_CHECK(list.nitems == 1);
    petrush_list_free(&list);

    memset(&list, 0, sizeof(list));
    TEST_CHECK(petrush_parse_list("echo \"a; b\"", &list) == 0);
    TEST_CHECK(list.nitems == 1);
    petrush_list_free(&list);
}

void test_parse_list_seq_trailing(void)
{
    petrush_list_t list = {0};
    TEST_CHECK(petrush_parse_list("echo a ;", &list) == 0);
    TEST_CHECK(list.nitems == 1);
    TEST_CHECK(list.items[0].cond == PETRUSH_COND_ALWAYS);
    petrush_list_free(&list);
}

void test_parse_list_seq_leading_empty(void)
{
    petrush_list_t list = {0};
    TEST_CHECK(petrush_parse_list("; echo a", &list) != 0);
    petrush_list_free(&list);
}

void test_parse_list_seq_middle_empty(void)
{
    petrush_list_t list = {0};
    TEST_CHECK(petrush_parse_list("echo a ; ; echo b", &list) != 0);
    petrush_list_free(&list);
}

void test_parse_list_and_trailing_empty(void)
{
    petrush_list_t list = {0};
    TEST_CHECK(petrush_parse_list("/bin/true &&", &list) != 0);
    petrush_list_free(&list);
}

void test_parse_list_pipe_then_seq(void)
{
    petrush_list_t list = {0};
    TEST_CHECK(petrush_parse_list("echo a | cat ; echo b", &list) == 0);
    TEST_CHECK(list.nitems == 2);
    TEST_CHECK(list.items[0].pl.ncmds == 2);
    TEST_CHECK(list.items[1].pl.ncmds == 1);
    petrush_list_free(&list);
}

/* UX-23: background `&` (separador; && / &> / 2>&1 não contam) */
void test_parse_list_bg_trailing(void)
{
    petrush_list_t list = {0};
    TEST_CHECK(petrush_parse_list("echo a &", &list) == 0);
    TEST_CHECK(list.nitems == 1);
    TEST_CHECK(list.items[0].cond == PETRUSH_COND_ALWAYS);
    TEST_CHECK(list.items[0].background == 1);
    petrush_list_free(&list);
}

void test_parse_list_bg_then_fg(void)
{
    petrush_list_t list = {0};
    TEST_CHECK(petrush_parse_list("echo a & echo b", &list) == 0);
    TEST_CHECK(list.nitems == 2);
    TEST_CHECK(list.items[0].background == 1);
    TEST_CHECK(list.items[0].cond == PETRUSH_COND_ALWAYS);
    TEST_CHECK(list.items[1].background == 0);
    TEST_CHECK(list.items[1].cond == PETRUSH_COND_ALWAYS);
    petrush_list_free(&list);
}

void test_parse_list_bg_two(void)
{
    petrush_list_t list = {0};
    TEST_CHECK(petrush_parse_list("echo a & echo b &", &list) == 0);
    TEST_CHECK(list.nitems == 2);
    TEST_CHECK(list.items[0].background == 1);
    TEST_CHECK(list.items[1].background == 1);
    petrush_list_free(&list);
}

void test_parse_list_and_not_bg(void)
{
    petrush_list_t list = {0};
    TEST_CHECK(petrush_parse_list("/bin/true && /bin/true", &list) == 0);
    TEST_CHECK(list.nitems == 2);
    TEST_CHECK(list.items[0].background == 0);
    TEST_CHECK(list.items[1].background == 0);
    TEST_CHECK(list.items[1].cond == PETRUSH_COND_AND);
    petrush_list_free(&list);
}

void test_parse_list_ampgt_not_bg(void)
{
    petrush_list_t list = {0};
    TEST_CHECK(petrush_parse_list("echo hi &> /tmp/both-ux23.txt", &list) == 0);
    TEST_CHECK(list.nitems == 1);
    TEST_CHECK(list.items[0].background == 0);
    TEST_CHECK(list.items[0].pl.cmds[0].redir_err_to_out == 1);
    petrush_list_free(&list);
}

void test_parse_list_errtoout_not_bg(void)
{
    petrush_list_t list = {0};
    TEST_CHECK(petrush_parse_list("echo hi 2>&1", &list) == 0);
    TEST_CHECK(list.nitems == 1);
    TEST_CHECK(list.items[0].background == 0);
    TEST_CHECK(list.items[0].pl.cmds[0].redir_err_to_out == 1);
    petrush_list_free(&list);
}

void test_parse_list_bg_quoted_literal(void)
{
    petrush_list_t list = {0};
    TEST_CHECK(petrush_parse_list("echo 'a & b'", &list) == 0);
    TEST_CHECK(list.nitems == 1);
    TEST_CHECK(list.items[0].background == 0);
    petrush_list_free(&list);
}

void test_parse_list_bg_leading_empty(void)
{
    petrush_list_t list = {0};
    TEST_CHECK(petrush_parse_list("& echo a", &list) != 0);
    petrush_list_free(&list);
}

void test_parse_list_fg_default(void)
{
    petrush_list_t list = {0};
    TEST_CHECK(petrush_parse_list("echo a ; echo b", &list) == 0);
    TEST_CHECK(list.nitems == 2);
    TEST_CHECK(list.items[0].background == 0);
    TEST_CHECK(list.items[1].background == 0);
    petrush_list_free(&list);
}

void test_parse_list_redir_err_then_seq(void)
{
    petrush_list_t list = {0};
    TEST_CHECK(petrush_parse_list("echo a 2> /tmp/e ; echo b", &list) == 0);
    TEST_CHECK(list.nitems == 2);
    TEST_CHECK(list.items[0].pl.ncmds == 1);
    TEST_CHECK(list.items[0].pl.cmds[0].redir_err != NULL);
    TEST_CHECK(strcmp(list.items[0].pl.cmds[0].redir_err, "/tmp/e") == 0);
    TEST_CHECK(list.items[1].cond == PETRUSH_COND_ALWAYS);
    petrush_list_free(&list);
}

/* UX-16: stderr redirs 2> 2>> 2>&1 &> */
void test_parse_redir_err(void)
{
    petrush_cmd_t cmd = {0};
    TEST_CHECK(petrush_parse("echo hi 2> /tmp/err.txt", &cmd) == 0);
    TEST_CHECK(cmd.argc == 2);
    TEST_CHECK(cmd.argv && strcmp(cmd.argv[0], "echo") == 0);
    TEST_CHECK(cmd.argv && cmd.argc >= 2 && strcmp(cmd.argv[1], "hi") == 0);
    TEST_CHECK(cmd.redir_err && strcmp(cmd.redir_err, "/tmp/err.txt") == 0);
    TEST_CHECK(cmd.redir_out == NULL);
    TEST_CHECK(cmd.redir_err_to_out == 0);
    TEST_CHECK(cmd.redir_err_append == 0);
    petrush_cmd_free(&cmd);
}

void test_parse_redir_err_glued(void)
{
    petrush_cmd_t cmd = {0};
    TEST_CHECK(petrush_parse("echo hi 2>/tmp/err.txt", &cmd) == 0);
    TEST_CHECK(cmd.argc == 2);
    TEST_CHECK(cmd.redir_err && strcmp(cmd.redir_err, "/tmp/err.txt") == 0);
    TEST_CHECK(cmd.redir_out == NULL);
    TEST_CHECK(cmd.redir_err_to_out == 0);
    TEST_CHECK(cmd.redir_err_append == 0);
    petrush_cmd_free(&cmd);
}

void test_parse_redir_err_append(void)
{
    petrush_cmd_t cmd = {0};
    TEST_CHECK(petrush_parse("echo hi 2>> /tmp/err.txt", &cmd) == 0);
    TEST_CHECK(cmd.argc == 2);
    TEST_CHECK(cmd.redir_err && strcmp(cmd.redir_err, "/tmp/err.txt") == 0);
    TEST_CHECK(cmd.redir_err_append == 1);
    TEST_CHECK(cmd.redir_err_to_out == 0);
    petrush_cmd_free(&cmd);
}

void test_parse_redir_err_to_out(void)
{
    petrush_cmd_t cmd = {0};
    TEST_CHECK(petrush_parse("echo hi 2>&1", &cmd) == 0);
    TEST_CHECK(cmd.argc == 2);
    TEST_CHECK(strcmp(cmd.argv[1], "hi") == 0);
    TEST_CHECK(cmd.redir_err == NULL);
    TEST_CHECK(cmd.redir_err_to_out == 1);
    TEST_CHECK(cmd.redir_out == NULL);
    petrush_cmd_free(&cmd);
}

void test_parse_redir_ampgt(void)
{
    petrush_cmd_t cmd = {0};
    TEST_CHECK(petrush_parse("echo hi &> /tmp/both.txt", &cmd) == 0);
    TEST_CHECK(cmd.argc == 2);
    TEST_CHECK(strcmp(cmd.argv[0], "echo") == 0);
    TEST_CHECK(strcmp(cmd.argv[1], "hi") == 0);
    TEST_CHECK(cmd.redir_out && strcmp(cmd.redir_out, "/tmp/both.txt") == 0);
    TEST_CHECK(cmd.redir_append == 0);
    TEST_CHECK(cmd.redir_err_to_out == 1);
    TEST_CHECK(cmd.redir_err == NULL);
    petrush_cmd_free(&cmd);
}

void test_parse_redir_out_then_err_merge(void)
{
    petrush_cmd_t cmd = {0};
    TEST_CHECK(petrush_parse("echo hi > /tmp/o.txt 2>&1", &cmd) == 0);
    TEST_CHECK(cmd.argc == 2);
    TEST_CHECK(cmd.redir_out && strcmp(cmd.redir_out, "/tmp/o.txt") == 0);
    TEST_CHECK(cmd.redir_err_to_out == 1);
    TEST_CHECK(cmd.redir_err == NULL);
    petrush_cmd_free(&cmd);
}

void test_parse_quoted_twogt_literal(void)
{
    petrush_cmd_t cmd = {0};
    TEST_CHECK(petrush_parse("echo \"2>\"", &cmd) == 0);
    TEST_CHECK(cmd.argc == 2);
    TEST_CHECK(strcmp(cmd.argv[1], "2>") == 0);
    TEST_CHECK(cmd.redir_err == NULL);
    TEST_CHECK(cmd.redir_out == NULL);
    TEST_CHECK(cmd.redir_err_to_out == 0);
    petrush_cmd_free(&cmd);
}

void test_parse_quoted_ampgt_literal(void)
{
    petrush_cmd_t cmd = {0};
    TEST_CHECK(petrush_parse("echo '&>'", &cmd) == 0);
    TEST_CHECK(cmd.argc == 2);
    TEST_CHECK(strcmp(cmd.argv[1], "&>") == 0);
    TEST_CHECK(cmd.redir_out == NULL);
    TEST_CHECK(cmd.redir_err_to_out == 0);
    petrush_cmd_free(&cmd);
}

void test_parse_digit_space_gt_is_stdout(void)
{
    petrush_cmd_t cmd = {0};
    TEST_CHECK(petrush_parse("echo 2 > /tmp/o.txt", &cmd) == 0);
    TEST_CHECK(cmd.argc == 2);
    TEST_CHECK(strcmp(cmd.argv[1], "2") == 0);
    TEST_CHECK(cmd.redir_out && strcmp(cmd.redir_out, "/tmp/o.txt") == 0);
    TEST_CHECK(cmd.redir_err == NULL);
    TEST_CHECK(cmd.redir_err_to_out == 0);
    petrush_cmd_free(&cmd);
}

/* Caracterização tokenize: 12> = WORD "12" + TOK_GT (não 2>) */
void test_parse_twelvegt_is_word_then_stdout(void)
{
    petrush_cmd_t cmd = {0};
    TEST_CHECK(petrush_parse("echo 12> /tmp/o.txt", &cmd) == 0);
    TEST_CHECK(cmd.argc == 2);
    TEST_CHECK(strcmp(cmd.argv[0], "echo") == 0);
    TEST_CHECK(strcmp(cmd.argv[1], "12") == 0);
    TEST_CHECK(cmd.redir_out && strcmp(cmd.redir_out, "/tmp/o.txt") == 0);
    TEST_CHECK(cmd.redir_err == NULL);
    TEST_CHECK(cmd.redir_err_to_out == 0);
    petrush_cmd_free(&cmd);
}

void test_parse_redir_err_incomplete(void)
{
    petrush_cmd_t cmd = {0};
    TEST_CHECK(petrush_parse("echo hi 2>", &cmd) != 0);
    petrush_cmd_free(&cmd);
    memset(&cmd, 0, sizeof(cmd));
    TEST_CHECK(petrush_parse("echo hi &>", &cmd) != 0);
    petrush_cmd_free(&cmd);
}

void test_parse_redir_err_bad_fd(void)
{
    petrush_cmd_t cmd = {0};
    TEST_CHECK(petrush_parse("echo hi 2>&2", &cmd) == -1);
    petrush_cmd_free(&cmd);
    memset(&cmd, 0, sizeof(cmd));
    TEST_CHECK(petrush_parse("echo hi 2>&", &cmd) == -1);
    petrush_cmd_free(&cmd);
    memset(&cmd, 0, sizeof(cmd));
    /* tokenizer recusa 2>&12 (não abrir arquivo "&12") */
    TEST_CHECK(petrush_parse("echo hi 2>&12", &cmd) == -1);
    petrush_cmd_free(&cmd);
}

void test_parse_pipeline_redir_err_first_stage(void)
{
    petrush_pipeline_t pl = {0};
    TEST_CHECK(petrush_parse_pipeline("ls 2>/tmp/e | cat", &pl) == 0);
    TEST_CHECK(pl.ncmds == 2);
    TEST_CHECK(pl.cmds[0].redir_err && strcmp(pl.cmds[0].redir_err, "/tmp/e") == 0);
    TEST_CHECK(pl.cmds[1].redir_err == NULL);
    TEST_CHECK(pl.cmds[1].redir_err_to_out == 0);
    petrush_pipeline_free(&pl);
}

void test_parse_redir_out_regression(void)
{
    petrush_cmd_t cmd = {0};
    TEST_CHECK(petrush_parse("echo hi > /tmp/out.txt", &cmd) == 0);
    TEST_CHECK(cmd.redir_out && strcmp(cmd.redir_out, "/tmp/out.txt") == 0);
    TEST_CHECK(cmd.redir_err == NULL);
    TEST_CHECK(cmd.redir_err_to_out == 0);
    petrush_cmd_free(&cmd);
}

/* UX-18: argv_quoted paralelo (aspas stripadas; flag preserva origem) */
void test_parse_argv_quoted_flag_double(void)
{
    petrush_cmd_t cmd = {0};
    TEST_CHECK(petrush_parse("echo \"*.c\"", &cmd) == 0);
    TEST_CHECK(cmd.argc == 2);
    TEST_CHECK(strcmp(cmd.argv[1], "*.c") == 0);
    TEST_CHECK(cmd.argv_quoted != NULL);
    if (cmd.argv_quoted) {
        TEST_CHECK(cmd.argv_quoted[0] == 0);
        TEST_CHECK(cmd.argv_quoted[1] == 1);
    }
    petrush_cmd_free(&cmd);
}

void test_parse_argv_quoted_flag_unquoted(void)
{
    petrush_cmd_t cmd = {0};
    TEST_CHECK(petrush_parse("echo *.c", &cmd) == 0);
    TEST_CHECK(cmd.argc == 2);
    TEST_CHECK(strcmp(cmd.argv[1], "*.c") == 0);
    TEST_CHECK(cmd.argv_quoted != NULL);
    if (cmd.argv_quoted) {
        TEST_CHECK(cmd.argv_quoted[0] == 0);
        TEST_CHECK(cmd.argv_quoted[1] == 0);
    }
    petrush_cmd_free(&cmd);
}

/* OSH-3: if/then/fi — um item IF, cond+then */
void test_parse_if_then_fi(void)
{
    petrush_list_t list = {0};
    TEST_CHECK(petrush_parse_list("if true; then echo a; fi", &list) == 0);
    TEST_CHECK(list.nitems == 1);
    TEST_CHECK(list.items[0].kind == PETRUSH_ITEM_IF);
    TEST_CHECK(list.items[0].ifc.narms == 1);
    TEST_CHECK(list.items[0].ifc.arms[0].is_else == 0);
    TEST_CHECK(list.items[0].ifc.arms[0].cond.nitems == 1);
    TEST_CHECK(list.items[0].ifc.arms[0].cond.items[0].kind == PETRUSH_ITEM_PIPELINE);
    TEST_CHECK(list.items[0].ifc.arms[0].cond.items[0].pl.cmds[0].argc >= 1);
    TEST_CHECK(strcmp(list.items[0].ifc.arms[0].cond.items[0].pl.cmds[0].argv[0],
                       "true") == 0);
    TEST_CHECK(list.items[0].ifc.arms[0].body.nitems == 1);
    TEST_CHECK(strcmp(list.items[0].ifc.arms[0].body.items[0].pl.cmds[0].argv[0],
                       "echo") == 0);
    petrush_list_free(&list);
}

/* OSH-3: else */
void test_parse_if_else_fi(void)
{
    petrush_list_t list = {0};
    TEST_CHECK(petrush_parse_list("if false; then echo a; else echo b; fi",
                                  &list) == 0);
    TEST_CHECK(list.nitems == 1);
    TEST_CHECK(list.items[0].kind == PETRUSH_ITEM_IF);
    TEST_CHECK(list.items[0].ifc.narms == 2);
    TEST_CHECK(list.items[0].ifc.arms[0].is_else == 0);
    TEST_CHECK(list.items[0].ifc.arms[1].is_else == 1);
    TEST_CHECK(list.items[0].ifc.arms[1].cond.nitems == 0);
    TEST_CHECK(list.items[0].ifc.arms[1].body.nitems == 1);
    TEST_CHECK(strcmp(list.items[0].ifc.arms[1].body.items[0].pl.cmds[0].argv[1],
                       "b") == 0);
    petrush_list_free(&list);
}

/* OSH-3: um elif */
void test_parse_if_elif_fi(void)
{
    petrush_list_t list = {0};
    TEST_CHECK(petrush_parse_list(
                   "if false; then echo a; elif true; then echo b; fi",
                   &list) == 0);
    TEST_CHECK(list.nitems == 1);
    TEST_CHECK(list.items[0].kind == PETRUSH_ITEM_IF);
    TEST_CHECK(list.items[0].ifc.narms == 2);
    TEST_CHECK(list.items[0].ifc.arms[0].is_else == 0);
    TEST_CHECK(list.items[0].ifc.arms[1].is_else == 0);
    TEST_CHECK(strcmp(list.items[0].ifc.arms[1].cond.items[0].pl.cmds[0].argv[0],
                       "true") == 0);
    TEST_CHECK(strcmp(list.items[0].ifc.arms[1].body.items[0].pl.cmds[0].argv[1],
                       "b") == 0);
    petrush_list_free(&list);
}

/* OSH-3: fi quoted nao fecha; palavra fi como argv */
void test_parse_if_fi_quoted_literal(void)
{
    petrush_list_t list = {0};
    TEST_CHECK(petrush_parse_list("if true; then echo \"fi\"; fi", &list) == 0);
    TEST_CHECK(list.nitems == 1);
    TEST_CHECK(list.items[0].kind == PETRUSH_ITEM_IF);
    TEST_CHECK(list.items[0].ifc.narms == 1);
    TEST_CHECK(list.items[0].ifc.arms[0].body.nitems == 1);
    TEST_CHECK(strcmp(list.items[0].ifc.arms[0].body.items[0].pl.cmds[0].argv[1],
                       "fi") == 0);
    petrush_list_free(&list);
}

/* OSH-3: if apos ; numa lista */
void test_parse_if_after_seq(void)
{
    petrush_list_t list = {0};
    TEST_CHECK(petrush_parse_list("echo x; if true; then echo y; fi", &list) == 0);
    TEST_CHECK(list.nitems == 2);
    TEST_CHECK(list.items[0].kind == PETRUSH_ITEM_PIPELINE);
    TEST_CHECK(list.items[1].kind == PETRUSH_ITEM_IF);
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
    { "parse_list_seq_spaced", test_parse_list_seq_spaced },
    { "parse_list_seq_glued", test_parse_list_seq_glued },
    { "parse_list_seq_three", test_parse_list_seq_three },
    { "parse_list_mix_and_seq", test_parse_list_mix_and_seq },
    { "parse_list_mix_seq_and", test_parse_list_mix_seq_and },
    { "parse_list_seq_quoted_literal", test_parse_list_seq_quoted_literal },
    { "parse_list_seq_trailing", test_parse_list_seq_trailing },
    { "parse_list_seq_leading_empty", test_parse_list_seq_leading_empty },
    { "parse_list_seq_middle_empty", test_parse_list_seq_middle_empty },
    { "parse_list_and_trailing_empty", test_parse_list_and_trailing_empty },
    { "parse_list_pipe_then_seq", test_parse_list_pipe_then_seq },
    { "parse_list_redir_err_then_seq", test_parse_list_redir_err_then_seq },
    { "parse_list_bg_trailing", test_parse_list_bg_trailing },
    { "parse_list_bg_then_fg", test_parse_list_bg_then_fg },
    { "parse_list_bg_two", test_parse_list_bg_two },
    { "parse_list_and_not_bg", test_parse_list_and_not_bg },
    { "parse_list_ampgt_not_bg", test_parse_list_ampgt_not_bg },
    { "parse_list_errtoout_not_bg", test_parse_list_errtoout_not_bg },
    { "parse_list_bg_quoted_literal", test_parse_list_bg_quoted_literal },
    { "parse_list_bg_leading_empty", test_parse_list_bg_leading_empty },
    { "parse_list_fg_default", test_parse_list_fg_default },
    { "parse_redir_err", test_parse_redir_err },
    { "parse_redir_err_glued", test_parse_redir_err_glued },
    { "parse_redir_err_append", test_parse_redir_err_append },
    { "parse_redir_err_to_out", test_parse_redir_err_to_out },
    { "parse_redir_ampgt", test_parse_redir_ampgt },
    { "parse_redir_out_then_err_merge", test_parse_redir_out_then_err_merge },
    { "parse_quoted_twogt_literal", test_parse_quoted_twogt_literal },
    { "parse_quoted_ampgt_literal", test_parse_quoted_ampgt_literal },
    { "parse_digit_space_gt_is_stdout", test_parse_digit_space_gt_is_stdout },
    { "parse_twelvegt_is_word_then_stdout", test_parse_twelvegt_is_word_then_stdout },
    { "parse_redir_err_incomplete", test_parse_redir_err_incomplete },
    { "parse_redir_err_bad_fd", test_parse_redir_err_bad_fd },
    { "parse_pipeline_redir_err_first_stage", test_parse_pipeline_redir_err_first_stage },
    { "parse_redir_out_regression", test_parse_redir_out_regression },
    { "parse_argv_quoted_flag_double", test_parse_argv_quoted_flag_double },
    { "parse_argv_quoted_flag_unquoted", test_parse_argv_quoted_flag_unquoted },
    { "parse_if_then_fi", test_parse_if_then_fi },
    { "parse_if_else_fi", test_parse_if_else_fi },
    { "parse_if_elif_fi", test_parse_if_elif_fi },
    { "parse_if_fi_quoted_literal", test_parse_if_fi_quoted_literal },
    { "parse_if_after_seq", test_parse_if_after_seq },
    { NULL, NULL }
};
