#include "error_macros.hh"

#include <cstdio>

static const char *error_handler_type_string[] = {
    "ERROR",
    "WARNING",
    "FATAL",
};

void err_print_error(const char *p_function, const char *p_file, int p_line, const char *p_error,
                     ErrorHandlerType p_type) {
    fprintf(stderr, "[%s] %s:%d @ %s(): %s\n", error_handler_type_string[p_type], p_file, p_line,
            p_function, p_error);
}
