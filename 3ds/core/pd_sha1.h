// SHA-1, for telling ROM hacks apart by their whole file (the app's
// Poller.detect).
#ifndef PD_SHA1_H
#define PD_SHA1_H

#include <stddef.h>
#include <stdint.h>

// The hash of data[0, len) as 40 lowercase hex digits, with holeLen bytes from
// holeStart read as zeros (the cart's GPIO registers: the app's masked hash).
void pd_sha1_hex(const uint8_t* data, size_t len, size_t holeStart, size_t holeLen, char out[41]);

#endif
