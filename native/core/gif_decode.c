// See gif_decode.h for scope.
#include "gif_decode.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
  const uint8_t *data;
  size_t len;
  size_t pos;
} Reader;

static bool read_bytes(Reader *r, void *out, size_t n) {
  if (r->pos + n > r->len) return false;
  memcpy(out, r->data + r->pos, n);
  r->pos += n;
  return true;
}
static bool read_u8(Reader *r, uint8_t *out) { return read_bytes(r, out, 1); }
static bool read_u16le(Reader *r, uint16_t *out) {
  uint8_t b[2];
  if (!read_bytes(r, b, 2)) return false;
  *out = (uint16_t)(b[0] | (b[1] << 8));
  return true;
}

typedef struct {
  uint8_t r, g, b;
} Rgb;

// Reads the concatenated GIF sub-block stream (each prefixed by a length
// byte, terminated by a zero-length block) into one contiguous buffer.
// Used both to skip unsupported extension blocks and to gather the raw
// LZW-compressed image data before decoding it.
static bool read_sub_blocks(Reader *r, uint8_t **out_buf, size_t *out_len) {
  size_t cap = 256, len = 0;
  uint8_t *buf = out_buf ? malloc(cap) : NULL;
  for (;;) {
    uint8_t block_len;
    if (!read_u8(r, &block_len)) { free(buf); return false; }
    if (block_len == 0) break;
    if (r->pos + block_len > r->len) { free(buf); return false; }
    if (out_buf) {
      if (len + block_len > cap) {
        while (len + block_len > cap) cap *= 2;
        buf = realloc(buf, cap);
      }
      memcpy(buf + len, r->data + r->pos, block_len);
    }
    len += block_len;
    r->pos += block_len;
  }
  if (out_buf) { *out_buf = buf; *out_len = len; }
  return true;
}

// GIF LZW: LSB-first bit packing across the sub-block byte stream.
typedef struct {
  const uint8_t *data;
  size_t len;
  size_t byte_pos;
  int32_t bit_pos; // 0-7, next bit to read within data[byte_pos]
} BitReader;

static bool read_code(BitReader *br, int32_t code_size, int32_t *out) {
  int32_t code = 0;
  for (int32_t i = 0; i < code_size; i++) {
    if (br->byte_pos >= br->len) return false;
    int32_t bit = (br->data[br->byte_pos] >> br->bit_pos) & 1;
    code |= bit << i;
    br->bit_pos++;
    if (br->bit_pos == 8) { br->bit_pos = 0; br->byte_pos++; }
  }
  *out = code;
  return true;
}

#define LZW_MAX_CODES 4096

static bool emit_byte(uint8_t *indices_out, int32_t *out_pos, int32_t pixel_count, uint8_t b) {
  if (*out_pos >= pixel_count) return false;
  indices_out[(*out_pos)++] = b;
  return true;
}

// The LZW dictionary runs to ~24KB all told (4096 int32 prefixes plus two
// 4096-byte tables), which is far too much to hold in a single stack
// frame on the PS Vita. Its main thread gets a fixed stack (see
// platform/vita/main.c) that game_run()'s own frame already claims most
// of, and the first real-hardware run died exactly here: a Data Abort on
// the FIRST write into this frame, the faulting instruction being the
// store immediately after the prologue's own `sub sp, sp, #0x6000`.
// Confirmed against the .psp2dmp coredump -- see ../TASKS_NATIVE.md's
// Vita hardware-crash entry. Held in one heap block instead, costing a
// single malloc per decoded GIF (~139 across startup, none in the render
// loop). A `static` would be cheaper still, but would make two
// concurrent decodes silently corrupt each other -- a bad trade for a
// few microseconds on a load path that already does far more I/O work
// per image than this.
typedef struct {
  int32_t prefix[LZW_MAX_CODES];
  uint8_t suffix[LZW_MAX_CODES];
  // Scratch: a dictionary entry's bytes, filled in REVERSE (last byte of
  // the sequence first, since each entry only records its own last byte
  // directly -- the rest comes from walking `prefix` back to a root).
  uint8_t rev[LZW_MAX_CODES];
} LzwTables;

