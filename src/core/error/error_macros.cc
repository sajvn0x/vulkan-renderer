#include "error_macros.hh"

#include <cstdio>

void err_print_error(const char *p_function, const char *p_file, int p_line, const char *p_error) {
    fprintf(stderr, "[%s:%d:%s]: %s", p_file, p_line, p_function, p_error);
}
