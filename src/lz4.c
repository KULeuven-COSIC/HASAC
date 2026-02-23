/* HASAC */
/* Sayon Duttagupta */

#include "lz4.h"
#include <stdint.h>

#define MIN_MATCH 4
#define ML_BITS  4
#define ML_MASK  ((1U<<ML_BITS)-1)
#define RUN_BITS (8-ML_BITS)
#define RUN_MASK ((1U<<RUN_BITS)-1)

int LZ4_decompress_safe(const char* source, char* dest, int compressedSize, int maxOutputSize)
{
    const uint8_t* ip = (const uint8_t*) source;
    const uint8_t* const iend = ip + compressedSize;
    uint8_t* op = (uint8_t*) dest;
    uint8_t* const oend = op + maxOutputSize;
    uint8_t* const outputStart = op;

    while (ip < iend) {
        unsigned token = *ip++;
        unsigned length = token >> ML_BITS;

        // Literal Length
        if (length == RUN_MASK) {
            unsigned s;
            do {
                if (ip >= iend) return -1;
                s = *ip++;
                length += s;
            } while (s == 255);
        }

        // Copy Literals
        if ((op + length) > oend || (ip + length) > iend) return -2;
        for (unsigned i = 0; i < length; i++) *op++ = *ip++;

        if (ip >= iend) break;

        // Match Copy
        unsigned offset = ip[0] | (ip[1] << 8);
        ip += 2;
        if (offset == 0) return -3;

        // Match Length
        length = token & ML_MASK;
        if (length == ML_MASK) {
            unsigned s;
            do {
                if (ip >= iend) return -4;
                s = *ip++;
                length += s;
            } while (s == 255);
        }
        length += MIN_MATCH;

        uint8_t* match = op - offset;
        if (match < outputStart) return -5;
        if (op + length > oend) return -6;

        for (unsigned i = 0; i < length; i++) *op++ = *match++;
    }
    return (int)(op - outputStart);
}