static bool lzw_decode_impl(const uint8_t *lzw_data, size_t lzw_len, int32_t min_code_size,
                             uint8_t *indices_out, int32_t pixel_count, LzwTables *tb) {
  if (min_code_size < 2 || min_code_size > 8) return false;
  int32_t clear_code = 1 << min_code_size;
  int32_t end_code = clear_code + 1;

  for (int32_t i = 0; i < clear_code; i++) { tb->prefix[i] = -1; tb->suffix[i] = (uint8_t)i; }

  int32_t next_code = end_code + 1;
  int32_t code_size = min_code_size + 1;
  int32_t old_code = -1;
  int32_t out_pos = 0;

  BitReader br = {lzw_data, lzw_len, 0, 0};
  for (;;) {
    // A handful of real assets (tiny ones, e.g. a 1x3-pixel image) omit
    // the trailing end-code entirely once the LZW stream has produced
    // exactly `pixel_count` pixels -- non-conformant strictly, but real
    // encoders do it and reference decoders (confirmed against Python's
    // PIL) accept it, so this checks "buffer already full" as an
    // equally valid stop condition, not just "saw the end code".
    if (out_pos == pixel_count) break;
    int32_t code;
    if (!read_code(&br, code_size, &code)) return false;

    if (code == clear_code) {
      next_code = end_code + 1;
      code_size = min_code_size + 1;
      old_code = -1;
      continue;
    }
    if (code == end_code) break;

    if (old_code == -1) {
      // First code after a clear (or the very first code) must be a root
      // (single-pixel) entry -- nothing has been added to the dictionary
      // yet for anything else to reference.
      if (code >= clear_code) return false;
      if (!emit_byte(indices_out, &out_pos, pixel_count, tb->suffix[code])) return false;
      old_code = code;
      continue;
    }

    bool kwkwk = (code == next_code);
    int32_t entry_code = kwkwk ? old_code : code;
    if (!kwkwk && code > next_code) return false; // invalid/out-of-sequence code

    int32_t n = 0;
    int32_t w = entry_code;
    while (w != -1) {
      if (n >= LZW_MAX_CODES) return false;
      tb->rev[n++] = tb->suffix[w];
      w = tb->prefix[w];
    }
    uint8_t first_byte = tb->rev[n - 1];

    for (int32_t i = n - 1; i >= 0; i--) {
      if (!emit_byte(indices_out, &out_pos, pixel_count, tb->rev[i])) return false;
    }
    // KwKwK ("code used before it was defined"): the sequence being
    // decoded is old_code's own sequence with its OWN first byte
    // appended once more at the end -- emit that repeat now, after
    // old_code's sequence, in forward order.
    if (kwkwk) {
      if (!emit_byte(indices_out, &out_pos, pixel_count, first_byte)) return false;
    }

    if (next_code < LZW_MAX_CODES) {
      tb->prefix[next_code] = old_code;
      tb->suffix[next_code] = first_byte;
      next_code++;
      if (next_code == (1 << code_size) && code_size < 12) code_size++;
    }
    old_code = code;
  }
  return out_pos == pixel_count;
}

static bool lzw_decode(const uint8_t *lzw_data, size_t lzw_len, int32_t min_code_size,
                        uint8_t *indices_out, int32_t pixel_count) {
  LzwTables *tb = malloc(sizeof(*tb));
  if (!tb) return false; // same "decode failed" path a malformed stream takes
  bool ok = lzw_decode_impl(lzw_data, lzw_len, min_code_size, indices_out, pixel_count, tb);
  free(tb);
  return ok;
}

