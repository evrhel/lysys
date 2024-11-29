#ifndef _LS_RAND_H_
#define _LS_RAND_H_

#include "ls_defs.h"

#if LS_FEATURE_RANDOM

//! \brief Generates a cryptographically secure sequence of random bytes.
//! 
//! The random bytes are generated using the system's preferred random number
//! generator.
//! 
//! \param buf Pointer to the buffer where the random bytes will be stored.
//! \param len Number of random bytes to generate.
//! 
//! \return 0 on success, -1 on failure.
int ls_rand_bytes(void *buf, size_t len);

//! \brief Generates a random 64-bit unsigned integer.
//! 
//! \return Random 64-bit unsigned integer, -1 on failure.
uint64_t ls_rand_uint64(void);

//! \brief Generates a random integer in the range [min, max].
//! 
//! \param min Minimum value of the range. If min is greater than max, the range
//! is clamped to [max, max].
//! \param max Maximum value of the range.
//! 
//! \return Random integer in the range [min, max], -1 on failure.
int ls_rand_int(int min, int max);

//! \brief Generates a random double in the range [0, 1).
//! 
//! \return Random double in the range [0, 1), -1 on failure.
double ls_rand_double(void);

//! \brief Generates a random float in the range [0, 1).
//!		
//! \return Random float in the range [0, 1), -1 on failure.
float ls_rand_float(void);

#endif // LS_FEATURE_RANDOM

#endif // _LS_RAND_H_
