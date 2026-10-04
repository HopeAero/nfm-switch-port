// radical_mod.c against the original jar's own music output.
//
// The expected values are the jar's: new RadicalMod(...) built by
// reflection on java/Game.jar (the same route web/tools/BakeMusic.java
// takes), its SuperClip stream dumped and hashed (FNV-1a 64), together
// with the stream length and the two loop fields. All 34 tracks were
// compared byte for byte when the port was written; these four cover a
// plain track, one with a rollBack loop point, the menu track
// (loadimod's own parameters) and party.zip.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../core/radical_mod.h"
#include "../core/vfs.h"

static uint64_t fnv1a(const uint8_t *b, int32_t n) {
  uint64_t h = 0xcbf29ce484222325ULL;
  for (int32_t i = 0; i < n; i++) h = (h ^ b[i]) * 0x100000001b3ULL;
  return h;
}

typedef struct {
  const char *zip;
  int32_t vol, rate, bpm; // vol < 0: the menu track
  int32_t len, rb_pos, rb_trig;
  uint64_t hash;
} Case;

static const Case kCases[] = {
    {"music/stage1.zip", 240, 8400, 135, 6570676, 0, 6570676, 0x60d3355943316955ULL},
    {"music/stage3.zip", 170, 8500, 145, 8360639, 2480062, 158783, 0x81918cb45c6c5abaULL},
    {"music/interface.zip", -1, 0, 0, 8959281, 0, 8959281, 0x5070f1dd8a46c353ULL},
    {"music/party.zip", 400, 7600, 125, 8759291, 0, 8759291, 0x103c0073e5ee9e80ULL},
};

static int failures = 0;
#define CHECK(c, ...) do { if (!(c)) { fprintf(stderr, "FAIL: " __VA_ARGS__); fprintf(stderr, "\n"); failures++; } } while (0)

static void test_tracks_match_the_jar(void) {
  for (size_t i = 0; i < sizeof(kCases) / sizeof(kCases[0]); i++) {
    const Case *c = &kCases[i];
    VfsZip zip;
    if (!vfs_read_zip(c->zip, &zip)) { CHECK(0, "could not open %s", c->zip); continue; }
    RadicalTrack t;
    bool ok = c->vol < 0 ? radical_render_interface(zip.entries[0].data, (size_t)zip.entries[0].len, &t)
                         : radical_render_stage(zip.entries[0].data, (size_t)zip.entries[0].len, c->vol, c->rate,
                                                c->bpm, &t);
    vfs_free_zip(&zip);
    CHECK(ok, "%s did not render", c->zip);
    if (!ok) continue;
    CHECK(t.len == c->len, "%s: len %d, jar %d", c->zip, t.len, c->len);
    CHECK(t.roll_back_pos == c->rb_pos, "%s: rollBackPos %d, jar %d", c->zip, t.roll_back_pos, c->rb_pos);
    CHECK(t.roll_back_trig == c->rb_trig, "%s: rollBackTrig %d, jar %d", c->zip, t.roll_back_trig, c->rb_trig);
    CHECK(fnv1a(t.bytes, t.len) == c->hash, "%s: bytes differ from the jar's", c->zip);
    radical_track_free(&t);
  }
}

// SuperClip.run() on a small stream: pairs of bytes as LE frames, 22000-byte
// chunks, an odd tail padded with a zero byte, then back to rollBackPos once
// fewer than rollBackTrig bytes remain.
static void test_superclip_loop(void) {
  uint8_t bytes[50001];
  for (int32_t i = 0; i < 50001; i++) bytes[i] = (uint8_t)(i * 7);
  RadicalTrack t = {bytes, 50001, 0, 50001};
  RadicalPlayer p;
  radical_player_start(&p, &t, RADICAL_PLAY_RATE);
  // start() pre-reads two frames for the resampler; restart the pull.
  memset(&p, 0, sizeof(p));
  p.track = &t;
  for (int32_t i = 0; i < 25000; i++) {
    int16_t v = radical_player_next_frame(&p);
    int16_t want = (int16_t)(bytes[2 * i] | bytes[2 * i + 1] << 8);
    if (v != want) { CHECK(0, "frame %d: %d, want %d", i, v, want); return; }
  }
  // The odd last byte, padded with zero, then the stream from the top.
  int16_t tail = radical_player_next_frame(&p);
  CHECK(tail == (int16_t)bytes[50000], "padded tail frame %d, want %d", tail, bytes[50000]);
  int16_t again = radical_player_next_frame(&p);
  CHECK(again == (int16_t)(bytes[0] | bytes[1] << 8), "no restart at end of stream");

  // With a loop point: the chunk that leaves fewer than rollBackTrig bytes
  // still plays, then the stream continues from rollBackPos.
  RadicalTrack l = {bytes, 50000, 1000, 10000};
  memset(&p, 0, sizeof(p));
  p.track = &l;
  // chunk 1: 0..21999 (available 50000), chunk 2: 22000..43999 (28000),
  // chunk 3: available 6000 < 10000 -> plays 44000..49999, then 1000...
  for (int32_t i = 0; i < 25000; i++) radical_player_next_frame(&p);
  int16_t v = radical_player_next_frame(&p);
  CHECK(v == (int16_t)(bytes[1000] | bytes[1001] << 8), "loop did not return to rollBackPos");
}

int main(void) {
  vfs_set_fpath("../../../");
  test_tracks_match_the_jar();
  test_superclip_loop();
  if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
  printf("radical_mod_test: all passed\n");
  return 0;
}
