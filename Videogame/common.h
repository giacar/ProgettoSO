#ifndef COMMON_H
#define COMMON_H

// macro to simplify error handling
#define GENERIC_ERROR_HELPER(cond, errCode, msg) do {               \
        if (cond) {                                                 \
            fprintf(stderr, "%s: %s\n", msg, strerror(errCode));    \
            exit(EXIT_FAILURE);                                     \
        }                                                           \
    } while(0)

#define ERROR_HELPER(ret, msg)      GENERIC_ERROR_HELPER((ret < 0), errno, msg)

/* Configuration parameters */
#define SERVER_PORT_TCP    2015
#define SERVER_PORT_UDP    2016

#endif
