#pragma once
#include <stdint.h>
#define PRIu8  PRIu32
#define PRIu16 PRIu32
#define PRIu32 "u"
#define PRIu64 "lu"

#define PRIi8  PRIi32
#define PRIi16 PRIi32
#define PRIi32 "d"
#define PRIi64 "ld"

#define PRId8  PRIi8 
#define PRId16 PRIi16
#define PRId32 PRIi32
#define PRId64 PRIi64

#define PRIx8  "x"
#define PRIx16 "x"
#define PRIx32 "x"
#define PRIx64 "x"

#define PRIX8  "X"
#define PRIX16 "X"
#define PRIX32 "X"
#define PRIX64 "X"
