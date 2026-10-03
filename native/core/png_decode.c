// See png_decode.h for scope.
#include "png_decode.h"
#include <zlib.h>
#include <stdlib.h>
#include <string.h>

static uint32_t read_u32be(const uint8_t *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

// Zlib-wrapped inflate (PNG's IDAT stream, unlike the zip format's raw
// deflate -- see core/vfs.c's own inflate_raw for that variant). Output
// size is known exactly upfront (from IHDR: (width*4+1) * height), so
// this decompresses straight into a fixed buffer rather than growing one.
static bool zlib_inflate(const uint8_t *src, uint32_t src_len, uint8_t *dst, uint32_t dst_len) {
  z_stream strm;
  memset(&strm, 0, sizeof(strm));
  if (inflateInit(&strm) != Z_OK) return false;
  strm.next_in = (Bytef *)src;
  strm.avail_in = src_len;
  strm.next_out = dst;
  strm.avail_out = dst_len;
  int ret = inflate(&strm, Z_FINISH);
  bool ok = (ret == Z_STREAM_END) && strm.avail_out == 0;
  inflateEnd(&strm);
  return ok;
}

static uint8_t paeth_predictor(int32_t a, int32_t b, int32_t c) {
  int32_t p = a + b - c;
  int32_t pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
  if (pa <= pb && pa <= pc) return (uint8_t)a;
  if (pb <= pc) return (uint8_t)b;
  return (uint8_t)c;
}

// Reconstructs each scanline in place per the PNG filter spec (§9.2-9.3).
// `bpp` is bytes-per-pixel (4 for our RGBA8-only scope, used as the
// filter's "distance to the pixel to the left").
static void unfilter(uint8_t *raw, int32_t width, int32_t height, int32_t bpp) {
  int32_t stride = width * bpp;
  uint8_t *prev_row = NULL;
  for (int32_t y = 0; y < height; y++) {
    uint8_t *row = raw + (size_t)y * (stride + 1);
    uint8_t filter_type = row[0];
    uint8_t *pixels = row + 1;
    for (int32_t x = 0; x < stride; x++) {
      int32_t a = x >= bpp ? pixels[x - bpp] : 0;
      int32_t b = prev_row ? prev_row[x] : 0;
      int32_t c = (prev_row && x >= bpp) ? prev_row[x - bpp] : 0;
      switch (filter_type) {
        case 0: break; // None
        case 1: pixels[x] = (uint8_t)(pixels[x] + a); break;             // Sub
        case 2: pixels[x] = (uint8_t)(pixels[x] + b); break;             // Up
        case 3: pixels[x] = (uint8_t)(pixels[x] + (a + b) / 2); break;   // Average
        case 4: pixels[x] = (uint8_t)(pixels[x] + paeth_predictor(a, b, c)); break; // Paeth
        default: break; // unknown filter type -- leave bytes as-is, caller's pixel data will just be wrong, not crash
      }
    }
    prev_row = pixels;
  }
}

bool png_decode(const uint8_t *data, size_t len, PngImage *out) {
  memset(out, 0, sizeof(*out));
  static const uint8_t SIGNATURE[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
  if (len < 8 || memcmp(data, SIGNATURE, 8) != 0) return false;

  int32_t width = 0, height = 0;
  bool have_ihdr = false;
  uint8_t *idat = NULL;
  size_t idat_len = 0, idat_cap = 0;

  size_t pos = 8;
  bool ok = true;
  while (pos + 8 <= len) {
    uint32_t chunk_len = read_u32be(data + pos);
    const uint8_t *chunk_type = data + pos + 4;
    const uint8_t *chunk_data = data + pos + 8;
    if (pos + 8 + (size_t)chunk_len + 4 > len) { ok = false; break; }

    if (memcmp(chunk_type, "IHDR", 4) == 0) {
      if (chunk_len < 13) { ok = false; break; }
      width = (int32_t)read_u32be(chunk_data);
      height = (int32_t)read_u32be(chunk_data + 4);
      uint8_t bit_depth = chunk_data[8];
      uint8_t color_type = chunk_data[9];
      uint8_t interlace = chunk_data[12];
      if (bit_depth != 8 || color_type != 6 || interlace != 0 || width <= 0 || height <= 0) {
        ok = false; // unsupported variant -- see this file's header scope note
        break;
      }
      have_ihdr = true;
    } else if (memcmp(chunk_type, "IDAT", 4) == 0) {
      if (idat_len + chunk_len > idat_cap) {
        idat_cap = (idat_len + chunk_len) * 2 + 64;
        idat = realloc(idat, idat_cap);
      }
      memcpy(idat + idat_len, chunk_data, chunk_len);
      idat_len += chunk_len;
    } else if (memcmp(chunk_type, "IEND", 4) == 0) {
      break;
    }
    // Every other chunk type (tEXt, pHYs, gAMA, ...) is skipped -- not
    // needed to reconstruct pixels.

    pos += 8 + (size_t)chunk_len + 4; // length + type + data + CRC
  }

  if (!ok || !have_ihdr || !idat) { free(idat); return false; }

  int32_t bpp = 4; // colour type 6, bit depth 8 -- see this file's header scope note
  int32_t stride = width * bpp;
  size_t raw_len = (size_t)(stride + 1) * (size_t)height;
  uint8_t *raw = malloc(raw_len);
  bool inflated = zlib_inflate(idat, (uint32_t)idat_len, raw, (uint32_t)raw_len);
  free(idat);
  if (!inflated) { free(raw); return false; }

  unfilter(raw, width, height, bpp);

  uint8_t *rgba = malloc((size_t)width * (size_t)height * 4);
  for (int32_t y = 0; y < height; y++) {
    memcpy(rgba + (size_t)y * stride, raw + (size_t)y * (stride + 1) + 1, (size_t)stride);
  }
  free(raw);

  out->width = width;
  out->height = height;
  out->rgba = rgba;
  return true;
}

void png_free(PngImage *img) {
  free(img->rgba);
  img->rgba = NULL;
  img->width = img->height = 0;
}
