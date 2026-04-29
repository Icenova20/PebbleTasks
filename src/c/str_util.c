#include "str_util.h"
#include <stdlib.h>
#include <string.h>

char *str_util_strdup(const char *s) {
  if (!s) {
    return NULL;
  }
  size_t n = strlen(s) + 1;
  char *d = (char *)malloc(n);
  if (d) {
    memcpy(d, s, n);
  }
  return d;
}

void str_util_free_titles(char **arr, int n) {
  for (int i = 0; i < n; i++) {
    if (arr[i]) {
      free(arr[i]);
      arr[i] = NULL;
    }
  }
}

int str_util_split_lines(char *buf, char *lines_out[], int max) {
  int c = 0;
  char *p = buf;
  while (c < max && p && *p) {
    char *nl = strchr(p, '\n');
    if (nl) {
      *nl = '\0';
    }
    if (*p) {
      lines_out[c] = str_util_strdup(p);
      if (!lines_out[c]) {
        break;
      }
      c++;
    }
    if (!nl) {
      break;
    }
    p = nl + 1;
  }
  return c;
}
