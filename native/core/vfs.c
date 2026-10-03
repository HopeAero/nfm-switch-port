// ports the file-reading half of web/vfs.js -- see vfs.h.
//
// NOT ported: detectFpath's probe logic -- its ./ vs ../ HTTP probe has no
// meaning on a real filesystem; the platform sets vfs_set_fpath() directly
// instead (see platform/linux/main.c).
#include "vfs.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

static char *g_fpath = NULL;

void vfs_set_fpath(const char *path) {
  free(g_fpath);
  if (!path) path = "";
  size_t len = strlen(path);
  g_fpath = malloc(len + 1);
  memcpy(g_fpath, path, len + 1);
}

static char *full_path(const char *path) {
  size_t prefix_len = g_fpath ? strlen(g_fpath) : 0;
  size_t path_len = strlen(path);
  char *full = malloc(prefix_len + path_len + 1);
  if (g_fpath) memcpy(full, g_fpath, prefix_len);
  memcpy(full + prefix_len, path, path_len + 1);
  return full;
}

/** Reads a whole file into a malloc'd buffer, NUL-terminated one byte past
 * `*out_size` for callers that want to treat it as text. NULL on failure. */
static uint8_t *read_file_bytes(const char *path, size_t *out_size) {
  char *full = full_path(path);
  FILE *f = fopen(full, "rb");
  if (!f) {
    fprintf(stderr, "vfs: %s: %s\n", full, strerror(errno));
    free(full);
    return NULL;
  }
  free(full);

  if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
  long size = ftell(f);
  if (size < 0) { fclose(f); return NULL; }
  if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }

  uint8_t *buf = malloc((size_t)size + 1);
  size_t got = fread(buf, 1, (size_t)size, f);
  fclose(f);
  buf[got] = '\0';
  *out_size = got;
  return buf;
}

char *vfs_read_text(const char *path) {
  size_t size;
  uint8_t *buf = read_file_bytes(path, &size);
  return (char *)buf; // already NUL-terminated by read_file_bytes
}

uint8_t *vfs_read_bytes(const char *path, int32_t *out_len) {
  size_t size;
  uint8_t *buf = read_file_bytes(path, &size);
  if (buf) *out_len = (int32_t)size;
  return buf;
}

void vfs_free_bytes(uint8_t *bytes) {
  free(bytes);
}

void vfs_read_lines(const char *text, char ***out_lines, int32_t *out_count) {
  size_t cap = 16;
  char **lines = malloc(sizeof(char *) * cap);
  int32_t count = 0;

  const char *p = text;
  const char *line_start = text;
  while (*p) {
    if (*p == '\r' || *p == '\n') {
      size_t len = (size_t)(p - line_start);
      char *line = malloc(len + 1);
      memcpy(line, line_start, len);
      line[len] = '\0';
      if ((size_t)count == cap) { cap *= 2; lines = realloc(lines, sizeof(char *) * cap); }
      lines[count++] = line;
      bool crlf = (*p == '\r' && p[1] == '\n');
      p += crlf ? 2 : 1;
      line_start = p;
    } else {
      p++;
    }
  }
  // Final segment, from line_start to the end (possibly empty).
  size_t len = (size_t)(p - line_start);
  char *line = malloc(len + 1);
  memcpy(line, line_start, len);
  line[len] = '\0';
  if ((size_t)count == cap) { cap *= 2; lines = realloc(lines, sizeof(char *) * cap); }
  lines[count++] = line;

  // No trailing empty line for text ending in a newline -- matches
  // web/vfs.js's readLines() popping a final ''. A blank line ANYWHERE
  // ELSE (including the whole input being empty, which behaves the same
  // as JS's ''.split(...) -> [''] -> popped -> []) is kept as-is.
  if (count > 0 && lines[count - 1][0] == '\0') {
    free(lines[count - 1]);
    count--;
  }

  *out_lines = lines;
  *out_count = count;
}

void vfs_free_lines(char **lines, int32_t count) {
  for (int32_t i = 0; i < count; i++) free(lines[i]);
  free(lines);
}

// --- ZIP -----------------------------------------------------------------
//
// Ports web/vfs.js's readZip/parseZip. See vfs.h's doc comment on
// vfs_read_zip for the format notes (EOCD-based lookup, store/deflate
// only) -- this is a mechanical translation of the same central-directory
// walk, with zlib's inflate() standing in for the JS's
// DecompressionStream('deflate-raw').

static uint16_t ru16(const uint8_t *p) {
  return (uint16_t)(p[0] | (p[1] << 8));
}
static uint32_t ru32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

#define EOCD_SIG 0x06054b50u
#define CD_SIG   0x02014b50u

/** Raw-deflate inflate (zip's method 8), matching DecompressionStream
 * ('deflate-raw'): no zlib/gzip header, windowBits = -15. */
