#pragma once

#include "core/typedefs.hh"

#ifdef __GNUC__
#define FUNCTION_STR __FUNCTION__
#else
#define FUNCTION_STR __FUNCTION__
#endif

_NO_INLINE_ void err_print_error(const char *p_function, const char *p_file,
                                 int p_line, const char *p_error);

#define ERR_FAIL_COND_V(m_cond, m_retval)                                      \
    if (unlikely(m_cond)) {                                                    \
        err_print_error(FUNCTION_STR, __FILE__, __LINE__,                      \
                        "Condition \"" _STR(                                   \
                            m_cond) "\" is true. Returning: " _STR(m_retval)); \
        return m_retval;                                                       \
    } else                                                                     \
        ((void)0)
