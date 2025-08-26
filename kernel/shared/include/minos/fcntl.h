#pragma once
#define O_CREAT      (1 << 0)
#define O_DIRECTORY  (1 << 1)
#define O_TRUNC      (1 << 2)
#define O_NONBLOCK   (1 << 3)
#define O_RDONLY     (1 << 4)
#define O_WRONLY     (1 << 5) 
#define O_RDWR       (O_RDONLY | O_WRONLY)
#define O_NDELAY     O_NONBLOCK
#define O_BINARY     0
