#pragma once

// use this macro for debugging
// put arbritrary code in the argument and it will
// be executed if the DEBUG_LOG compile flag is set.
// Example:
// SLIM_DEBUG(printf("Hello World\n"));
// for debugging with a specific level,
// the flag DEBUG_LEVEL must be set
// by slim::DEBUG_LEVEL = level;
// The default value is 0
// Example:
// SLIM_DEBUG_WITH_LEVEL(1, printf("Hello World\n"));

// Some common level macros
#define SLIM_LOG_LEVEL_LOG 0
#define SLIM_LOG_LEVEL_INFO 1
#define SLIM_LOG_LEVEL_WARNING 2
#define SLIM_LOG_LEVEL_TIME 3

namespace slim {
extern unsigned int DEBUG_LEVEL;
}
#ifdef DEBUG
#define SLIM_DEBUG_WITH_LEVEL(level, x)                                        \
    do {                                                                       \
        if (level >= slim::DEBUG_LEVEL) {                                      \
            x;                                                                 \
        }                                                                      \
    } while (false)

#define SLIM_DEBUG(x) SLIM_DEBUG_WITH_LEVEL(0, x)
#else
#define SLIM_DEBUG(x)                                                          \
    do {                                                                       \
    } while (false)
#define SLIM_DEBUG_WITH_LEVEL(level, x)                                        \
    do {                                                                       \
    } while (false)
#endif

#define ALWAYS_SLIM_DEBUG(x)                                                   \
    do {                                                                       \
        x;                                                                     \
    } while (false)
