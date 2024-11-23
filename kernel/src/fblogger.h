#pragma once
#include "logger.h"
#include "fbwriter.h"
#include "bootutils.h"
extern FbTextWriter fbt0;
extern struct Logger fb_logger;
intptr_t init_fb_logger();
