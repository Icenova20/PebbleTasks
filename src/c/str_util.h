#pragma once

#include <stddef.h>

char *str_util_strdup(const char *s);
void str_util_free_titles(char **arr, int n);
/** Splits buf by newlines; writes pointers into lines_out; mutates buf with temporary NULs. */
int str_util_split_lines(char *buf, char *lines_out[], int max);
