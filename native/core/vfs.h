// ports the file-reading half of web/vfs.js: readText, readLines, readBytes,
// readZip/parseZip/entryText. NOT detectFpath -- see vfs.c's header comment
// for why (its ./ vs ../ HTTP probe has no meaning on a real filesystem).
//
// Genuinely platform-agnostic despite the name suggesting "platform glue":
// both Linux and PS Vita provide standard C `fopen`/`fread` (VitaSDK's
// newlib does too, against `ux0:`-style paths), so the only thing that
// differs per platform is the STRING passed to vfs_set_fpath, not the code
// that reads through it. That path-selection decision belongs in
// platform/{linux,vita}/, this file does not.
#ifndef NFM_VFS_H
#define NFM_VFS_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Equivalent of Madness.fpath / web/vfs.js's `fpath` -- prefixes every
// path passed to vfs_read_text. Defaults to "" (paths relative to the
// process's current directory). Copies `path`; safe to free the argument
// after the call.
void vfs_set_fpath(const char *path);

/**
 * Read `fpath + path` as text. Returns a malloc'd, NUL-terminated buffer
 * the caller must free, or NULL on failure (missing file, read error) --
 * mirrors web/vfs.js's readText() throwing on a missing file, adapted to
 * C's error-return convention rather than exceptions.
 *
 * No encoding conversion: bytes are copied through as-is. This matches
 * java.io.DataInputStream.readLine()'s actual behaviour (each byte 0-255
 * maps directly to one char, i.e. ISO-8859-1) and web/vfs.js's entryText()
 * (explicitly iso-8859-1) -- NOT web/vfs.js's readText(), which decodes as
 * UTF-8 via the browser fetch API and so can differ from the Java on
 * non-ASCII bytes. That is a latent inconsistency in the existing JS port,
 * not something to replicate: a fresh native implementation has no fetch()
 * API to inherit it from, so it goes with the behaviour that actually
 * matches Java.
 */
char *vfs_read_text(const char *path);

/**
 * Split text into lines the way DataInputStream.readLine() does: on \n,
 * \r, or \r\n, with no trailing empty line for text ending in a newline.
 * Ports web/vfs.js's readLines() exactly (see its own tests in
 * web/vfs.test.js for the cases this must match).
 *
 * Writes a malloc'd array of malloc'd, NUL-terminated line strings to
 * *out_lines (caller frees each string, then the array) and the count to
 * *out_count. `text` is not modified or retained.
 */
void vfs_read_lines(const char *text, char ***out_lines, int32_t *out_count);

/** Frees an array produced by vfs_read_lines. */
void vfs_free_lines(char **lines, int32_t count);

/**
 * Read `fpath + path` as raw bytes. Returns a malloc'd buffer the caller
 * must free via vfs_free_bytes, or NULL on failure. Ports web/vfs.js's
 * readBytes() (the binary-safe counterpart to vfs_read_text, needed for
 * zip archives -- .rad/.txt game files are all plain text and use
 * vfs_read_text instead).
 */
uint8_t *vfs_read_bytes(const char *path, int32_t *out_len);
void vfs_free_bytes(uint8_t *bytes);

/** One entry of a parsed zip archive -- name and (already-decompressed)
 * data, both owned by the VfsZip that produced them. */
typedef struct {
  char *name; // NUL-terminated, as stored in the zip's central directory
  uint8_t *data;
  int32_t len;
} VfsZipEntry;

typedef struct {
  VfsZipEntry *entries;
  int32_t count;
} VfsZip;

/**
 * Read and decode a zip archive at `fpath + path` into `out`. Ports
 * web/vfs.js's readZip()+parseZip() (split apart there for headless
 * testing; combined here since there is no browser `fetch` step to keep
 * separate). Locates entries via the End Of Central Directory record, same
 * as the JS -- see vfs.c's header comment for why (local headers can carry
 * zero sizes with a trailing data descriptor; the EOCD's directory doesn't).
 *
 * Supports store (method 0) and deflate (method 8) entries -- every entry
 * in this game's data/ zip archives is one or the other. Deflate entries
 * are inflated via zlib's raw-deflate mode (no zlib/gzip wrapper, matching
 * the zip format's own raw DEFLATE streams). Assumed available on PS Vita
 * too via VitaSDK's bundled zlib port -- not yet verified, no VitaSDK in
 * this environment; flag if that assumption turns out wrong.
 *
 * Returns false (and leaves *out zeroed) on a malformed archive or missing
 * file. Entry order matches the zip's central directory order.
 */
bool vfs_read_zip(const char *path, VfsZip *out); // also Extended's .radq, swapped or not

/** Frees every entry's name/data and the entries array itself. */
void vfs_free_zip(VfsZip *zip);

/**
 * Decode a zip entry's bytes as text -- ports web/vfs.js's entryText().
 * Same no-conversion behaviour as vfs_read_text (see its doc comment):
 * raw bytes copied through, matching ISO-8859-1 / DataInputStream.readLine.
 * Returns a malloc'd, NUL-terminated buffer the caller must free.
 */
char *vfs_entry_text(const VfsZipEntry *entry);

#ifdef __cplusplus
}
#endif

#endif
