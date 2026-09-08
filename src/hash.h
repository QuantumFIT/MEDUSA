/**
 * @file hash.h
 * @brief Hash functions
 */

#include <stdint.h>
#include <gmp.h>

#ifndef HASH_H
#define HASH_H

/**
 * Hash function for GMP integers (combines all GMP limbs)
 */
uint64_t combine_mpz_t(mpz_t x);

/**
 * Hash function for 64bit integers
 * 
 * Taken from: https://stackoverflow.com/a/12996028, author: Thomas Mueller (https://stackoverflow.com/users/382763/thomas-mueller)
 */
uint64_t my_int_hash(uint64_t x);

/**
 * Hash function for strings
 * 
 * Taken from: http://www.cse.yorku.ca/~oz/hash.html, variant sdbm
 */
uint64_t my_str_hash(const char *str);

/**
 * Hash value and combine it with an already existing hash
 * 
 * Taken from: http://www.boost.org/doc/libs/1_64_0/boost/functional/hash/hash.hpp
 */
#define MY_HASH_COMB_GMP(val, data) ( (val) ^ (my_int_hash(combine_mpz_t(data)) + 0x9e3779b9 + ((val)<<6) + ((val)>>2)) )

/**
 * Hash value of datatype based on backend and combine it with an already existing hash
 * 
 * Taken from: http://www.boost.org/doc/libs/1_64_0/boost/functional/hash/hash.hpp
 */
#define MY_HASH_COMB_GENERIC(val, data) ( (val) ^ (my_int_hash(hash_comb_generic(data)) + 0x9e3779b9 + ((val)<<6) + ((val)>>2)) )

/**
 * Hash value and combine it with an already existing hash for symbolic representation (works with all ptrs universally)
 */
#define MY_HASH_COMB(val, data) ( (val) ^ (my_int_hash((uint64_t)data) + 0x9e3779b9 + ((val)<<6) + ((val)>>2)) )

#endif
/* end of "hash.h" */