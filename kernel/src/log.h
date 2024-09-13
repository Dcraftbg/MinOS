#pragma once
#include "logger.h"
#include "kernel.h"
#define klog(level, ...) logger_log(kernel.logger, level, __VA_ARGS__)
#define ktrace(...) klog(LOG_TRACE, __VA_ARGS__)
#define kdebug(...) klog(LOG_DEBUG, __VA_ARGS__)
#define kinfo(...)  klog(LOG_INFO , __VA_ARGS__)
#define kwarn(...)  klog(LOG_WARN , __VA_ARGS__)
#define kerror(...) klog(LOG_ERROR, __VA_ARGS__)
#define kfatal(...) klog(LOG_FATAL, __VA_ARGS__)

