#include "pd_sha1.h"

#include <string.h>

// FIPS 180-4's SHA-1, one 64-byte block at a time.

static uint32_t rol(uint32_t v, int n) {
    return v << n | v >> (32 - n);
}

static void block(uint32_t h[5], const uint8_t* p) {
    uint32_t w[80];
    for (int i = 0; i < 16; i++) {
        w[i] = (uint32_t) p[i * 4] << 24 | (uint32_t) p[i * 4 + 1] << 16 | (uint32_t) p[i * 4 + 2] << 8 | p[i * 4 + 3];
    }
    for (int i = 16; i < 80; i++) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20) {
            f = (b & c) | (~b & d);
            k = 0x5A827999;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDC;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6;
        }
        uint32_t t = rol(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = rol(b, 30);
        b = a;
        a = t;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
}

void pd_sha1_hex(const uint8_t* data, size_t len, size_t holeStart, size_t holeLen, char out[41]) {
    uint32_t h[5] = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 };
    uint8_t buf[64];
    size_t off = 0;
    // Whole blocks; the one holding the masked range goes through buf.
    while (len - off >= 64) {
        if (holeLen && off < holeStart + holeLen && holeStart < off + 64) {
            memcpy(buf, data + off, 64);
            for (size_t i = holeStart; i < holeStart + holeLen; i++) {
                if (i >= off && i < off + 64) buf[i - off] = 0;
            }
            block(h, buf);
        } else {
            block(h, data + off);
        }
        off += 64;
    }
    size_t rest = len - off;
    memset(buf, 0, sizeof(buf));
    memcpy(buf, data + off, rest);
    for (size_t i = holeStart; holeLen && i < holeStart + holeLen; i++) {
        if (i >= off && i < len) buf[i - off] = 0;
    }
    buf[rest] = 0x80;
    if (rest >= 56) {
        block(h, buf);
        memset(buf, 0, sizeof(buf));
    }
    uint64_t bits = (uint64_t) len * 8;
    for (int i = 0; i < 8; i++) buf[63 - i] = (uint8_t) (bits >> (8 * i));
    block(h, buf);
    static const char HEX[] = "0123456789abcdef";
    for (int i = 0; i < 20; i++) {
        uint8_t v = (uint8_t) (h[i / 4] >> (24 - 8 * (i % 4)));
        out[i * 2] = HEX[v >> 4];
        out[i * 2 + 1] = HEX[v & 15];
    }
    out[40] = 0;
}
