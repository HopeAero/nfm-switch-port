// See jpeg_decode.h for scope. Implements ITU-T T.81's baseline
// sequential DCT process (Huffman coding, Annexes A/C/F) directly --
// this is a byte-for-byte-with-the-spec implementation, not adapted
// from anywhere, so comments here cite spec section numbers rather than
// "matches the JS at line N" the way this port's actual web/*.js
// translations do.
#include "jpeg_decode.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

// Zigzag-to-natural-order index map (spec Figure A.6).
static const int32_t ZIGZAG[64] = {
    0, 1, 8, 16, 9, 2, 3, 10,
    17, 24, 32, 25, 18, 11, 4, 5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13, 6, 7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63,
};

typedef struct {
  uint16_t values[64]; // in ZIGZAG order, as stored in the file (spec B.2.4.1)
  bool present;
} QuantTable;

typedef struct {
  // Canonical Huffman decode tables, built per spec Annex C.
  int32_t mincode[17], maxcode[17], valptr[17];
  uint8_t huffval[256];
  bool present;
} HuffTable;

typedef struct {
  int32_t id;
  int32_t h_samp, v_samp;
  int32_t quant_id;
  int32_t dc_huff_id, ac_huff_id;
  // Decoded plane, sized to this component's own MCU-aligned dimensions
  // (mcus_across * h_samp * 8) x (mcus_down * v_samp * 8) -- smaller
  // than the luma plane for a subsampled chroma component.
  uint8_t *plane;
  int32_t plane_w, plane_h;
} Component;

typedef struct {
  const uint8_t *data;
  size_t len;
  size_t byte_pos;
  int32_t bit_buf;
  int32_t bit_count;
} BitReader;

// Reads one bit from the entropy-coded segment, transparently unstuffing
// 0xFF 0x00 -> 0xFF (spec F.1.2.3 / B.1.1.5). A 0xFF followed by
// anything else marks the end of the scan (EOI, or a marker this
// decoder doesn't support, e.g. a restart marker -- see jpeg_decode.h's
// scope note) -- treated as "no more bits", which naturally stops
// decoding once every MCU expected from SOF0's width/height has already
// been read.
static int32_t read_bit(BitReader *br) {
  if (br->bit_count == 0) {
    if (br->byte_pos >= br->len) return -1;
    uint8_t b = br->data[br->byte_pos++];
    if (b == 0xFF) {
      if (br->byte_pos < br->len && br->data[br->byte_pos] == 0x00) {
        br->byte_pos++; // stuffed -- literal 0xFF
      } else {
        return -1; // a real marker -- end of entropy data
      }
    }
    br->bit_buf = b;
    br->bit_count = 8;
  }
  br->bit_count--;
  return (br->bit_buf >> br->bit_count) & 1;
}

static int32_t read_bits(BitReader *br, int32_t n) {
  int32_t v = 0;
  for (int32_t i = 0; i < n; i++) {
    int32_t bit = read_bit(br);
    if (bit < 0) return -1;
    v = (v << 1) | bit;
  }
  return v;
}

// JPEG's signed-magnitude coefficient encoding (spec F.1.2.1.1 / Table F.1).
static int32_t extend(int32_t v, int32_t size) {
  if (size == 0) return 0;
  if (v < (1 << (size - 1))) return v - (1 << size) + 1;
  return v;
}

static int32_t huff_decode(BitReader *br, const HuffTable *t) {
  int32_t code = 0;
  for (int32_t length = 1; length <= 16; length++) {
    int32_t bit = read_bit(br);
    if (bit < 0) return -1;
    code = (code << 1) | bit;
    if (t->maxcode[length] != -1 && code <= t->maxcode[length]) {
      int32_t j = t->valptr[length] + (code - t->mincode[length]);
      return t->huffval[j];
    }
  }
  return -1;
}

static void build_huff_table(HuffTable *t, const uint8_t *bits, const uint8_t *huffval, int32_t nvals) {
  memcpy(t->huffval, huffval, (size_t)nvals);
  int32_t code = 0, k = 0;
  for (int32_t l = 1; l <= 16; l++) {
    int32_t count = bits[l];
    if (count == 0) {
      t->maxcode[l] = -1;
    } else {
      t->valptr[l] = k;
      t->mincode[l] = code;
      code += count;
      k += count;
      t->maxcode[l] = code - 1;
    }
    code <<= 1;
  }
  t->present = true;
}

