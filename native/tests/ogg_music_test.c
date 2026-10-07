// ogg_music.c against a real career track: the intro decodes to sound, and
// past its end the loop takes over.
#include <stdio.h>
#include <stdlib.h>

#include "../core/ogg_music.h"

static uint8_t *slurp(const char *path, int32_t *len) {
  FILE *f = fopen(path, "rb");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END);
  *len = (int32_t)ftell(f);
  fseek(f, 0, SEEK_SET);
  uint8_t *b = malloc((size_t)*len);
  if (fread(b, 1, (size_t)*len, f) != (size_t)*len) *len = 0;
  fclose(f);
  return b;
}

int main(void) {
  int32_t alen = 0, blen = 0;
  uint8_t *a = slurp("../../../ext/data/Files/careermusic/stage1a.ogg", &alen);
  uint8_t *b = slurp("../../../ext/data/Files/careermusic/stage1b.ogg", &blen);
  if (!a || !b) {
    fprintf(stderr, "FAIL: stage1a/b.ogg not found\n");
    return 1;
  }
  static OggMusic o;
  if (!ogg_music_start(&o, a, alen, b, blen, 48000)) {
    fprintf(stderr, "FAIL: ogg_music_start\n");
    return 1;
  }
  static int16_t out[48000 * 2];
  int64_t energy = 0;
  bool looped = false;
  for (int32_t sec = 0; sec < 600 && !looped; sec++) {   // the intro is shorter than ten minutes
    for (int32_t i = 0; i < 48000 * 2; i++) out[i] = 0;
    ogg_music_render(&o, out, 48000);
    for (int32_t i = 0; i < 48000 * 2; i += 97) energy += out[i] < 0 ? -out[i] : out[i];
    looped = o.in_loop;
  }
  ogg_music_stop(&o);
  if (energy == 0 || !looped) {
    fprintf(stderr, "FAIL: energy %lld looped %d\n", (long long)energy, looped);
    return 1;
  }
  printf("all tests passed\n");
  return 0;
}
