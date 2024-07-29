#pragma once
#include <stdint.h>
#include <stddef.h> // NULL
#include "utils.h"
enum {
   NOT_ENOUGH_MEM=1,
   BAD_INODE,
   INVALID_PARAM,
   FILE_CORRUPTION,
   LIMITS,
   NOT_FOUND,
   UNSUPPORTED,
   ALREADY_EXISTS,
   INODE_IS_DIRECTORY,
   INVALID_OFFSET,
   BAD_DEVICE,
   PERMISION_DENIED,
   PREMATURE_EOF,
   INVALID_MAGIC,
   NO_ENTRYPOINT,
   INVALID_TYPE,
   INVALID_HANDLE,

   STATUS_COUNT
};
const char* status_str(intptr_t status);