static bool inflate_raw(const uint8_t *src, uint32_t src_len, uint8_t *dst, uint32_t dst_len) {
  z_stream strm;
  memset(&strm, 0, sizeof(strm));
  if (inflateInit2(&strm, -15) != Z_OK) return false;
  strm.next_in = (Bytef *)src;
  strm.avail_in = src_len;
  strm.next_out = (Bytef *)dst;
  strm.avail_out = dst_len;
  int ret = inflate(&strm, Z_FINISH);
  bool ok = (ret == Z_STREAM_END) && (strm.avail_out == 0);
  inflateEnd(&strm);
  return ok;
}

bool vfs_read_zip(const char *path, VfsZip *out) {
  memset(out, 0, sizeof(*out));

  int32_t buf_len;
  uint8_t *buf = vfs_read_bytes(path, &buf_len);
  if (!buf) return false;
  if (buf_len < 22) { vfs_free_bytes(buf); return false; }

  // Locate the EOCD: scan backwards over the maximum comment length.
  int32_t eocd = -1;
  int32_t min_eocd = buf_len - 22 - 0xffff;
  if (min_eocd < 0) min_eocd = 0;
  for (int32_t i = buf_len - 22; i >= min_eocd; i--) {
    if (ru32(buf + i) == EOCD_SIG) { eocd = i; break; }
  }
  if (eocd < 0) {
    fprintf(stderr, "vfs: %s: not a zip (no EOCD)\n", path);
    vfs_free_bytes(buf);
    return false;
  }

  int32_t count = ru16(buf + eocd + 10);
  uint32_t p = ru32(buf + eocd + 16); // central directory offset

  VfsZipEntry *entries = calloc((size_t)count, sizeof(VfsZipEntry));
  for (int32_t i = 0; i < count; i++) {
    if (p + 46 > (uint32_t)buf_len || ru32(buf + p) != CD_SIG) {
      fprintf(stderr, "vfs: %s: bad central directory at entry %d\n", path, i);
      for (int32_t j = 0; j < i; j++) { free(entries[j].name); free(entries[j].data); }
      free(entries);
      vfs_free_bytes(buf);
      return false;
    }
    uint16_t method = ru16(buf + p + 10);
    uint32_t comp_size = ru32(buf + p + 20);
    uint32_t uncomp_size = ru32(buf + p + 24);
    uint16_t name_len = ru16(buf + p + 28);
    uint16_t extra_len = ru16(buf + p + 30);
    uint16_t comment_len = ru16(buf + p + 32);
    uint32_t local_off = ru32(buf + p + 42);

    char *name = malloc((size_t)name_len + 1);
    memcpy(name, buf + p + 46, name_len);
    name[name_len] = '\0';
    p += 46u + name_len + extra_len + comment_len;

    // The local header's own name/extra lengths give the true data offset;
    // the central directory's extra field length often differs from it.
    uint16_t l_name_len = ru16(buf + local_off + 26);
    uint16_t l_extra_len = ru16(buf + local_off + 28);
    uint32_t data_off = local_off + 30u + l_name_len + l_extra_len;
    const uint8_t *raw = buf + data_off;

    entries[i].name = name;
    if (method == 0) {
      entries[i].data = malloc(comp_size);
      memcpy(entries[i].data, raw, comp_size);
      entries[i].len = (int32_t)comp_size;
    } else if (method == 8) {
      entries[i].data = malloc(uncomp_size > 0 ? uncomp_size : 1);
      if (!inflate_raw(raw, comp_size, entries[i].data, uncomp_size)) {
        fprintf(stderr, "vfs: %s: inflate failed for %s\n", path, name);
        for (int32_t j = 0; j <= i; j++) { free(entries[j].name); free(entries[j].data); }
        free(entries);
        vfs_free_bytes(buf);
        return false;
      }
      entries[i].len = (int32_t)uncomp_size;
    } else {
      fprintf(stderr, "vfs: %s: %s: unsupported compression method %u\n", path, name, method);
      for (int32_t j = 0; j <= i; j++) { free(entries[j].name); free(entries[j].data); }
      free(entries);
      vfs_free_bytes(buf);
      return false;
    }
  }

  vfs_free_bytes(buf);
  out->entries = entries;
  out->count = count;
  return true;
}

void vfs_free_zip(VfsZip *zip) {
  for (int32_t i = 0; i < zip->count; i++) {
    free(zip->entries[i].name);
    free(zip->entries[i].data);
  }
  free(zip->entries);
  zip->entries = NULL;
  zip->count = 0;
}

char *vfs_entry_text(const VfsZipEntry *entry) {
  char *buf = malloc((size_t)entry->len + 1);
  memcpy(buf, entry->data, (size_t)entry->len);
  buf[entry->len] = '\0';
  return buf;
}