bool gif_decode(const uint8_t *data, size_t len, GifImage *out) {
  memset(out, 0, sizeof(*out));
  Reader r = {data, len, 0};

  uint8_t magic[6];
  if (!read_bytes(&r, magic, 6)) return false;
  if (memcmp(magic, "GIF87a", 6) != 0 && memcmp(magic, "GIF89a", 6) != 0) return false;

  uint16_t screen_w, screen_h;
  uint8_t packed, bg_index, aspect;
  if (!read_u16le(&r, &screen_w) || !read_u16le(&r, &screen_h) ||
      !read_u8(&r, &packed) || !read_u8(&r, &bg_index) || !read_u8(&r, &aspect)) {
    return false;
  }
  (void)bg_index;
  (void)aspect;

  Rgb global_table[256];
  int32_t global_count = 0;
  if (packed & 0x80) {
    global_count = 2 << (packed & 0x07);
    for (int32_t i = 0; i < global_count; i++) {
      uint8_t rgb[3];
      if (!read_bytes(&r, rgb, 3)) return false;
      global_table[i].r = rgb[0];
      global_table[i].g = rgb[1];
      global_table[i].b = rgb[2];
    }
  }

  bool have_transparency = false;
  uint8_t transparent_index = 0;

  for (;;) {
    uint8_t block_type;
    if (!read_u8(&r, &block_type)) return false;

    if (block_type == 0x3B) return false; // trailer reached with no image block

    if (block_type == 0x21) { // extension
      uint8_t label;
      if (!read_u8(&r, &label)) return false;
      if (label == 0xF9) { // graphic control extension
        uint8_t block_size;
        if (!read_u8(&r, &block_size) || block_size != 4) return false;
        uint8_t gce_packed, delay_lo, delay_hi, tidx;
        if (!read_u8(&r, &gce_packed) || !read_u8(&r, &delay_lo) ||
            !read_u8(&r, &delay_hi) || !read_u8(&r, &tidx)) {
          return false;
        }
        (void)delay_lo; (void)delay_hi;
        if (gce_packed & 0x01) { have_transparency = true; transparent_index = tidx; }
        uint8_t terminator;
        if (!read_u8(&r, &terminator) || terminator != 0) return false;
      } else {
        if (!read_sub_blocks(&r, NULL, NULL)) return false;
      }
      continue;
    }

    if (block_type == 0x2C) { // image descriptor
      uint16_t img_left, img_top, img_w, img_h;
      uint8_t img_packed;
      if (!read_u16le(&r, &img_left) || !read_u16le(&r, &img_top) ||
          !read_u16le(&r, &img_w) || !read_u16le(&r, &img_h) || !read_u8(&r, &img_packed)) {
        return false;
      }
      (void)img_left; (void)img_top;
      bool interlaced = (img_packed & 0x40) != 0;

      const Rgb *table = global_table;
      int32_t table_count = global_count;
      Rgb local_table[256];
      if (img_packed & 0x80) {
        int32_t local_count = 2 << (img_packed & 0x07);
        for (int32_t i = 0; i < local_count; i++) {
          uint8_t rgb[3];
          if (!read_bytes(&r, rgb, 3)) return false;
          local_table[i].r = rgb[0];
          local_table[i].g = rgb[1];
          local_table[i].b = rgb[2];
        }
        table = local_table;
        table_count = local_count;
      }
      if (table_count == 0) return false;

      uint8_t min_code_size;
      if (!read_u8(&r, &min_code_size)) return false;
      uint8_t *lzw_data;
      size_t lzw_len;
      if (!read_sub_blocks(&r, &lzw_data, &lzw_len)) return false;

      int32_t pixel_count = (int32_t)img_w * (int32_t)img_h;
      uint8_t *indices = malloc((size_t)pixel_count);
      bool ok = lzw_decode(lzw_data, lzw_len, min_code_size, indices, pixel_count);
      free(lzw_data);
      if (!ok) { free(indices); return false; }

      if (interlaced) {
        // The LZW stream is still one flat width*height sequence of
        // pixels either way -- interlacing only changes which SCREEN
        // ROW each successive `width`-pixel chunk of that sequence
        // belongs to. Standard 4-pass GIF interlace row order: every
        // 8th row starting at 0, then every 8th starting at 4, then
        // every 4th starting at 2, then every 2nd starting at 1.
        uint8_t *deinterlaced = malloc((size_t)pixel_count);
        int32_t src_row = 0;
        int32_t starts[4] = {0, 4, 2, 1};
        int32_t steps[4] = {8, 8, 4, 2};
        for (int32_t pass = 0; pass < 4; pass++) {
          for (int32_t y = starts[pass]; y < img_h; y += steps[pass]) {
            memcpy(deinterlaced + (size_t)y * img_w, indices + (size_t)src_row * img_w, (size_t)img_w);
            src_row++;
          }
        }
        free(indices);
        indices = deinterlaced;
      }

      uint8_t *rgba = malloc((size_t)pixel_count * 4);
      for (int32_t i = 0; i < pixel_count; i++) {
        uint8_t idx = indices[i];
        Rgb c = idx < table_count ? table[idx] : (Rgb){0, 0, 0};
        rgba[i * 4 + 0] = c.r;
        rgba[i * 4 + 1] = c.g;
        rgba[i * 4 + 2] = c.b;
        rgba[i * 4 + 3] = (have_transparency && idx == transparent_index) ? 0 : 255;
      }
      free(indices);

      out->width = img_w;
      out->height = img_h;
      out->rgba = rgba;
      return true;
    }

    return false; // unknown block type
  }
}

void gif_free(GifImage *img) {
  free(img->rgba);
  img->rgba = NULL;
  img->width = img->height = 0;
}
