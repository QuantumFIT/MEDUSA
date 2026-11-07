/**
 * @file error.h
 * @brief Custom error handling functions
 */

#include <stdlib.h>

#ifndef ERROR_H
#define ERROR_H

/**
 * Function that handles program exiting in case of error.
 * 
 * @param error Error message format.
*/
__attribute__((noreturn, format(printf, 1, 2)))
void error_exit(const char *error, ...);

/**
 * Custom malloc function including error handling.
 */
void* my_malloc(size_t size);

/**
 * Custom realloc function including error handling.
 */
void* my_realloc(void *p, size_t size);

#endif
/* end of "error.h" */