// Simple, direct (not fast-DCT-optimised) 2D 8x8 IDCT, per spec A.3.3 --
// runs once per block at asset-LOAD time on a handful of small images,
// not per-frame, so O(n^4) per block (4096 multiply-adds) is fine.
static void idct_8x8(const float *in_natural_order, uint8_t *out, int32_t out_stride) {
  static float cos_table[8][8]; // cos_table[x][u] = cos((2x+1)*u*PI/16)
  static bool cos_ready = false;
  if (!cos_ready) {
    for (int32_t x = 0; x < 8; x++) {
      for (int32_t u = 0; u < 8; u++) {
        cos_table[x][u] = cosf((2.0f * x + 1.0f) * u * (float)M_PI / 16.0f);
      }
    }
    cos_ready = true;
  }
  const float c0 = 0.70710678118654752440f; // 1/sqrt(2)
  for (int32_t y = 0; y < 8; y++) {
    for (int32_t x = 0; x < 8; x++) {
      float sum = 0.0f;
      for (int32_t v = 0; v < 8; v++) {
        float cv = (v == 0) ? c0 : 1.0f;
        for (int32_t u = 0; u < 8; u++) {
          float cu = (u == 0) ? c0 : 1.0f;
          sum += cu * cv * in_natural_order[v * 8 + u] * cos_table[x][u] * cos_table[y][v];
        }
      }
      int32_t px = (int32_t)lroundf(sum / 4.0f) + 128;
      if (px < 0) px = 0;
      if (px > 255) px = 255;
      out[y * out_stride + x] = (uint8_t)px;
    }
  }
}

static bool decode_block(BitReader *br, const HuffTable *dc_table, const HuffTable *ac_table,
                          const QuantTable *quant, int32_t *dc_pred, uint8_t *out_plane,
                          int32_t plane_stride, int32_t block_x, int32_t block_y) {
  float coeff[64]; // natural order, dequantized
  memset(coeff, 0, sizeof(coeff));

  int32_t dc_size = huff_decode(br, dc_table);
  if (dc_size < 0 || dc_size > 11) return false;
  int32_t dc_bits = dc_size > 0 ? read_bits(br, dc_size) : 0;
  if (dc_size > 0 && dc_bits < 0) return false;
  int32_t dc_diff = extend(dc_bits, dc_size);
  *dc_pred += dc_diff;
  coeff[0] = (float)(*dc_pred) * (float)quant->values[0];

  int32_t k = 1;
  while (k < 64) {
    int32_t rs = huff_decode(br, ac_table);
    if (rs < 0) return false;
    int32_t run = rs >> 4;
    int32_t size = rs & 0x0F;
    if (size == 0) {
      if (run == 15) { k += 16; continue; } // ZRL: 16 zero coefficients
      break; // EOB: rest of block is zero
    }
    k += run;
    if (k >= 64) return false;
    int32_t bits = read_bits(br, size);
    if (bits < 0) return false;
    int32_t val = extend(bits, size);
    int32_t natural_idx = ZIGZAG[k];
    coeff[natural_idx] = (float)val * (float)quant->values[k];
    k++;
  }

  uint8_t block_pixels[64];
  idct_8x8(coeff, block_pixels, 8);
  for (int32_t y = 0; y < 8; y++) {
    memcpy(out_plane + (size_t)(block_y + y) * plane_stride + block_x, block_pixels + y * 8, 8);
  }
  return true;
}

static uint8_t clamp_u8(int32_t v) { return (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v)); }

