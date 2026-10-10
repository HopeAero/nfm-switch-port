// A PNG writer for the photo mode (game.c's photos, gallery.inc reads them
// back with png_decode): 8-bit RGB, one zlib stream, no filtering.
#ifndef NFM_PNG_WRITE_H
#define NFM_PNG_WRITE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Writes `w`x`h` RGB pixels (3 bytes each, top row first) to `path`. */
bool png_write_rgb(const char *path, const uint8_t *rgb, int32_t w, int32_t h);

/** The same PNG in memory: malloc'd, its size in *len; NULL on failure. */
uint8_t *png_encode_rgb(const uint8_t *rgb, int32_t w, int32_t h, size_t *len);

#ifdef __cplusplus
}
#endif

#endif
