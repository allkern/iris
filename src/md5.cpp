/*
 * Derived from the RSA Data Security, Inc. MD5 Message-Digest Algorithm
 * and modified slightly to be functionally identical but condensed into control structures.
 */

#include "md5.hpp"

namespace iris::md5 {

static constexpr uint32_t INIT_STATE[4] = { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476 };

static const uint32_t S[] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
                             5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
                             4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
                             6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};

static const uint32_t K[] = {0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
                             0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
                             0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
                             0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
                             0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
                             0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
                             0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
                             0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
                             0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
                             0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
                             0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
                             0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
                             0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
                             0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
                             0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
                             0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};

// Pads the size (in bits) of the input to 448 mod 512
static const uint8_t PADDING[] = {0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static uint32_t f(uint32_t x, uint32_t y, uint32_t z) { return (x & y) | (~x & z); }
static uint32_t g(uint32_t x, uint32_t y, uint32_t z) { return (x & z) | (y & ~z); }
static uint32_t h(uint32_t x, uint32_t y, uint32_t z) { return x ^ y ^ z; }
static uint32_t i(uint32_t x, uint32_t y, uint32_t z) { return y ^ (x | ~z); }

static uint32_t rotate_left(uint32_t x, uint32_t n) {
    return (x << n) | (x >> (32 - n));
}

static void step(uint32_t* state, const uint32_t* words) {
    uint32_t aa = state[0];
    uint32_t bb = state[1];
    uint32_t cc = state[2];
    uint32_t dd = state[3];

    uint32_t e;
    unsigned int j;

    for (unsigned int n = 0; n < 64; ++n) {
        switch (n / 16) {
            case 0:
                e = f(bb, cc, dd);
                j = n;
                break;
            case 1:
                e = g(bb, cc, dd);
                j = ((n * 5) + 1) % 16;
                break;
            case 2:
                e = h(bb, cc, dd);
                j = ((n * 3) + 5) % 16;
                break;
            default:
                e = i(bb, cc, dd);
                j = (n * 7) % 16;
                break;
        }

        uint32_t temp = dd;
        dd = cc;
        cc = bb;
        bb = bb + rotate_left(aa + e + K[n] + words[j], S[n]);
        aa = temp;
    }

    state[0] += aa;
    state[1] += bb;
    state[2] += cc;
    state[3] += dd;
}

void init(Md5* md5) {
    md5->size = 0;

    for (unsigned int n = 0; n < 4; ++n)
        md5->state[n] = INIT_STATE[n];
}

void update(Md5* md5, const uint8_t* data, size_t len) {
    uint32_t words[16];

    unsigned int offset = md5->size % 64;

    md5->size += (uint64_t)len;

    for (size_t n = 0; n < len; ++n) {
        md5->block[offset++] = data[n];

        if (offset % 64 == 0) {
            for (unsigned int j = 0; j < 16; ++j) {
                words[j] = (uint32_t)(md5->block[(j * 4) + 3]) << 24 |
                           (uint32_t)(md5->block[(j * 4) + 2]) << 16 |
                           (uint32_t)(md5->block[(j * 4) + 1]) <<  8 |
                           (uint32_t)(md5->block[(j * 4)]);
            }

            step(md5->state, words);

            offset = 0;
        }
    }
}

void finalize(Md5* md5) {
    uint32_t words[16];

    unsigned int offset = md5->size % 64;
    unsigned int padding_length = offset < 56 ? 56 - offset : (56 + 64) - offset;

    update(md5, PADDING, padding_length);

    // update() counted the padding, but the length appended below is of the real input
    md5->size -= (uint64_t)padding_length;

    for (unsigned int j = 0; j < 14; ++j) {
        words[j] = (uint32_t)(md5->block[(j * 4) + 3]) << 24 |
                   (uint32_t)(md5->block[(j * 4) + 2]) << 16 |
                   (uint32_t)(md5->block[(j * 4) + 1]) <<  8 |
                   (uint32_t)(md5->block[(j * 4)]);
    }

    words[14] = (uint32_t)(md5->size * 8);
    words[15] = (uint32_t)((md5->size * 8) >> 32);

    step(md5->state, words);

    for (unsigned int n = 0; n < 4; ++n) {
        md5->digest[(n * 4) + 0] = (uint8_t)((md5->state[n] & 0x000000FF));
        md5->digest[(n * 4) + 1] = (uint8_t)((md5->state[n] & 0x0000FF00) >>  8);
        md5->digest[(n * 4) + 2] = (uint8_t)((md5->state[n] & 0x00FF0000) >> 16);
        md5->digest[(n * 4) + 3] = (uint8_t)((md5->state[n] & 0xFF000000) >> 24);
    }
}

}
