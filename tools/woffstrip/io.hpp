#pragma once
#include "main.hpp"


bool file_read(const char*, char*&, size_t&);
bool file_write(const char*, const char*, size_t);
bool docompress(const char* src, size_t, char*& dst, size_t*);
bool decompress(const char* src, size_t, char*& dst, size_t);
