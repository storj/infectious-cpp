// Copyright (C) 2022 Storj Labs, Inc.
// See LICENSE for copying information.

#ifndef ADDMUL_H
#define ADDMUL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>

void addmul(uint8_t* z, const uint8_t* x, int z_len, uint8_t y);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ADDMUL_H
