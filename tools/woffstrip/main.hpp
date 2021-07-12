#pragma once
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>


#define STDERR(fmt, ...) fprintf(stderr, fmt "\n", ##__VA_ARGS__)
#define LOG(fmt, ...) STDERR(fmt, ##__VA_ARGS__)
#define LOG_ERRNO(fmt, ...) LOG(fmt " - %d: %s", ##__VA_ARGS__, errno, strerror(errno))
#define LOG_INFO(fmt, ...) if (config.verbose) LOG(fmt, ##__VA_ARGS__)
#define LOG_DUMP(fmt, ...) if (config.dump) printf(fmt "\n", ##__VA_ARGS__)

#define CONCAT_(a, b) a ## b
#define CONCAT(a, b) CONCAT_(a, b)

#define MAX(a,b) (((a)>(b))?(a):(b))
#define PAD4(l) (((l + 3) / 4) * 4)
#define PADMEMB uint8_t CONCAT(padmemb_, __LINE__)

extern struct config_s {
	bool verbose;
	bool dump;
} config;
