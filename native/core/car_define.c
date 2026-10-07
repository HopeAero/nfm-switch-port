// ports web/CarDefine.js -- see car_define.h for scope.
//
// Every table below is transcribed verbatim from the JS constructor's
// literal arrays (same order, same values). One genuine quirk preserved,
// not fixed: the JS's own `this.acelf` array is only 55 elements long
// (every other table is 56 -- 16 built-in cars + 40 custom-car slots).
// Padded to 56 here with one more {0,0,0} row so this struct's arrays are
// uniformly sized; unreachable for the 16 built-in cars this port
// actually drives.
#include "car_define.h"
#include "cont_o.h"
#include "java_compat.h"
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static const int32_t handb_data[56] = {7,10,7,15,12,8,9,10,5,7,8,10,8,12,7,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static const int32_t airc_data[56] = {70,30,40,40,30,50,40,90,40,50,75,10,50,0,100,60,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static const int32_t turn_data[56] = {6,9,5,7,8,7,5,5,9,7,7,4,6,5,7,6,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static const int32_t push_data[56] = {2,2,3,3,2,2,2,4,2,2,2,4,2,2,2,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static const float revpush_data[56] = {2.0f,3.0f,2.0f,2.0f,2.0f,2.0f,2.0f,1.0f,2.0f,1.0f,2.0f,1.0f,2.0f,2.0f,2.0f,1.0f,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static const int32_t lift_data[56] = {0,30,0,20,0,30,0,0,20,0,0,0,10,0,30,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static const int32_t revlift_data[56] = {0,0,15,0,0,0,0,0,0,0,0,0,0,0,0,32,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static const int32_t powerloss_data[56] = {2500000,2500000,3500000,2500000,4000000,2500000,3200000,3200000,2750000,5500000,2750000,4500000,3500000,16700000,3000000,5500000,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static const int32_t flipy_data[56] = {-50,-60,-92,-44,-60,-57,-54,-60,-77,-57,-82,-85,-28,-100,-63,-127,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static const int32_t msquash_data[56] = {7,4,7,2,8,4,6,4,3,8,4,10,3,20,3,8,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static const int32_t clrad_data[56] = {3300,1700,4700,3000,2000,4500,3500,5000,10000,15000,4000,7000,10000,15000,5500,5000,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static const int32_t maxmag_data[56] = {7600,4200,7200,6000,6000,15000,17200,17000,18000,11000,19000,10700,13000,45000,5800,18000,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static const float dishandle_data[56] = {0.65f,0.6f,0.55f,0.77f,0.62f,0.9f,0.6f,0.72f,0.45f,0.8f,0.95f,0.4f,0.87f,0.42f,1.0f,0.95f,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static const float outdam_data[56] = {0.68f,0.35f,0.8f,0.5f,0.42f,0.76f,0.82f,0.76f,0.72f,0.62f,0.79f,0.95f,0.77f,1.0f,0.85f,1.0f,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static const int32_t cclass_data[56] = {0,0,0,0,0,1,2,2,2,2,3,4,4,4,4,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static const int32_t enginsignature_data[56] = {0,1,2,1,0,3,2,2,1,0,3,4,1,4,0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static const float airs_data[56] = {1.0f,1.2f,0.95f,1.0f,2.2f,1.0f,0.9f,0.8f,1.0f,0.9f,1.15f,0.8f,1.0f,0.3f,1.3f,1.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f};
static const float grip_data[56] = {20.0f,27.0f,18.0f,22.0f,19.0f,20.0f,25.0f,20.0f,19.0f,24.0f,22.5f,25.0f,30.0f,27.0f,25.0f,27.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f};
static const float bounce_data[56] = {1.2f,1.05f,1.3f,1.15f,1.3f,1.2f,1.15f,1.1f,1.2f,1.1f,1.15f,0.8f,1.05f,0.8f,1.1f,1.15f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f};
static const float simag_data[56] = {0.9f,0.85f,1.05f,0.9f,0.85f,0.9f,1.05f,0.9f,1.0f,1.05f,0.9f,1.1f,0.9f,1.3f,0.9f,1.15f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f};
static const float moment_data[56] = {1.3f,0.75f,1.4f,1.2f,1.1f,1.38f,1.43f,1.48f,1.35f,1.7f,1.42f,2.0f,1.26f,3.0f,1.5f,2.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f};
static const float comprad_data[56] = {0.5f,0.4f,0.8f,0.5f,0.4f,0.5f,0.5f,0.5f,0.5f,0.8f,0.5f,1.5f,0.5f,0.8f,0.5f,0.8f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f};
static const float dammult_data[56] = {0.75f,0.8f,0.45f,0.8f,0.42f,0.7f,0.72f,0.6f,0.58f,0.41f,0.67f,0.45f,0.61f,0.25f,0.38f,0.52f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f};
static const int32_t swits_data[56][3] = {
  {50,185,282},
  {100,200,310},
  {60,180,275},
  {76,195,298},
  {70,170,275},
  {70,202,293},
  {60,170,289},
  {70,206,291},
  {90,210,295},
  {90,190,276},
  {70,200,295},
  {50,160,270},
  {90,200,305},
  {50,130,210},
  {80,200,300},
  {70,210,290},
  {0,0,0},
  {0,0,0},
  {0,0,0},
  {0,0,0},
  {0,0,0},
  {0,0,0},
  {0,0,0},
  {0,0,0},
  {0,0,0},
  {0,0,0},
  {0,0,0},
  {0,0,0},
  {0,0,0},
  {0,0,0},
  {0,0,0},
  {0,0,0},
  {0,0,0},
  {0,0,0},
  {0,0,0},
  {0,0,0},
  {0,0,0},
  {0,0,0},
  {0,0,0},
  {0,0,0},
  {0,0,0},
  {0,0,0},
  {0,0,0},
  {0,0,0},
  {0,0,0},
  {0,0,0},
  {0,0,0},
  {0,0,0},
  {0,0,0},
  {0,0,0},
  {0,0,0},
  {0,0,0},
  {0,0,0},
  {0,0,0},
  {0,0,0},
  {0,0,0},
};
static const float acelf_data[56][3] = {
  {11.0f,5.0f,3.0f},
  {14.0f,7.0f,5.0f},
  {10.0f,5.0f,3.5f},
  {11.0f,6.0f,3.5f},
  {10.0f,5.0f,3.5f},
  {12.0f,6.0f,3.0f},
  {7.0f,9.0f,4.0f},
  {11.0f,5.0f,3.0f},
  {12.0f,7.0f,4.0f},
  {12.0f,7.0f,3.5f},
  {11.5f,6.5f,3.5f},
  {9.0f,5.0f,3.0f},
  {13.0f,7.0f,4.5f},
  {7.5f,3.5f,3.0f},
  {11.0f,7.5f,4.0f},
  {12.0f,6.0f,3.5f},
  {0.0f,0.0f,0.0f},
  {0.0f,0.0f,0.0f},
  {0.0f,0.0f,0.0f},
  {0.0f,0.0f,0.0f},
  {0.0f,0.0f,0.0f},
  {0.0f,0.0f,0.0f},
  {0.0f,0.0f,0.0f},
  {0.0f,0.0f,0.0f},
  {0.0f,0.0f,0.0f},
  {0.0f,0.0f,0.0f},
  {0.0f,0.0f,0.0f},
  {0.0f,0.0f,0.0f},
  {0.0f,0.0f,0.0f},
  {0.0f,0.0f,0.0f},
  {0.0f,0.0f,0.0f},
  {0.0f,0.0f,0.0f},
  {0.0f,0.0f,0.0f},
  {0.0f,0.0f,0.0f},
  {0.0f,0.0f,0.0f},
  {0.0f,0.0f,0.0f},
  {0.0f,0.0f,0.0f},
  {0.0f,0.0f,0.0f},
  {0.0f,0.0f,0.0f},
  {0.0f,0.0f,0.0f},
  {0.0f,0.0f,0.0f},
  {0.0f,0.0f,0.0f},
  {0.0f,0.0f,0.0f},
  {0.0f,0.0f,0.0f},
  {0.0f,0.0f,0.0f},
  {0.0f,0.0f,0.0f},
  {0.0f,0.0f,0.0f},
  {0.0f,0.0f,0.0f},
  {0.0f,0.0f,0.0f},
  {0.0f,0.0f,0.0f},
  {0.0f,0.0f,0.0f},
  {0.0f,0.0f,0.0f},
  {0.0f,0.0f,0.0f},
  {0.0f,0.0f,0.0f},
  {0.0f,0.0f,0.0f},
  {0.0f,0.0f,0.0f}, // JS's own acelf array is only 55 long (see car_define.c) -- padded, unreachable for built-in cars 0-15
};

void car_define_init(CarDefine *cd) {
  memcpy(cd->handb, handb_data, sizeof(cd->handb));
  memcpy(cd->airs, airs_data, sizeof(cd->airs));
  memcpy(cd->airc, airc_data, sizeof(cd->airc));
  memcpy(cd->turn, turn_data, sizeof(cd->turn));
  memcpy(cd->grip, grip_data, sizeof(cd->grip));
  memcpy(cd->bounce, bounce_data, sizeof(cd->bounce));
  memcpy(cd->simag, simag_data, sizeof(cd->simag));
  memcpy(cd->moment, moment_data, sizeof(cd->moment));
  memcpy(cd->comprad, comprad_data, sizeof(cd->comprad));
  memcpy(cd->push, push_data, sizeof(cd->push));
  memcpy(cd->revpush, revpush_data, sizeof(cd->revpush));
  memcpy(cd->lift, lift_data, sizeof(cd->lift));
  memcpy(cd->revlift, revlift_data, sizeof(cd->revlift));
  memcpy(cd->powerloss, powerloss_data, sizeof(cd->powerloss));
  memcpy(cd->flipy, flipy_data, sizeof(cd->flipy));
  memcpy(cd->msquash, msquash_data, sizeof(cd->msquash));
  memcpy(cd->clrad, clrad_data, sizeof(cd->clrad));
  memcpy(cd->dammult, dammult_data, sizeof(cd->dammult));
  memcpy(cd->maxmag, maxmag_data, sizeof(cd->maxmag));
  memcpy(cd->enginsignature, enginsignature_data, sizeof(cd->enginsignature));
  memcpy(cd->acelf, acelf_data, sizeof(cd->acelf));
  memcpy(cd->swits, swits_data, sizeof(cd->swits));
  // The car-select screen's Handling and Endurance bars, and the class
  // (CarDefine.java:108-110); loadstat() sets them for a .rad car.
  memcpy(cd->dishandle, dishandle_data, sizeof(cd->dishandle));
  memcpy(cd->outdam, outdam_data, sizeof(cd->outdam));
  memcpy(cd->cclass, cclass_data, sizeof(cd->cclass));
}

// --- loadstat --------------------------------------------------------
//
// Ports CarDefine.js's `loadstat(buf, s, n, n2, n3, n4)` plus its
// `getvalue()` helper. The JS scans the .rad file's TEXT for lines
// starting with `stat(`/`physics(`/`handling(` and pulls comma-separated
// numeric values out of each. The interpolation tables that follow
// (`array[0]`->swits, `array[1]`->acelf via built-in cars 0-15 as
// keyframes) are transcribed verbatim from the JS -- see the "n12/n13/
// n14" and "n16/n17/n18" ladder in web/CarDefine.js.

// Ports `getvalue(s, s2, n)`. Scans s2 starting after `${s}(` and
// returns the nth comma-separated numeric field, truncated to int. The
// JS THROWS on a missing/malformed field (caught by loadstat's outer
// try/catch, flipping b/b2 to false); the C port uses a *found out-
// param instead (setting *found = false), letting the caller unwind the
// enclosing block without exceptions.
//
// Ported literally from the JS's own for-loop body/order, INCLUDING its
// double-incrementing of `i` inside the `,`/`)` branch: `++n2; ++i;`
// there, THEN (unconditionally, whether or not this iteration hit a
// delimiter) `if (n2 === n) string += s2[i]`, THEN the for-loop's own
// `++i`. Net effect for a delimiter iteration: the char immediately
// after the delimiter is what gets tested/appended (not skipped) --
// confirmed against the real web/CarDefine.js under Node on
// "stat(114,150,135,97,104)" (yields 114/150/135/97/104, no dropped
// digits). An earlier version of this port double-incremented `idx`
// AND skipped the append on the delimiter iteration, which silently
// dropped the first digit of every field after the first -- fixed here
// to match the JS's real per-character order exactly.
static int32_t cd_getvalue(const char *tag, const char *line, int32_t n, bool *found) {
  int32_t tag_len = (int32_t)strlen(tag);
  int32_t line_len = (int32_t)strlen(line);
  int32_t idx = tag_len + 1; // past `tag(`
  int32_t field = 0;
  char buf[32];
  int32_t buf_len = 0;
  bool any = false;
  while (idx < line_len) {
    char c = line[idx];
    if (c == ',' || c == ')') {
      field++;
      idx++;
    }
    if (field == n && idx < line_len) {
      if (buf_len < (int32_t)sizeof(buf) - 1) buf[buf_len++] = line[idx];
      any = true;
    }
    idx++;
  }
  *found = any;
  if (!any) return 0;
  buf[buf_len] = '\0';
  // JS: parseFloat + trunc. C: strtod + jtrunc_d.
  char *endp;
  double val = strtod(buf, &endp);
  if (endp == buf) { *found = false; return 0; }
  return jtrunc_d(val);
}

static bool cd_line_starts_with(const char *line, const char *prefix) {
  return strncmp(line, prefix, strlen(prefix)) == 0;
}

// Trim leading/trailing whitespace in-place. Returns a pointer into
// `buf` past any leading whitespace, and writes a NUL at the last
// non-whitespace char + 1.
static char *cd_trim(char *buf) {
  while (*buf && isspace((unsigned char)*buf)) buf++;
  int32_t len = (int32_t)strlen(buf);
  while (len > 0 && isspace((unsigned char)buf[len - 1])) {
    buf[--len] = '\0';
  }
  return buf;
}

// Iterate over the newline-separated lines of `text`, calling `visit`
// on each trimmed line. Trims in-place inside a scratch buffer.
static void cd_for_each_line(const char *text, void (*visit)(char *line, void *cookie), void *cookie) {
  const char *p = text;
  char buf[512];
  while (*p) {
    const char *nl = strchr(p, '\n');
    int32_t len = nl ? (int32_t)(nl - p) : (int32_t)strlen(p);
    if (len >= (int32_t)sizeof(buf)) len = (int32_t)sizeof(buf) - 1;
    memcpy(buf, p, (size_t)len);
    buf[len] = '\0';
    // Strip trailing \r too (Windows-style line endings).
    if (len > 0 && buf[len - 1] == '\r') buf[len - 1] = '\0';
    char *trimmed = cd_trim(buf);
    visit(trimmed, cookie);
    if (!nl) break;
    p = nl + 1;
  }
}

typedef struct {
  int32_t array[5];    // stat(...)
  int32_t array2[11];  // physics(...) fields 0..10
  int32_t array3[3];   // physics(...) fields 11..13
  int32_t engineSig;   // physics(...) field 14
  float n6;            // physics(...) field 15 (maxmag base)
  int32_t handling;    // handling(...) field 0, or -1 if absent
  int32_t sum_n5;      // sum of array[0..4] after clamp -- becomes the "class total"
  bool have_stat;      // b in the JS
  bool have_physics;   // b2 in the JS
} CdLoadstatParse;

static void cd_parse_visit(char *line, void *cookie) {
  CdLoadstatParse *p = (CdLoadstatParse *)cookie;
  if (cd_line_starts_with(line, "stat(")) {
    int32_t sum = 0;
    bool ok = true;
    int32_t tmp[5];
    for (int32_t i = 0; i < 5; i++) {
      bool found;
      int32_t v = cd_getvalue("stat", line, i, &found);
      if (!found) { ok = false; break; }
      if (v > 200) v = 200;
      if (v < 16) v = 16;
      tmp[i] = v;
      sum = sum + v;
    }
    if (ok) {
      for (int32_t i = 0; i < 5; i++) p->array[i] = tmp[i];
      p->sum_n5 = sum;
      p->have_stat = true;
    }
  } else if (cd_line_starts_with(line, "physics(")) {
    int32_t tmp2[11];
    int32_t tmp3[3];
    int32_t sig = 0;
    float sixteenth = 0.0f;
    bool ok = true;
    for (int32_t j = 0; j < 11; j++) {
      bool found;
      int32_t v = cd_getvalue("physics", line, j, &found);
      if (!found) { ok = false; break; }
      if (v > 100) v = 100;
      if (v < 0) v = 0;
      tmp2[j] = v;
    }
    if (ok) {
      for (int32_t k = 0; k < 3; k++) {
        bool found;
        int32_t v = cd_getvalue("physics", line, k + 11, &found);
        if (!found) { ok = false; break; }
        if (k != 0 && v > 100) v = 100;
        if (v < 0) v = 0;
        tmp3[k] = v;
      }
    }
    if (ok) {
      bool found;
      sig = cd_getvalue("physics", line, 14, &found);
      if (!found || sig > 4 || sig < 0) sig = 0;
      int32_t maxmag_base = cd_getvalue("physics", line, 15, &found);
      if (!found) { ok = false; }
      else {
        // fr(getvalue(...)) -- int-to-float, exact for small ints.
        sixteenth = (float)maxmag_base;
      }
    }
    if (ok && sixteenth > 0.0f) {
      for (int32_t j = 0; j < 11; j++) p->array2[j] = tmp2[j];
      for (int32_t k = 0; k < 3; k++) p->array3[k] = tmp3[k];
      p->engineSig = sig;
      p->n6 = sixteenth;
      p->have_physics = true;
    }
  } else if (cd_line_starts_with(line, "handling(")) {
    bool found;
    int32_t v = cd_getvalue("handling", line, 0, &found);
    if (found) {
      if (v > 200) v = 200;
      if (v < 50) v = 50;
      p->handling = v;
    }
  }
}

// Ports the "which two built-in cars keyframe this array[0] value" ladder
// (JS lines 242-299). Returns (n12, n13, n14) via out-params; n14 stays
// 0.5f for the exact-match cases. Matches JS exactly, including the
// order the `if` clauses are written -- some clauses `if (array[0] ===
// N)` overlap with the surrounding range clauses (JS falls through to
// the last-set values in that case).
static void cd_swits_keyframes(int32_t arr0, int32_t *out_n12, int32_t *out_n13, float *out_n14) {
  int32_t n12 = 0, n13 = 0;
  float n14 = 0.5f;
  if (arr0 == 200) { n12 = 1; n13 = 1; }
  if (arr0 > 192 && arr0 < 200) { n12 = 12; n13 = 1; n14 = (float)(arr0 - 192) / 8.0f; }
  if (arr0 == 192) { n12 = 12; n13 = 12; }
  if (arr0 > 148 && arr0 < 192) { n12 = 14; n13 = 12; n14 = (float)(arr0 - 148) / 44.0f; }
  if (arr0 == 148) { n12 = 14; n13 = 14; }
  if (arr0 > 133 && arr0 < 148) { n12 = 10; n13 = 14; n14 = (float)(arr0 - 133) / 15.0f; }
  if (arr0 == 133) { n12 = 10; n13 = 10; }
  if (arr0 > 112 && arr0 < 133) { n12 = 15; n13 = 10; n14 = (float)(arr0 - 112) / 21.0f; }
  if (arr0 == 112) { n12 = 15; n13 = 15; }
  if (arr0 > 107 && arr0 < 112) { n12 = 11; n13 = 15; n14 = (float)(arr0 - 107) / 5.0f; }
  if (arr0 == 107) { n12 = 11; n13 = 11; }
  if (arr0 > 88 && arr0 < 107) { n12 = 13; n13 = 11; n14 = (float)(arr0 - 88) / 19.0f; }
  if (arr0 == 88) { n12 = 13; n13 = 13; }
  *out_n12 = n12; *out_n13 = n13; *out_n14 = n14;
}

// Ports the array[1] -> acelf keyframe ladder (JS lines 313-402).
static void cd_acelf_keyframes(int32_t arr1, int32_t arr0, int32_t *out_n16, int32_t *out_n17, float *out_n18) {
  int32_t n16 = 0, n17 = 0;
  float n18 = 0.5f;
  if (arr1 == 200) { n16 = 1; n17 = 1; }
  if (arr1 > 150 && arr1 < 200) { n16 = 14; n17 = 1; n18 = (float)(arr1 - 150) / 50.0f; }
  if (arr1 == 150) { n16 = 14; n17 = 14; }
  if (arr1 > 144 && arr1 < 150) { n16 = 9; n17 = 14; n18 = (float)(arr1 - 144) / 6.0f; }
  if (arr1 == 144) { n16 = 9; n17 = 9; }
  if (arr1 > 139 && arr1 < 144) { n16 = 6; n17 = 9; n18 = (float)(arr1 - 139) / 5.0f; }
  if (arr1 == 139) { n16 = 6; n17 = 6; }
  if (arr1 > 128 && arr1 < 139) { n16 = 15; n17 = 6; n18 = (float)(arr1 - 128) / 11.0f; }
  if (arr1 == 128) { n16 = 15; n17 = 15; }
  if (arr1 > 122 && arr1 < 128) { n16 = 10; n17 = 15; n18 = (float)(arr1 - 122) / 6.0f; }
  if (arr1 == 122) { n16 = 10; n17 = 10; }
  if (arr1 > 119 && arr1 < 122) { n16 = 3; n17 = 10; n18 = (float)(arr1 - 119) / 3.0f; }
  if (arr1 == 119) { n16 = 3; n17 = 3; }
  if (arr1 > 98 && arr1 < 119) { n16 = 5; n17 = 3; n18 = (float)(arr1 - 98) / 21.0f; }
  if (arr1 == 98) { n16 = 5; n17 = 5; }
  if (arr1 > 81 && arr1 < 98) { n16 = 0; n17 = 5; n18 = (float)(arr1 - 81) / 17.0f; }
  if (arr1 == 81) { n16 = 0; n17 = 0; }
  if (arr1 <= 80) { n16 = 2; n17 = 2; }
  if (arr0 <= 88) { n16 = 13; n17 = 13; }
  *out_n16 = n16; *out_n17 = n17; *out_n18 = n18;
}

bool car_define_loadstat(CarDefine *cd, const char *text, int32_t maxR, int32_t roofat, int32_t wh, int32_t slot) {
  CdLoadstatParse p;
  memset(&p, 0, sizeof(p));
  for (int32_t i = 0; i < 5; i++) p.array[i] = 128;
  p.sum_n5 = 640;
  for (int32_t j = 0; j < 11; j++) p.array2[j] = 50;
  for (int32_t k = 0; k < 3; k++) p.array3[k] = 50;
  p.handling = -1;
  cd->enginsignature[slot] = 0;

  cd_for_each_line(text, cd_parse_visit, &p);

  if (p.have_stat) {
    // JS updates enginsignature/handling/n6 as it parses (assigned to
    // `this.enginsignature[n4]`/`this.dishandle[n4]` mid-scan). We just
    // apply the parsed values in one place here since the JS's own
    // in-loop assignment is harmless for a straight-line parse.
    cd->enginsignature[slot] = p.engineSig;
  }
  if (p.handling >= 0) {
    cd->dishandle[slot] = (float)p.handling / 200.0f;
  }
  if (!(p.have_stat && p.have_physics)) {
    // JS: `this.names[n4] = '';` -- we don't track names, so nothing to
    // do here. The unmodified tables in slot `slot` stay whatever
    // car_define_init put there (or all zeros).
    return false;
  }

  // Normalize the 5 stats to snap to the nearest class total (520, 560,
  // 600, 640, or 680). The JS reads its own l via a chain of `if
  // (n5 > X && n5 < Y)` clauses that are mutually exclusive except at
  // the exact boundary; a straight-line C version does the same.
  int32_t n5 = p.sum_n5;
  int32_t l = 0;
  if (n5 > 680) l = 680 - n5;
  if (n5 > 640 && n5 < 680) l = 640 - n5;
  if (n5 > 600 && n5 < 640) l = 600 - n5;
  if (n5 > 560 && n5 < 600) l = 560 - n5;
  if (n5 > 520 && n5 < 560) l = 520 - n5;
  if (n5 < 520) l = 520 - n5;
  while (l != 0) {
    for (int32_t n7 = 0; n7 < 5; n7++) {
      if (l > 0 && p.array[n7] < 200) { p.array[n7] = p.array[n7] + 1; l = l - 1; }
      if (l < 0 && p.array[n7] > 16) { p.array[n7] = p.array[n7] - 1; l = l + 1; }
    }
  }
  int32_t n10 = 0;
  for (int32_t n11 = 0; n11 < 5; n11++) n10 = n10 + p.array[n11];
  if (n10 == 520) cd->cclass[slot] = 0;
  if (n10 == 560) cd->cclass[slot] = 1;
  if (n10 == 600) cd->cclass[slot] = 2;
  if (n10 == 640) cd->cclass[slot] = 3;
  if (n10 == 680) cd->cclass[slot] = 4;

  int32_t n12, n13; float n14;
  cd_swits_keyframes(p.array[0], &n12, &n13, &n14);
  if (p.array[0] > 88) {
    // fr(fr((swits[n13][i] - swits[n12][i]) * n14) + swits[n12][i])
    // -- inner fr(int_diff * float) case 1, outer fr(float + int) case 1.
    for (int32_t i = 0; i < 3; i++) {
      float diff = (float)(cd->swits[n13][i] - cd->swits[n12][i]);
      cd->swits[slot][i] = jtrunc(diff * n14 + (float)cd->swits[n12][i]);
    }
  } else {
    float n15 = (float)p.array[0] / 88.0f;
    if (n15 < 0.76f) n15 = 0.76f;
    cd->swits[slot][0] = jtrunc(50.0f * n15);
    cd->swits[slot][1] = jtrunc(130.0f * n15);
    cd->swits[slot][2] = jtrunc(210.0f * n15);
  }

  int32_t n16, n17; float n18;
  cd_acelf_keyframes(p.array[1], p.array[0], &n16, &n17, &n18);
  for (int32_t i = 0; i < 3; i++) {
    // fr(fr((acelf[n17][i] - acelf[n16][i]) * n18) + acelf[n16][i])
    // -- acelf is float, so inner is float*float (case 1) and outer is
    // float+float (case 1). Result stored as float directly.
    float diff = cd->acelf[n17][i] - cd->acelf[n16][i];
    cd->acelf[slot][i] = diff * n18 + cd->acelf[n16][i];
  }
  if (p.array[1] <= 70 && p.array[0] > 88) {
    cd->acelf[slot][0] = 9.0f;
    cd->acelf[slot][1] = 4.0f;
    cd->acelf[slot][2] = 3.0f;
  }

  // n19 = fr(fr(array[2]-88) / 109.0). int-diff cast to float (exact),
  // then / 109.0 case 1.
  float n19 = (float)(p.array[2] - 88) / 109.0f;
  if (n19 > 1.0f) n19 = 1.0f;
  if (n19 < -0.55f) n19 = -0.55f;

  // airs = fr(fr(0.55 + fr(0.45 * n19)) + fr(0.4 * (array2[9]/100.0)))
  // Each layer is a single float op. All case 1.
  float airs_part1 = 0.55f + 0.45f * n19;
  float airs_part2 = 0.4f * ((float)p.array2[9] / 100.0f);
  cd->airs[slot] = airs_part1 + airs_part2;
  if (cd->airs[slot] < 0.3f) cd->airs[slot] = 0.3f;

  // airc = trunc(fr(10.0 + fr(70.0 * n19)) + fr(30.0 * (array2[10]/100.0)))
  float airc_part1 = 10.0f + 70.0f * n19;
  float airc_part2 = 30.0f * ((float)p.array2[10] / 100.0f);
  cd->airc[slot] = jtrunc(airc_part1 + airc_part2);
  if (cd->airc[slot] < 0) cd->airc[slot] = 0;

  // powerloss: two possible formulas depending on array[0] (top-tier
  // cars use the 670-based one; slow cars use the 1670-based one for
  // much slower power decay). Then Math.imul by 10000 (int mult, wraps).
  int32_t n20;
  {
    // trunc(fr(670.0 - fr((array2[9]+array2[10])/200.0 * 420.0)))
    // The JS's fr(fr(fr(x/y)*z)) is a chain of single-op fr()s but
    // (array2[9]+array2[10])/200.0 in JS is int/double = one op wrapped
    // in fr = case 1 exact for small values. Then * 420.0 case 1.
    // Then 670.0 - ... case 1. All native float.
    float phys_sum_over_200 = (float)(p.array2[9] + p.array2[10]) / 200.0f;
    n20 = jtrunc(670.0f - phys_sum_over_200 * 420.0f);
  }
  if (p.array[0] <= 88) {
    float phys_sum_over_200 = (float)(p.array2[9] + p.array2[10]) / 200.0f;
    n20 = jtrunc(1670.0f - phys_sum_over_200 * 1420.0f);
  }
  if (p.array[2] > 190 && n20 < 300) n20 = 300;
  cd->powerloss[slot] = n20 * 10000; // Math.imul-equivalent under -fwrapv

  // moment: two formulas depending on array[0] < 110.
  {
    float base = 0.7f;
    float scale = 1.0f;
    if (p.array[0] < 110) { base = 0.75f; scale = 1.25f; }
    cd->moment[slot] = base + ((float)(p.array[3] - 16) / 184.0f) * scale;
    if (p.array[3] == 200 && p.array[4] == 200 && p.array[0] <= 88) cd->moment[slot] = 3.0f;
  }

  // n21 -> maxmag, outdam. n21 = fr(0.9 + fr((array[4]-90) * 0.01))
  float n21 = 0.9f + (float)(p.array[4] - 90) * 0.01f;
  if (n21 < 0.6f) n21 = 0.6f;
  if (p.array[4] == 200 && p.array[0] <= 88) n21 = 3.0f;
  cd->maxmag[slot] = jtrunc(p.n6 * n21);
  cd->outdam[slot] = 0.35f + (n21 - 0.6f) * 0.5f;
  if (cd->outdam[slot] < 0.35f) cd->outdam[slot] = 0.35f;
  if (cd->outdam[slot] > 1.0f) cd->outdam[slot] = 1.0f;

  // clrad = trunc(fr(fr(array3[0] * array3[0]) * 1.5))
  {
    int32_t sq = p.array3[0] * p.array3[0]; // int mult, small
    cd->clrad[slot] = jtrunc((float)sq * 1.5f);
    if (cd->clrad[slot] < 1000) cd->clrad[slot] = 1000;
  }
  cd->dammult[slot] = 0.3f + (float)p.array3[1] * 0.005f;
  cd->msquash[slot] = jtrunc_d(2.0 + (double)p.array3[2] / 7.6);
  cd->flipy[slot] = roofat;

  cd->handb[slot] = jtrunc(7.0f + ((float)p.array2[0] / 100.0f) * 8.0f);
  cd->turn[slot] = jtrunc(4.0f + ((float)p.array2[1] / 100.0f) * 6.0f);
  cd->grip[slot] = 16.0f + ((float)p.array2[2] / 100.0f) * 14.0f;
  if (cd->grip[slot] < 21.0f) {
    // Compound-assign rewrite exactly per the JS's own §2 note:
    // swits[slot][0] = trunc(fr(swits[slot][0] + fr(40.0 * fr((21.0 - grip) / 5.0))))
    float scaled = 40.0f * ((21.0f - cd->grip[slot]) / 5.0f);
    cd->swits[slot][0] = jtrunc((float)cd->swits[slot][0] + scaled);
    if (cd->swits[slot][0] > 100) cd->swits[slot][0] = 100;
  }
  cd->bounce[slot] = 0.8f + ((float)p.array2[3] / 100.0f) * 0.6f;
  if (p.array2[3] > 67) {
    float damp = 0.76f + (1.0f - (float)p.array2[3] / 100.0f) * 0.24f;
    cd->airs[slot] = cd->airs[slot] * damp;
    // airc compound-assign: JS multiplies airc by fr(damp), then
    // truncates the resulting int*float to int. Case 3 shape.
    cd->airc[slot] = jtrunc((float)cd->airc[slot] * damp);
  }

  // lift = trunc(fr(fr((array2[5]^2) / 10000.0) * 30.0))
  cd->lift[slot] = jtrunc(((float)(p.array2[5] * p.array2[5]) / 10000.0f) * 30.0f);
  cd->revlift[slot] = jtrunc(((float)p.array2[6] / 100.0f) * 32.0f);
  // push = trunc(fr(2.0 + fr(fr(array2[7]/100.0 * 2.0) * idiv(30-lift, 30))))
  {
    int32_t lift_slot = cd->lift[slot];
    int32_t idiv_part = (30 - lift_slot) / 30; // Java idiv: truncates toward zero
    float push_scaled = (((float)p.array2[7] / 100.0f) * 2.0f) * (float)idiv_part;
    cd->push[slot] = jtrunc(2.0f + push_scaled);
  }
  cd->revpush[slot] = jtrunc(1.0f + ((float)p.array2[8] / 100.0f) * 2.0f);

  // comprad = fr(maxR/400.0 + fr((array[3]-16)/184.0) * 0.2)
  cd->comprad[slot] = (float)maxR / 400.0f + ((float)(p.array[3] - 16) / 184.0f) * 0.2f;
  if (cd->comprad[slot] < 0.4f) cd->comprad[slot] = 0.4f;

  // simag = fr((wh - 17) * 0.0167 + 0.85)
  cd->simag[slot] = (float)(wh - 17) * 0.0167f + 0.85f;

  return true;
}

bool car_define_loadcar(CarDefine *cd, const char *text, const ContO *co, int32_t maxR, int32_t roofat, int32_t wh, int32_t slot) {
  // Wheel-corner validation (JS loadcar lines 728-743): the four keyx/
  // keyz corners must be in the expected quadrants (rear-left/
  // rear-right/front-right/front-left order, given the sign convention
  // z-forward, x-right). Any deviation invalidates the load.
  if (co->keyz[0] < 0 || co->keyx[0] > 0) return false;
  if (co->keyz[1] < 0 || co->keyx[1] < 0) return false;
  if (co->keyz[2] > 0 || co->keyx[2] > 0) return false;
  if (co->keyz[3] > 0 || co->keyx[3] < 0) return false;
  return car_define_loadstat(cd, text, maxR, roofat, wh, slot);
}

// Extended Mode's retuned NFM 2 cars (Madness.java:404-435, its cars 23-38,
// which are NFM 2's 0-15): only the values that differ from NFM 2's own.
// The extended build applies it once at boot; the tests keep NFM 2's table.
void car_define_extended(CarDefine *cd) {
  cd->grip[6] = 22.0f;
  cd->grip[8] = 16.0f;
  cd->grip[10] = 22.4f;
  cd->grip[14] = 30.0f;
  cd->grip[15] = 24.0f;
  cd->bounce[3] = 1.05f;
  cd->bounce[10] = 1.1f;
  cd->moment[0] = 1.2f;
  cd->moment[3] = 1.0f;
  cd->moment[5] = 1.25f;
  cd->moment[6] = 1.4f;
  cd->moment[7] = 1.3f;
  cd->moment[8] = 1.2f;
  cd->moment[9] = 1.45f;
  cd->moment[10] = 1.375f;
  cd->moment[12] = 1.2f;
  cd->comprad[4] = 0.3f;
  cd->comprad[11] = 1.0f;
  cd->comprad[13] = 0.6f;
  cd->revpush[2] = 1.0f;
  cd->revpush[14] = 0.25f;
  cd->revpush[15] = 0.4f;
  cd->revlift[15] = 15;
  cd->msquash[7] = 2;
  cd->clrad[1] = 2500;
  cd->clrad[9] = 9500;
  cd->clrad[13] = 500000;
  cd->clrad[15] = 4200;
  cd->dammult[0] = 0.8f;
  cd->dammult[1] = 1.0f;
  cd->dammult[2] = 0.55f;
  cd->dammult[3] = 1.0f;
  cd->dammult[4] = 0.6f;
  cd->dammult[7] = 0.8f;
  cd->dammult[8] = 0.6f;
  cd->dammult[9] = 0.46f;
  cd->dammult[10] = 0.6f;
  cd->dammult[11] = 0.48f;
  cd->dammult[12] = 0.6f;
  cd->dammult[13] = 0.2f;
  cd->dammult[14] = 0.3f;
  cd->dammult[15] = 0.46f;
  cd->maxmag[0] = 6000;
  cd->maxmag[5] = 9100;
  cd->maxmag[6] = 14000;
  cd->maxmag[7] = 12000;
  cd->maxmag[8] = 12000;
  cd->maxmag[9] = 9700;
  cd->maxmag[10] = 13000;
  cd->maxmag[13] = 63000;
  cd->swits[0][1] = 180;
  cd->swits[0][2] = 280;
  cd->swits[3][0] = 70;
  cd->swits[3][1] = 200;
  cd->swits[3][2] = 295;
  cd->swits[5][0] = 60;
  cd->swits[5][1] = 200;
  cd->swits[5][2] = 290;
  cd->swits[6][2] = 280;
  cd->swits[7][0] = 60;
  cd->swits[7][1] = 180;
  cd->swits[7][2] = 280;
  cd->acelf[6][0] = 9.0f;
  cd->acelf[6][1] = 7.0f;
}
