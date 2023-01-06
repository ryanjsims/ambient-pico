#pragma once

#define LOG_LEVEL_TRACE    0
#define LOG_LEVEL_DEBUG    1
#define LOG_LEVEL_INFO     2
#define LOG_LEVEL_WARN     3
#define LOG_LEVEL_CRITICAL 4
#define LOG_LEVEL_ERROR    5

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_INFO
#endif

#if LOG_LEVEL<=0
#define trace(message, ...) printf("[trace] " message, __VA_ARGS__)
#define trace1(message) printf("[trace] " message)
#else
#define trace(message, ...) (void)0
#define trace1(message) (void)0
#endif

#if LOG_LEVEL<=1
#define debug(message, ...) printf("[debug] " message, __VA_ARGS__)
#define debug1(message) printf("[debug] " message)
#else
#define debug(message, ...) (void)0
#define debug1(message) (void)0
#endif

#if LOG_LEVEL<=2
#define info(message, ...) printf("[info] " message, __VA_ARGS__)
#define info1(message) printf("[info] " message)
#else
#define info(message, ...) (void)0
#define info1(message) (void)0
#endif

#if LOG_LEVEL<=3
#define warn(message, ...) printf("[warn] " message, __VA_ARGS__)
#define warn1(message) printf("[warn] " message)
#else
#define warn(message, ...) (void)0
#define warn1(message) (void)0
#endif

#if LOG_LEVEL<=4
#define critical(message, ...) printf("[critical] " message, __VA_ARGS__)
#define critical1(message) printf("[critical] " message)
#else
#define critical(message, ...) (void)0
#define critical1(message) (void)0
#endif

#if LOG_LEVEL<=5
#define error(message, ...) printf("[error] " message, __VA_ARGS__)
#define error1(message) printf("[error] " message)
#else
#define error(message, ...) (void)0
#define error1(message) (void)0
#endif