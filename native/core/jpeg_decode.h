// Minimal baseline JPEG decoder for the HUD/menu background image assets
// in data/images.zip.
//
// NOT a port of any web/*.js file -- same reasoning as gif_decode.h/
// png_decode.h: the browser reads these through its own built-in
// decoder, nothing to translate. New code implementing the JPEG spec
// (ITU-T T.81) directly, scoped to exactly what the 4 real JPG files in
// data/images.zip actually use (verified by inspecting every one of
// them): baseline DCT (SOF0, not progressive/SOF2 and not the rare
// lossless/arithmetic-coding variants), 8-bit precision, 3 components
// (standard YCbCr), Huffman entropy coding, 4:2:0 OR 4:4:4 chroma
// subsampling, a single scan (no restart markers, no multiple scans).
// NOT implemented because nothing in data/images.zip needs it:
// progressive/arithmetic-coded JPEG, 12-bit precision, restart markers,
// grayscale/CMYK, subsampling ratios other than the two above.
#ifndef NFM_JPEG_DECODE_H
#define NFM_JPEG_DECODE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int32_t width, height;
  uint8_t *rgba; // width*height*4 bytes, RGBA8888 (alpha always 255 -- JPEG has no alpha channel), malloc'd
} JpegImage;

/** Decodes `data` (the raw bytes of a .jpg file, `len` long) into `out`.
 * Returns false (leaving `out` zeroed) on any parse error or unsupported
 * feature (see this header's own scope note). */
bool jpeg_decode(const uint8_t *data, size_t len, JpegImage *out);

void jpeg_free(JpegImage *img);

#ifdef __cplusplus
}
#endif

#endif
