// Minimal GIF87a/GIF89a decoder for the HUD/menu image assets in
// data/images.zip.
//
// NOT a port of any web/*.js file -- the browser reads these through the
// browser's OWN built-in GIF decoder (an `<img>`/ImageBitmap/OffscreenCanvas
// pipeline, see web/vfs.js's `readZip`+image loading and web/images.js's
// own top comment), which has no native equivalent to translate. This is
// new code implementing the GIF87a/89a spec directly, scoped to exactly
// what the 139 real asset files in data/images.zip actually use (verified
// by decoding every one of them, see this module's own test): single
// frame, standard 4-pass interlacing on a handful of files (dome.gif,
// mycl.gif, myfr.gif, roomp.gif -- found the hard way: an image-info
// dict check that looked authoritative said none of the 122 real GIFs
// were interlaced, but that turned out to be a red herring, not ground
// truth -- reading the actual per-image packed byte off every file
// found these 4), optional single transparent colour index, global OR
// local colour table, standard variable-width LZW compression. NOT
// implemented because nothing in data/images.zip needs it: animation
// (multiple image blocks), plain-text extensions.
#ifndef NFM_GIF_DECODE_H
#define NFM_GIF_DECODE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int32_t width, height;
  uint8_t *rgba; // width*height*4 bytes, RGBA8888, top-to-bottom row order, malloc'd
} GifImage;

/** Decodes `data` (the raw bytes of a .gif file, `len` long) into `out`.
 * Returns false (leaving `out` zeroed) on any parse error -- malformed
 * input, an unsupported feature (interlaced/multi-frame/no colour table),
 * or a LZW stream that doesn't terminate cleanly. Never partially fills
 * `out` on failure. */
bool gif_decode(const uint8_t *data, size_t len, GifImage *out);

void gif_free(GifImage *img);

#ifdef __cplusplus
}
#endif

#endif
