#pragma once
#include "../../config.h"
#ifdef PROGRESS_BAR
#include <stddef.h>
#include "fbwriter.h"
#define TOTAL_STEPS 24 
void update_bar(size_t at, const char* msg);
extern size_t step;
#else
#define update_bar(at, msg)
#endif
