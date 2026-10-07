// Host-buildable test for native/core/vfs.c.
//
// readLines cases are copied verbatim from web/vfs.test.js's own
// "readLines matches DataInputStream.readLine splitting" test. The real-file
// case reads the actual repo-root stages/1.txt and checks against the same
// assertions web/vfs.test.js's "stage 1 parses into the lines the loader
// expects" test makes on it.
//
// test_read_zip covers vfs_read_zip against the real data/models.zip: 84
// entries, and a total uncompressed size of 621172 bytes -- the EXACT
// constant GameSparker.js's own loadbase() checks the archive against (see
// its comment: "Java compares the summed uncompressed size against this
// exact constant to flag a tampered/short models.zip"), so matching it
// isn't just "the test passes", it's the same integrity check the real
// game performs landing on the same number. Also spot-checks one entry's
// decompressed text (road.rad) against known content.
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../core/vfs.h"

static int failures = 0;

#define CHECK(cond, msg) do { \
  if (!(cond)) { fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); failures++; } \
} while (0)

static void check_lines(const char *text, const char **want, int32_t want_count, const char *label) {
  char **got;
  int32_t got_count;
  vfs_read_lines(text, &got, &got_count);
  char msg[128];
  snprintf(msg, sizeof(msg), "%s: count", label);
  CHECK(got_count == want_count, msg);
  for (int32_t i = 0; i < got_count && i < want_count; i++) {
    snprintf(msg, sizeof(msg), "%s: line %d", label, i);
    CHECK(strcmp(got[i], want[i]) == 0, msg);
  }
  vfs_free_lines(got, got_count);
}

static void test_read_lines(void) {
  const char *w1[] = {"a", "b", "c"};
  check_lines("a\nb\nc", w1, 3, "plain \\n");

  const char *w2[] = {"a", "b"};
  check_lines("a\nb\n", w2, 2, "no trailing empty");

  const char *w3[] = {"a", "b", "c"};
  check_lines("a\r\nb\rc", w3, 3, "mixed \\r\\n and \\r");

  const char *w4[] = {"a", "", "b"};
  check_lines("a\n\nb", w4, 3, "blank line kept");

  check_lines("", NULL, 0, "empty text");
}

static void test_read_stage_1(void) {
  vfs_set_fpath("../../../"); // native/tests/build/ -> repo root
  char *txt = vfs_read_text("stages/1.txt");
  CHECK(txt != NULL, "stages/1.txt read");
  if (!txt) return;

  char **lines;
  int32_t count;
  vfs_read_lines(txt, &lines, &count);

  bool has_name = false, has_sky = false, has_nlaps = false;
  int32_t set_count = 0;
  for (int32_t i = 0; i < count; i++) {
    // trim like the JS test does (leading/trailing whitespace)
    char *l = lines[i];
    while (*l == ' ' || *l == '\t') l++;
    size_t len = strlen(l);
    while (len > 0 && (l[len - 1] == ' ' || l[len - 1] == '\t')) len--;
    char trimmed[512];
    size_t n = len < sizeof(trimmed) - 1 ? len : sizeof(trimmed) - 1;
    memcpy(trimmed, l, n);
    trimmed[n] = '\0';

    if (strcmp(trimmed, "name(The Introductory Stage)") == 0) has_name = true;
    if (strcmp(trimmed, "sky(207,232,255)") == 0) has_sky = true;
    if (strcmp(trimmed, "nlaps(4)") == 0) has_nlaps = true;
    if (strncmp(trimmed, "set(", 4) == 0) set_count++;
  }
  CHECK(has_name, "stage 1 has name(...)");
  CHECK(has_sky, "stage 1 has sky(207,232,255)");
  CHECK(has_nlaps, "stage 1 has nlaps(4)");
  CHECK(set_count > 10, "stage 1 has more than 10 set(...) lines");

  vfs_free_lines(lines, count);
  free(txt);
}

static void test_read_zip(void) {
  vfs_set_fpath("../../../"); // native/tests/build/ -> repo root
  VfsZip zip;
  CHECK(vfs_read_zip("data/models.zip", &zip), "data/models.zip read");
  if (!zip.count) return;

  CHECK(zip.count == 84, "models.zip entry count");
  int64_t total = 0;
  bool found_road = false;
  for (int32_t i = 0; i < zip.count; i++) {
    total += zip.entries[i].len;
    if (strcmp(zip.entries[i].name, "road.rad") == 0) {
      found_road = true;
      char *text = vfs_entry_text(&zip.entries[i]);
      CHECK(strncmp(text, "div(700)", 8) == 0, "road.rad starts with div(700)");
      CHECK(strstr(text, "grounded(30000)") != NULL, "road.rad has grounded(30000)");
      free(text);
    }
  }
  CHECK(found_road, "models.zip has road.rad");
  CHECK(total == 621172, "models.zip total uncompressed size matches GameSparker.js's own constant");

  vfs_free_zip(&zip);
}

// Extended's .radq: a byte-swapped one (models, the track packs) reads as the
// zip it is, and a plain one as before. Sizes from Python's zipfile.
static void test_read_radq(void) {
  vfs_set_fpath("../../../");
  VfsZip zip;
  CHECK(vfs_read_zip("ext/data/models.radq", &zip), "swapped models.radq read");
  CHECK(zip.count == 129, "models.radq entry count");
  int64_t total = 0;
  for (int32_t i = 0; i < zip.count; i++) total += zip.entries[i].len;
  CHECK(total == 1851994, "models.radq inflates whole (every entry's size)");
  CHECK(zip.count && strcmp(zip.entries[0].name, "offroad.rad") == 0, "models.radq first entry");
  vfs_free_zip(&zip);
  CHECK(vfs_read_zip("ext/data/Files/classictracks.radq", &zip) && zip.count == 17, "classictracks.radq: 17 stages");
  vfs_free_zip(&zip);
  CHECK(vfs_read_zip("ext/data/images.radq", &zip) && zip.count == 60, "plain images.radq read");
  vfs_free_zip(&zip);
}

int main(void) {
  test_read_lines();
  test_read_stage_1();
  test_read_zip();
  test_read_radq();
  if (failures == 0) {
    printf("all tests passed\n");
    return 0;
  }
  fprintf(stderr, "%d failure(s)\n", failures);
  return 1;
}
