#ifndef LOGGER_H
#define LOGGER_H

#define DISABLE_PRINTF false

#if DISABLE_PRINTF
    #define printf(...)
#endif

#endif // LOGGER_H
