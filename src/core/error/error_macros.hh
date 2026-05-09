#pragma once

#include "core/typedefs.hh"

enum ErrorHandlerType {
    ERR_HANDLER_ERROR,
    ERR_HANDLER_WARNING,
    ERR_HANDLER_FATAL,
};

_NO_INLINE_ void err_print_error(const char *p_function, const char *p_file, int p_line,
                                 const char *p_error, ErrorHandlerType p_type = ERR_HANDLER_ERROR);

#ifdef __GNUC__
#define FUNCTION_STR __FUNCTION__
#else
#define FUNCTION_STR __FUNCTION__
#endif

#define _ERR_PRINT(m_msg, m_type) err_print_error(FUNCTION_STR, __FILE__, __LINE__, m_msg, m_type)

#define ERR_FAIL()                                                \
    do {                                                          \
        _ERR_PRINT("Method/function failed.", ERR_HANDLER_ERROR); \
        return;                                                   \
    } while (0)

#define ERR_FAIL_MSG(m_msg)                   \
    do {                                      \
        _ERR_PRINT(m_msg, ERR_HANDLER_ERROR); \
        return;                               \
    } while (0)

#define ERR_FAIL_V(m_retval)                                                                 \
    do {                                                                                     \
        _ERR_PRINT("Method/function failed. Returning: " _STR(m_retval), ERR_HANDLER_ERROR); \
        return m_retval;                                                                     \
    } while (0)

#define ERR_FAIL_V_MSG(m_retval, m_msg)       \
    do {                                      \
        _ERR_PRINT(m_msg, ERR_HANDLER_ERROR); \
        return m_retval;                      \
    } while (0)

#define ERR_FAIL_COND(m_cond)                                                    \
    do {                                                                         \
        if (unlikely(m_cond)) {                                                  \
            _ERR_PRINT("Condition \"" #m_cond "\" is true.", ERR_HANDLER_ERROR); \
            return;                                                              \
        }                                                                        \
    } while (0)

#define ERR_FAIL_COND_MSG(m_cond, m_msg)          \
    do {                                          \
        if (unlikely(m_cond)) {                   \
            _ERR_PRINT(m_msg, ERR_HANDLER_ERROR); \
            return;                               \
        }                                         \
    } while (0)

#define ERR_FAIL_COND_V(m_cond, m_retval)                                        \
    do {                                                                         \
        if (unlikely(m_cond)) {                                                  \
            _ERR_PRINT("Condition \"" #m_cond "\" is true.", ERR_HANDLER_ERROR); \
            return m_retval;                                                     \
        }                                                                        \
    } while (0)

#define ERR_FAIL_COND_V_MSG(m_cond, m_retval, m_msg) \
    do {                                             \
        if (unlikely(m_cond)) {                      \
            _ERR_PRINT(m_msg, ERR_HANDLER_ERROR);    \
            return m_retval;                         \
        }                                            \
    } while (0)

#define WARN_PRINT(m_msg)                       \
    do {                                        \
        _ERR_PRINT(m_msg, ERR_HANDLER_WARNING); \
    } while (0)

#define WARN_PRINT_ONCE(m_msg)                      \
    do {                                            \
        static bool warning_printed = false;        \
        if (!warning_printed) {                     \
            warning_printed = true;                 \
            _ERR_PRINT(m_msg, ERR_HANDLER_WARNING); \
        }                                           \
    } while (0)

#ifdef DEV_ENABLED

#define DEV_ASSERT(m_cond)                                                \
    do {                                                                  \
        if (unlikely(!(m_cond))) {                                        \
            _ERR_PRINT("DEV_ASSERT failed: " #m_cond, ERR_HANDLER_FATAL); \
            std::abort();                                                 \
        }                                                                 \
    } while (0)

#else

#define DEV_ASSERT(m_cond) \
    do {                   \
    } while (0)
#endif

#ifdef DEV_ENABLED

#define DEV_ASSERT_MSG(m_cond, m_msg)             \
    do {                                          \
        if (unlikely(!(m_cond))) {                \
            _ERR_PRINT(m_msg, ERR_HANDLER_FATAL); \
            std::abort();                         \
        }                                         \
    } while (0)

#else

#define DEV_ASSERT_MSG(m_cond, m_msg) \
    do {                              \
    } while (0)

#endif