bool jpeg_decode(const uint8_t *data, size_t len, JpegImage *out) {
  memset(out, 0, sizeof(*out));
  if (len < 4 || data[0] != 0xFF || data[1] != 0xD8) return false; // SOI

  QuantTable quant[4];
  memset(quant, 0, sizeof(quant));
  HuffTable dc_huff[4], ac_huff[4];
  memset(dc_huff, 0, sizeof(dc_huff));
  memset(ac_huff, 0, sizeof(ac_huff));

  int32_t width = 0, height = 0, ncomp = 0;
  Component comps[4];
  memset(comps, 0, sizeof(comps));
  bool have_sof = false;

  size_t pos = 2;
  bool ok = true;
  while (pos + 4 <= len) {
    if (data[pos] != 0xFF) { ok = false; break; }
    uint8_t marker = data[pos + 1];
    if (marker == 0xD9) break; // EOI (shouldn't reach here before SOS handling, but harmless)
    if (marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7)) { pos += 2; continue; }

    uint32_t seg_len = ((uint32_t)data[pos + 2] << 8) | data[pos + 3];
    const uint8_t *seg = data + pos + 4;
    if (pos + 2 + seg_len > len) { ok = false; break; }
    uint32_t seg_data_len = seg_len - 2;

    if (marker == 0xDB) { // DQT
      size_t p = 0;
      while (p < seg_data_len) {
        uint8_t pq_tq = seg[p++];
        int32_t precision = pq_tq >> 4;
        int32_t id = pq_tq & 0x0F;
        if (id > 3) { ok = false; break; }
        for (int32_t i = 0; i < 64; i++) {
          if (precision == 0) {
            quant[id].values[i] = seg[p++];
          } else {
            quant[id].values[i] = (uint16_t)((seg[p] << 8) | seg[p + 1]);
            p += 2;
          }
        }
        quant[id].present = true;
      }
    } else if (marker == 0xC4) { // DHT
      size_t p = 0;
      while (p < seg_data_len) {
        uint8_t tc_th = seg[p++];
        int32_t class_ = tc_th >> 4;
        int32_t id = tc_th & 0x0F;
        if (id > 3) { ok = false; break; }
        uint8_t bits[17];
        bits[0] = 0;
        int32_t nvals = 0;
        for (int32_t i = 1; i <= 16; i++) { bits[i] = seg[p++]; nvals += bits[i]; }
        const uint8_t *huffval = seg + p;
        p += (size_t)nvals;
        HuffTable *t = (class_ == 0) ? &dc_huff[id] : &ac_huff[id];
        build_huff_table(t, bits, huffval, nvals);
      }
    } else if (marker == 0xC0 || marker == 0xC1) { // SOF0/SOF1 (baseline)
      int32_t precision = seg[0];
      height = (seg[1] << 8) | seg[2];
      width = (seg[3] << 8) | seg[4];
      ncomp = seg[5];
      if (precision != 8 || ncomp < 1 || ncomp > 4 || width <= 0 || height <= 0) { ok = false; break; }
      for (int32_t c = 0; c < ncomp; c++) {
        comps[c].id = seg[6 + c * 3];
        comps[c].h_samp = seg[7 + c * 3] >> 4;
        comps[c].v_samp = seg[7 + c * 3] & 0x0F;
        comps[c].quant_id = seg[8 + c * 3];
      }
      have_sof = true;
    } else if (marker == 0xC2 || marker == 0xC3 || (marker >= 0xC5 && marker <= 0xCF && marker != 0xC8)) {
      return false; // progressive/lossless/arithmetic -- out of scope, see header
    } else if (marker == 0xDA) { // SOS -- header, then entropy-coded data, then we're done
      int32_t ns = seg[0];
      for (int32_t i = 0; i < ns; i++) {
        int32_t cs = seg[1 + i * 2];
        int32_t td_ta = seg[2 + i * 2];
        for (int32_t c = 0; c < ncomp; c++) {
          if (comps[c].id == cs) {
            comps[c].dc_huff_id = td_ta >> 4;
            comps[c].ac_huff_id = td_ta & 0x0F;
          }
        }
      }
      pos += 2 + seg_len;
      break; // entropy-coded data starts at `pos` now
    }

    if (!ok) break;
    pos += 2 + seg_len;
  }

  if (!ok || !have_sof) return false;

  int32_t max_h = 1, max_v = 1;
  for (int32_t c = 0; c < ncomp; c++) {
    if (comps[c].h_samp > max_h) max_h = comps[c].h_samp;
    if (comps[c].v_samp > max_v) max_v = comps[c].v_samp;
    if (!quant[comps[c].quant_id].present) return false;
  }
  int32_t mcu_w = 8 * max_h, mcu_h = 8 * max_v;
  int32_t mcus_across = (width + mcu_w - 1) / mcu_w;
  int32_t mcus_down = (height + mcu_h - 1) / mcu_h;

  for (int32_t c = 0; c < ncomp; c++) {
    comps[c].plane_w = mcus_across * comps[c].h_samp * 8;
    comps[c].plane_h = mcus_down * comps[c].v_samp * 8;
    comps[c].plane = malloc((size_t)comps[c].plane_w * (size_t)comps[c].plane_h);
  }

  BitReader br = {data, len, pos, 0, 0};
  int32_t dc_pred[4] = {0, 0, 0, 0};
  ok = true;
  for (int32_t my = 0; my < mcus_down && ok; my++) {
    for (int32_t mx = 0; mx < mcus_across && ok; mx++) {
      for (int32_t c = 0; c < ncomp && ok; c++) {
        Component *comp = &comps[c];
        const HuffTable *dct = &dc_huff[comp->dc_huff_id];
        const HuffTable *act = &ac_huff[comp->ac_huff_id];
        if (!dct->present || !act->present) { ok = false; break; }
        for (int32_t by = 0; by < comp->v_samp && ok; by++) {
          for (int32_t bx = 0; bx < comp->h_samp && ok; bx++) {
            int32_t block_x = (mx * comp->h_samp + bx) * 8;
            int32_t block_y = (my * comp->v_samp + by) * 8;
            ok = decode_block(&br, dct, act, &quant[comp->quant_id], &dc_pred[c],
                               comp->plane, comp->plane_w, block_x, block_y);
          }
        }
      }
    }
  }

  if (!ok) {
    for (int32_t c = 0; c < ncomp; c++) free(comps[c].plane);
    return false;
  }

  uint8_t *rgba = malloc((size_t)width * (size_t)height * 4);
  if (ncomp == 1) {
    // Grayscale -- not in this port's actual asset set (all 4 real
    // files are YCbCr), but cheap to handle correctly rather than
    // reject, since a single-component SOF0 is otherwise fully valid
    // baseline JPEG.
    for (int32_t y = 0; y < height; y++) {
      for (int32_t x = 0; x < width; x++) {
        uint8_t v = comps[0].plane[y * comps[0].plane_w + x];
        int32_t o = (y * width + x) * 4;
        rgba[o + 0] = v; rgba[o + 1] = v; rgba[o + 2] = v; rgba[o + 3] = 255;
      }
    }
  } else {
    for (int32_t y = 0; y < height; y++) {
      for (int32_t x = 0; x < width; x++) {
        // Nearest-neighbour chroma upsample (spec doesn't mandate an
        // interpolation filter -- box/nearest is the simplest correct
        // choice, and these are small opaque background images, not
        // something where chroma smoothing quality matters).
        int32_t yy = comps[0].plane[(y * comps[0].v_samp / max_v) * comps[0].plane_w + (x * comps[0].h_samp / max_h)];
        int32_t cb = comps[1].plane[(y * comps[1].v_samp / max_v) * comps[1].plane_w + (x * comps[1].h_samp / max_h)];
        int32_t cr = comps[2].plane[(y * comps[2].v_samp / max_v) * comps[2].plane_w + (x * comps[2].h_samp / max_h)];
        // ITU-R BT.601 YCbCr -> RGB (spec Annex, informative; matches
        // libjpeg's/every real decoder's own conversion for JFIF files).
        float cb_ = (float)cb - 128.0f, cr_ = (float)cr - 128.0f;
        int32_t r = (int32_t)lroundf((float)yy + 1.402f * cr_);
        int32_t g = (int32_t)lroundf((float)yy - 0.344136f * cb_ - 0.714136f * cr_);
        int32_t b = (int32_t)lroundf((float)yy + 1.772f * cb_);
        int32_t o = (y * width + x) * 4;
        rgba[o + 0] = clamp_u8(r);
        rgba[o + 1] = clamp_u8(g);
        rgba[o + 2] = clamp_u8(b);
        rgba[o + 3] = 255;
      }
    }
  }

  for (int32_t c = 0; c < ncomp; c++) free(comps[c].plane);

  out->width = width;
  out->height = height;
  out->rgba = rgba;
  return true;
}

void jpeg_free(JpegImage *img) {
  free(img->rgba);
  img->rgba = NULL;
  img->width = img->height = 0;
}
