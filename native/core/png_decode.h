// Minimal PNG decoder for the HUD/menu image assets in data/images.zip.
//
// NOT a port of any web/*.js file -- same reasoning as gif_decode.h: the
// browser reads these through its own built-in decoder, nothing to
// translate. New code implementing the PNG spec directly, scoped to
// exactly what the 13 real PNG files in data/images.zip actually use
// (verified by inspecting every one of them): 8-bit depth, colour type 6
// (truecolour with alpha, i.e. already-RGBA8 pixels, no palette lookup or
// bit-unpacking needed), non-interlaced, one IHDR + one-or-more IDAT
// chunks. NOT implemented because nothing in data/images.zip needs it:
// any other bit depth/colour type, interlacing (Adam7), ancillary chunks
// beyond what's needed to find IHDR/IDAT/IEND.
#ifndef NFM_PNG_DECODE_H
#define NFM_PNG_DECODE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int32_t width, height;
  uint8_t *rgba; // width*height*4 bytes, RGBA8888, top-to-bottom row order, malloc'd
} PngImage;

/** Decodes `data` (the raw bytes of a .png file, `len` long) into `out`.
 * Returns false (leaving `out` zeroed) on any parse error or unsupported
 * feature (see this header's own scope note). */
bool png_decode(const uint8_t *data, size_t len, PngImage *out);

void png_free(PngImage *img);

#ifdef __cplusplus
}
#endif

#endif
