// native/core/ext_stage.c: Extended's model table and its stages.
//
// Every stage of tracks.radq, classictracks.radq and matchtracks.radq must
// place, with Extended's numbering and walls; the career pack is loaded too
// and its count reported (its effects are the career's). Counts here are the
// loader's own on the shipped archives, checked against the stage files by
// hand (checkpoint lines, fix lines, wall counts).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../core/check_points.h"
#include "../core/cont_o.h"
#include "../core/ext_stage.h"
#include "../core/medium.h"
#include "../core/trackers.h"
#include "../core/vfs.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
  if (!(cond)) { fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); failures++; } \
} while (0)

static Trackers t;
static CheckPoints cp;
static ContO models[EXT_NUM_MODELS];

static int32_t count_lines(const char *text, const char *head) {
  int32_t n = 0;
  for (const char *s = text; s; s = strchr(s, '\n') ? strchr(s, '\n') + 1 : NULL) {
    while (*s == ' ' || *s == '\t') s++;
    if (strncmp(s, head, strlen(head)) == 0) n++;
  }
  return n;
}

static void test_renumber(void) {
  char old[] = "chk(44,0,0,0)\nset(14,1,2,0)p\n  setfloat(20,1,2,0,-500)\nfix(45,1,2,3,0)\nsky(1,2,3)\n";
  CHECK(ext_renumber_old_stage(old), "a checkpoint on 44 marks an old-model stage");
  CHECK(strcmp(old, "chk(40,0,0,0)\nset(10,1,2,0)p\n  setfloat(16,1,2,0,-500)\nfix(41,1,2,3,0)\nsky(1,2,3)\n") == 0,
        "set*, chk*, fix ids lowered by 4, nothing else");
  char cur[] = "chk(40,0,0,0)\nset(14,1,2,0)p\n";
  CHECK(!ext_renumber_old_stage(cur) && strcmp(cur, "chk(40,0,0,0)\nset(14,1,2,0)p\n") == 0,
        "a current stage is left alone");
}

static int32_t race_pack(Medium *m, const char *pack, const char *const *entries, int32_t n, bool must) {
  int32_t ok_count = 0;
  ContO *objects = calloc(2000, sizeof(ContO));
  for (int32_t e = 0; e < n; e++) {
    char *text = ext_stage_text(pack, entries[e]);
    char label[96];
    snprintf(label, sizeof(label), "%s/%s present", pack, entries[e]);
    CHECK(text != NULL, label);
    if (!text) continue;
    check_points_init(&cp);
    ExtStageInfo info;
    int32_t count = 0;
    const bool ok = ext_loadstage(objects, 2000, &count, models, m, &t, &cp, text, 23, &info);
    const int32_t chks = count_lines(text, "chk") + count_lines(text, "specialchk") + count_lines(text, "telechk");
    if (ok) ok_count++;
    if (must) {
      snprintf(label, sizeof(label), "%s/%s loads (%d objects, %d trackers, %d checkpoints)", pack, entries[e], count,
               t.nt, cp.nsp);
      CHECK(ok && count > 15 && t.nt > 10, label);   // The Warzone (27) is a small arena
      snprintf(label, sizeof(label), "%s/%s: one checkpoint per chk line (%d vs %d)", pack, entries[e], cp.nsp, chks);
      CHECK(cp.nsp == chks, label);
      snprintf(label, sizeof(label), "%s/%s: walls and a sector grid", pack, entries[e]);
      CHECK(info.wallr > info.walll && info.wallt > info.wallb && t.sect != NULL, label);
    } else if (!ok) {
      fprintf(stderr, "note: %s/%s does not load (%d objects, %d checkpoints)\n", pack, entries[e], count, cp.nsp);
    }
    for (int32_t i = 0; i < count; i++) cont_o_free(&objects[i]);
    memset(objects, 0, sizeof(ContO) * 2000);
    free(text);
  }
  free(objects);
  return ok_count;
}

int main(void) {
  vfs_set_fpath("../../../");   // native/tests/build*/ -> repo root
  test_renumber();

  Medium m;
  medium_init(&m);
  trackers_init(&t);
  CHECK(ext_loadbase(models, &m, &t), "ext/data/models.radq read");
  int32_t loaded = 0;
  for (int32_t i = 0; i < EXT_NUM_MODELS; i++) loaded += models[i].p != NULL;
  CHECK(loaded == EXT_NUM_MODELS, "all 129 models in their slots");
  CHECK(models[EXT_MODEL_WALL].npl > 0, "the wall model (its trackers come from the stage's max* lines)");
  CHECK(models[18].keyx[4] != 0 || models[18].keyz[4] != 0, "newcar3 keeps its fifth wheel");
  CHECK(models[22].keyx[5] != 0 || models[22].keyz[5] != 0, "secretcar3 keeps its sixth wheel");
  CHECK(models[78].maxR > models[0].maxR, "beast models read on the 6 scale: bigger than their cars");

  // tracks.radq has no 26 (the Premier Tournament is matchtracks') and a 28.
  static const char *const kTracks[27] = {"1.txt", "2.txt", "3.txt", "4.txt", "5.txt", "6.txt", "7.txt", "8.txt",
                                          "9.txt", "10.txt", "11.txt", "12.txt", "13.txt", "14.txt", "15.txt",
                                          "16.txt", "17.txt", "18.txt", "19.txt", "20.txt", "21.txt", "22.txt",
                                          "23.txt", "24.txt", "25.txt", "27.txt", "28.txt"};
  static const char *const kMatch[5] = {"26m1.txt", "26m2.txt", "26m3.txt", "26m4.txt", "26m5.txt"};
  CHECK(race_pack(&m, "tracks", kTracks, 27, true) == 27, "every tracks.radq stage loads");
  CHECK(race_pack(&m, "classictracks", kTracks, 17, true) == 17, "every classictracks.radq stage loads");
  CHECK(race_pack(&m, "matchtracks", kMatch, 5, true) == 5, "every matchtracks.radq stage loads");

  // Extended's setpoint marks a route point the AI repairs at.
  {
    char *text = ext_stage_text("classictracks", "1.txt");
    ContO *objects = calloc(2000, sizeof(ContO));
    ExtStageInfo info;
    int32_t count = 0;
    check_points_init(&cp);
    CHECK(text && ext_loadstage(objects, 2000, &count, models, &m, &t, &cp, text, 23, &info), "classic 1 loads");
    CHECK(info.numfixes == 1 && cp.x[info.fixpoint[0]] == -10220 && cp.z[info.fixpoint[0]] == 22400,
          "classic 1's setpoint is its route point at (-10220, 22400)");
    for (int32_t i = 0; i < count; i++) cont_o_free(&objects[i]);
    free(objects);
    free(text);
  }

  static const char *kCareer[36];
  char names[36][16];
  for (int32_t i = 0; i < 31; i++) {
    snprintf(names[i], sizeof(names[i]), "%d.txt", i + 1);
    kCareer[i] = names[i];
  }
  for (int32_t i = 0; i < 4; i++) {
    snprintf(names[31 + i], sizeof(names[31 + i]), "bonus/%d.txt", i + 1);
    kCareer[31 + i] = names[31 + i];
  }
  const int32_t career = race_pack(&m, "careertracks", kCareer, 35, false);
  fprintf(stderr, "careertracks: %d of 35 load\n", career);

  if (failures == 0) {
    printf("all tests passed\n");
    return 0;
  }
  fprintf(stderr, "%d failure(s)\n", failures);
  return 1;
}
