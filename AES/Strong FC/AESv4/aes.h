/*
*   Byte-oriented AES-256 implementation.
*   All lookup tables replaced with 'on the fly' calculations.
*/

#include <inttypes.h>

struct aes256_context{
  uint8_t key[32];
  uint8_t enckey[32];
  uint8_t deckey[32];
};

////////////////////////////////////////////////////////////////////////////////
// Test harness interface code.

struct bench_args_t {
  struct aes256_context ctx;
  uint8_t k[32];
  uint8_t buf[16];
};

