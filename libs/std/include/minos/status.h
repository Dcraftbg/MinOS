#pragma once
#include <stdint.h>
#include <stddef.h> // NULL
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
   SIZE_MISMATCH,
   WOULD_SEGFAULT,
   RESOURCE_BUSY,
   YOU_ARE_CHILD,
   INVALID_PATH,
   IS_NOT_DIRECTORY,
   BUFFER_TOO_SMALL,
   TIMEOUT_REACHED,
   ADDR_SOCKET_FAMILY_MISMATCH,
   UNSUPPORTED_DOMAIN,
   UNSUPPORTED_SOCKET_TYPE,
   WOULD_BLOCK,
   VIRTUAL_SPACE_OCCUPIED,
   RECURSIVE_ELF_INTERP,

   STATUS_COUNT
};
const char* status_str(intptr_t status);
