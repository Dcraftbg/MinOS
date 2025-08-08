#pragma once
// prot
#define PROT_EXEC (1 << 0)
#define PROT_READ (1 << 1)
#define PROT_WRITE (1 << 2)
#define PROT_NONE 0
// flags
#define MAP_PRIVATE   (1 << 0)
// @TODO:
#define MAP_SHARED    (1 << 1)
#define MAP_ANONYMOUS (1 << 2)
// @TODO:
#define MAP_FIXED     (1 << 3)
#define MAP_FIXED_NOREPLACE (1 << 4)

