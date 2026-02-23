#ifndef LZ4_H
#define LZ4_H

#include <stddef.h>

int LZ4_decompress_safe(const char* src, char* dst, int compressedSize, int dstCapacity);

#endif