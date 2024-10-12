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

#define klog_va(level, fmt, va) logger_log_va(kernel.logger, level, fmt, va)
#define ktrace_va(fmt, va) klog(LOG_TRACE, fmt, va)
#define kdebug_va(fmt, va) klog(LOG_DEBUG, fmt, va)
#define kinfo_va(fmt, va)  klog(LOG_INFO , fmt, va)
#define kwarn_va(fmt, va)  klog(LOG_WARN , fmt, va)
#define kerror_va(fmt, va) klog(LOG_ERROR, fmt, va)
#define kfatal_va(fmt, va) klog(LOG_FATAL, fmt, va)

