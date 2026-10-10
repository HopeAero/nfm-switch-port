#include "png_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

static void put32(uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)(v >> 24);
  p[1] = (uint8_t)(v >> 16);
  p[2] = (uint8_t)(v >> 8);
  p[3] = (uint8_t)v;
}

// One chunk at `out`: length, type, data, CRC over type and data.
static size_t chunk(uint8_t *out, const char type[4], const uint8_t *data, uint32_t len) {
  put32(out, len);
  memcpy(out + 4, type, 4);
  if (len) memcpy(out + 8, data, len);
  put32(out + 8 + len, (uint32_t)crc32(0, out + 4, len + 4));
  return 12 + len;
}

uint8_t *png_encode_rgb(const uint8_t *rgb, int32_t w, int32_t h, size_t *len) {
  if (w <= 0 || h <= 0) return NULL;
  // Each row behind a filter byte 0 (none).
  const size_t row = (size_t)w * 3, raw_len = (row + 1) * (size_t)h;
  uint8_t *raw = malloc(raw_len);
  if (!raw) return NULL;
  for (int32_t y = 0; y < h; y++) {
    raw[(row + 1) * (size_t)y] = 0;
    memcpy(raw + (row + 1) * (size_t)y + 1, rgb + row * (size_t)y, row);
  }
  uLongf zlen = compressBound((uLong)raw_len);
  uint8_t *png = malloc(8 + 25 + 12 + zlen + 12);
  if (!png) {
    free(raw);
    return NULL;
  }
  static const uint8_t kSig[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
  memcpy(png, kSig, 8);
  size_t n = 8;
  uint8_t ihdr[13];
  put32(ihdr, (uint32_t)w);
  put32(ihdr + 4, (uint32_t)h);
  ihdr[8] = 8;   // bit depth
  ihdr[9] = 2;   // colour type RGB
  ihdr[10] = ihdr[11] = ihdr[12] = 0;
  n += chunk(png + n, "IHDR", ihdr, 13);
  // The IDAT's data goes straight into place, behind its length and type.
  if (compress2(png + n + 8, &zlen, raw, (uLong)raw_len, 6) != Z_OK) {
    free(raw);
    free(png);
    return NULL;
  }
  free(raw);
  put32(png + n, (uint32_t)zlen);
  memcpy(png + n + 4, "IDAT", 4);
  put32(png + n + 8 + zlen, (uint32_t)crc32(0, png + n + 4, (uInt)zlen + 4));
  n += 12 + zlen;
  n += chunk(png + n, "IEND", NULL, 0);
  *len = n;
  return png;
}

bool png_write_rgb(const char *path, const uint8_t *rgb, int32_t w, int32_t h) {
  size_t len = 0;
  uint8_t *png = png_encode_rgb(rgb, w, h, &len);
  if (!png) return false;
  FILE *f = fopen(path, "wb");
  bool ok = f && fwrite(png, 1, len, f) == len;
  if (f) ok = fclose(f) == 0 && ok;
  free(png);
  return ok;
}
