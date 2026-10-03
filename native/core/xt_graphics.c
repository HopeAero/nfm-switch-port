// M2 stub for web/XtGraphics.js -- see xt_graphics.h. Part 16 adds a real
// port of the physics-triggered SFX state machine (crash/skid/scrape/
// gscrape/sparkeng/#playsounds), so this file now #includes mad.h (pulls
// in Medium/Control/CarDefine transitively) despite staying otherwise a
// data-only stub for the rest of XtGraphics.js.
#include "xt_graphics.h"
#include "mad.h"
#include "java_compat.h"
#include <string.h>
#include <math.h>

void xt_graphics_stub_init(XtGraphicsStub *xt) {
  memset(xt, 0, sizeof(*xt));
  xt->auscnt = 45; // matches XtGraphics.js's own constructor default -- see xt_graphics.h
  // Matches XtGraphics.js's own resetstat(), called once per stage load
  // (ana/cntan/cntovn/clear already 0 from the memset above; tcnt/wasay
  // still spelled out for clarity since 30/false aren't the zero value).
  xt->tcnt = 30;
  xt->wasay = false;
  xt->pwcnt = 0;
  // -1 == "no car manually locked" (xtGraphics.java:598's own default, and
  // what :3690-3691 resets them to when a locked car gets wasted). Spelled
  // out here for the same reason tcnt/wasay are: -1 is not the zero value
  // the memset above leaves behind, and arrow()'s `alocked == -1` test
  // reads it directly. See xt_graphics.h's own field comment.
  xt->alocked = -1;
  xt->lalocked = -1;
  // XtGraphics.js's constructor default (line 27), never touched by
  // resetstat() itself -- in real Java/JS this field persists across
  // races within a session (xtGraphics is a session-long singleton);
  // this port combines constructor+resetstat into one per-race init (see
  // this function's own established pattern for tcnt/wasay above), so
  // set it here rather than let the memset above zero it.
  xt->looped = 1;
  // bfcrash/bfskid/bfscrape/bfsc1/bfsc2/cntwis/pwait/stopcnt/grrd/aird/
  // pwastd/pengs[]/lcn all correctly default to 0/false from the memset
  // above, matching XtGraphics.js's own constructor zeroes for all of
  // these (see xt_graphics.h's own field comments) -- resetstat() itself
  // additionally zeroes bfcrash/bfscrape/cntwis/bfskid/pwait/forstart
  // (xtGraphics.java:1495-1499), all already 0 here, so nothing extra to
  // spell out.
}

bool xt_graphics_stub_human(const XtGraphicsStub *xt, int32_t n) {
  // XtGraphics.js's real human(i): `this.humans ? this.humans.has(i) :
  // i === this.im` -- a netplay-lobby `humans` Set (populated from
  // server player-list data, not ported -- see this file's header
  // comment) that lets a netplay client know which OTHER clients' cars
  // are human-driven too. This stub never populates one (out of scope,
  // same as the rest of the netplay lobby), so the JS's ternary always
  // takes its `i === this.im` branch here: "human" means "the car I'm
  // actually looking at/controlling", nothing to do with `isbot`. Was
  // `!xt->isbot[n]` until a two-car mad_colide() scenario caught the
  // divergence (native/tests/mad_test.c's colide_scenario) -- dormant
  // until then because every scenario before it only ever drove ONE car
  // at im === xt.im, where both formulas happen to agree.
  return n == xt->im;
}

// Ports `#crash(a, n)` (xtGraphics.java:9081ish / web/XtGraphics.js:450-482).
// `n` selects the case: 0 = ordinary collision (rotates through 3 crash/
// lowcrash variants via crshturn), -1 = wall/scenery scrape-collision
// (always variant 3), 1 = tire-screech ("tires" one-shot, e.g. landing
// hard on all 4 wheels). `crashup` (toggled by the "Car Fixed" trigger in
// main.c's hud_messages_tick) picks which DIRECTION crshturn rotates, so
// consecutive crashes don't always cycle the same way.
void xt_graphics_stub_crash(XtGraphicsStub *xt, float mag, int32_t n2) {
  if (xt->bfcrash != 0) return;
  float absa = fabsf(mag);
  if (n2 == 0) {
    if (absa > 25.0f && absa < 170.0f) {
      xt->pending_crash = XT_CRASH_LOWCRASH1 + xt->crshturn;
      xt->bfcrash = 2;
    }
    if (absa >= 170.0f) {
      xt->pending_crash = XT_CRASH_CRASH1 + xt->crshturn;
      xt->bfcrash = 2;
    }
    if (absa > 25.0f) {
      if (xt->crashup) xt->crshturn--;
      else xt->crshturn++;
      if (xt->crshturn == -1) xt->crshturn = 2;
      if (xt->crshturn == 3) xt->crshturn = 0;
    }
  }
  if (n2 == -1) {
    if (absa > 25.0f && absa < 170.0f) { xt->pending_crash = XT_CRASH_LOWCRASH3; xt->bfcrash = 2; }
    if (absa > 170.0f) { xt->pending_crash = XT_CRASH_CRASH3; xt->bfcrash = 2; }
  }
  if (n2 == 1) { xt->pending_crash = XT_CRASH_TIRES; xt->bfcrash = 3; }
}

// Ports `#skid(n, n2)` (web/XtGraphics.js:489-506). `n` selects skid
// (0, tire squeal) vs dustskid (else, off-road skid puff); `n2` is the
// lateral-slip magnitude threshold check. `skidup` (toggled by the stunt
// announcer, see main.c's hud_stunt_detect()) picks the rotation
// direction through the 3 variants, same "Car Fixed"-adjacent toggle
// pattern as crash()'s own crashup.
void xt_graphics_stub_skid(XtGraphicsStub *xt, int32_t n, float mag) {
  if (xt->bfcrash == 0 && xt->bfskid == 0 && mag > 150.0f) {
    if (n == 0) {
      xt->pending_skid = XT_SKID_SKID1 + xt->skflg;
      if (xt->skidup) xt->skflg--;
      else xt->skflg++;
      if (xt->skflg == 3) xt->skflg = 0;
      if (xt->skflg == -1) xt->skflg = 2;
    } else {
      xt->pending_skid = XT_SKID_DUSTSKID1 + xt->dskflg;
      if (xt->skidup) xt->dskflg--;
      else xt->dskflg++;
      if (xt->dskflg == 3) xt->dskflg = 0;
      if (xt->dskflg == -1) xt->dskflg = 2;
    }
    xt->bfskid = 5;
  }
}

// Ports `#scrape(n, n2, n3)` (web/XtGraphics.js:513-529) -- a chassis-
// vs-track-geometry scrape (n/n2/n3 are the scrape point's velocity
// components). Alternates between scrape1/scrape2 via a 3-tick-run
// counter pair (sturn0/sturn1) so the SAME variant doesn't repeat 3
// times running, itself picked by `this.m.random() > this.m.random()`
// (sequenced through named temporaries -- same left-to-right-eval
// pitfall this project's other rand_gt_rand() helpers already document).
void xt_graphics_stub_scrape(XtGraphicsStub *xt, int32_t x, int32_t y, int32_t z, Medium *m) {
  double mag = sqrt((double)x * (double)x + (double)y * (double)y + (double)z * (double)z) / 10.0;
  if (xt->bfscrape == 0 && mag > 10.0) {
    float r1 = medium_random(m);
    float r2 = medium_random(m);
    int32_t n4 = (r1 > r2) ? 1 : 0;
    if (n4 == 0) {
      xt->sturn1 = 0;
      xt->sturn0++;
      if (xt->sturn0 == 3) { n4 = 1; xt->sturn1 = 1; xt->sturn0 = 0; }
    } else {
      xt->sturn0 = 0;
      xt->sturn1++;
      if (xt->sturn1 == 3) { n4 = 0; xt->sturn0 = 1; xt->sturn1 = 0; }
    }
    xt->pending_scrape = XT_SCRAPE_SCRAPE1 + n4;
    xt->bfscrape = 5;
  }
}

// Ports `#gscrape(n, n2, n3)` (web/XtGraphics.js:536-552) -- a HARDER
// ground-scrape (higher magnitude threshold than scrape()), always
// scrape3.wav. The real Java loads scrape3.wav into TWO separate clip
// objects (`scrapeClips[2]`/`[3]`) purely so a fresh scrape can cut a
// still-ringing previous one on the OTHER key while it keeps playing
// (see the JS's own comment) -- both keys select the identical asset,
// so `pending_gscrape` doesn't need to distinguish them; the two-key
// alternation only matters for the debounce timing (bfsc1/bfsc2), which
// IS preserved here.
void xt_graphics_stub_gscrape(XtGraphicsStub *xt, int32_t x, int32_t y, int32_t z) {
  double mag = sqrt((double)x * (double)x + (double)y * (double)y + (double)z * (double)z) / 10.0;
  if ((xt->bfsc1 == 0 || xt->bfsc2 == 0) && mag > 15.0) {
    if (xt->bfsc1 == 0) {
      xt->pending_gscrape = true;
      xt->bfsc1 = 12;
      xt->bfsc2 = 6;
    } else {
      xt->pending_gscrape = true;
      xt->bfsc2 = 12;
      xt->bfsc1 = 6;
    }
  }
}

// Ports `sparkeng(n, lcn)` (web/XtGraphics.js:877-899) -- picks EXACTLY
// rev slot `n` (-1 = none looping) of car `lcn`'s engine bank as the one
// `xt->pengs[]` says should be looping now.
//
// Simplified from the JS's own imperative "stop old / start new only if
// not already looping" dance down to a direct `pengs[j] = (n===j)`
// assignment -- behaviorally identical in FINAL STATE (a `loop()` call on
// an already-looping name is a no-op in the original too, so what
// matters is only the end result, not how many redundant start/stop
// calls got skipped along the way) and simpler to reconcile against real
// mixer channels in main.c (which does its own idempotency check via
// per-slot channel tracking).
//
// The one behavior NOT collapsed away: when `lcn` changes (a different
// car's bank), every previously-looping channel on the OLD bank must
// stop even if the SAME slot index is about to be wanted again on the
// NEW bank -- the underlying WAV asset differs per bank. `xt->lcn` is
// updated unconditionally here so main.c can detect that edge itself
// (comparing against its own remembered previous bank) and force-stop
// all 5 engine channels before reconciling pengs[] against the new one.
void xt_graphics_stub_sparkeng(XtGraphicsStub *xt, int32_t n, int32_t lcn) {
  xt->lcn = lcn;
  n++;
  for (int32_t j = 0; j < 5; j++) {
    xt->pengs[j] = (n == j);
  }
}

// Ports `#playsounds(mad, control, n)` (web/XtGraphics.js:659-832) -- see
// xt_graphics.h's own doc comment for the call-site/timing contract. The
// JS's own `this.fase === 0 || this.fase === 7001` outer gate is
// simplified away to always-true: main.c only ever calls this during
// STATE_RACING, the ONLY state that fase pair covers in a single-player
// session (see this port's own fase-to-GameState mapping, MENU_FLOW.md).
void xt_graphics_stub_playsounds(XtGraphicsStub *xt, Mad *mad, Control *control, Medium *m, int32_t starcnt) {
  // This tick's fresh decisions -- see xt_graphics.h's own field comment
  // on why air_stop_all/air_start_slot/pending_firewasted reset HERE
  // (this function is the only place that sets them, called exactly
  // once per tick), unlike pending_crash/skid/scrape/gscrape which reset
  // in main.c instead (crash()/skid()/scrape()/gscrape() can each be
  // called from multiple mad.c sites within the same tick).
  xt->air_stop_all = false;
  xt->air_start_slot = -1;
  xt->pending_firewasted = false;

  if (starcnt < 35 && xt->cntwis != 8 && !xt->mutes) {
    bool b = (control->up && mad->speed > 0.0f) || (control->down && mad->speed < 10.0f);
    float scz_avg = (((mad->scz[1] + mad->scz[0]) + mad->scz[2]) + mad->scz[3]) / 4.0f;
    float scx_avg = (((mad->scx[1] + mad->scx[0]) + mad->scx[2]) + mad->scx[3]) / 4.0f;
    bool b2 = (mad->skid == 1 && control->handb)
        || fabsf(mad->scz[0] - scz_avg) > 1.0f
        || fabsf(mad->scx[0] - scx_avg) > 1.0f;
    bool b3 = false;
    if (control->up && mad->speed < 10.0f) { b2 = true; b = true; b3 = true; }

    if (b && mad->mtouch) {
      if (!mad->capsized) {
        if (!b2) {
          float absSpeed = fabsf(mad->speed);
          if (mad->power != 98.0f) {
            // Three rev bands, split at the car's own gear-change points
            // (swits[cn][0..2]). Within a band the sample index is the
            // fraction of the way through it; `pwait` holds the top slot
            // for a few ticks so the engine doesn't chatter between two
            // samples right at a threshold.
            if (absSpeed > 0.0f && absSpeed <= (float)mad->cd->swits[mad->cn][0]) {
              int32_t n2 = jtrunc((3.0f * absSpeed) / (float)mad->cd->swits[mad->cn][0]);
              if (n2 == 2) {
                if (xt->pwait == 0) n2 = 0;
                else xt->pwait--;
              } else {
                xt->pwait = 7;
              }
              xt_graphics_stub_sparkeng(xt, n2, mad->cn);
            }
            if (absSpeed > (float)mad->cd->swits[mad->cn][0] && absSpeed <= (float)mad->cd->swits[mad->cn][1]) {
              int32_t n3 = jtrunc((3.0f * (absSpeed - (float)mad->cd->swits[mad->cn][0]))
                                   / (float)(mad->cd->swits[mad->cn][1] - mad->cd->swits[mad->cn][0]));
              if (n3 == 2) {
                if (xt->pwait == 0) n3 = 0;
                else xt->pwait--;
              } else {
                xt->pwait = 7;
              }
              xt_graphics_stub_sparkeng(xt, n3, mad->cn);
            }
            if (absSpeed > (float)mad->cd->swits[mad->cn][1] && absSpeed <= (float)mad->cd->swits[mad->cn][2]) {
              int32_t n_ = jtrunc((3.0f * (absSpeed - (float)mad->cd->swits[mad->cn][1]))
                                   / (float)(mad->cd->swits[mad->cn][2] - mad->cd->swits[mad->cn][1]));
              xt_graphics_stub_sparkeng(xt, n_, mad->cn);
            }
          } else {
            int32_t n4 = 2;
            if (xt->pwait == 0) {
              if (absSpeed > (float)mad->cd->swits[mad->cn][1]) n4 = 3;
            } else {
              xt->pwait--;
            }
            xt_graphics_stub_sparkeng(xt, n4, mad->cn);
          }
        } else {
          // Wheelspin: the engine drops out and a tyre-squeal air sample
          // takes over.
          xt_graphics_stub_sparkeng(xt, -1, mad->cn);
          if (b3) {
            if (xt->stopcnt <= 0) { xt->air_start_slot = 5; xt->stopcnt = 10; }
          } else if (xt->stopcnt <= -2) {
            xt->air_start_slot = 2 + jtrunc(medium_random(m) * 3.0f);
            xt->stopcnt = 7;
          }
        }
      } else {
        xt_graphics_stub_sparkeng(xt, 3, mad->cn);
      }
      xt->grrd = false;
      xt->aird = false;
    } else {
      xt->pwait = 15;
      if (!mad->mtouch && !xt->grrd && medium_random(m) > 0.4f) {
        xt->air_start_slot = jtrunc(medium_random(m) * 4.0f);
        xt->stopcnt = 5;
        xt->grrd = true;
      }
      if (!mad->wtouch && !xt->aird) {
        // Supersedes any grrd-picked slot just above within this SAME
        // call -- see xt_graphics.h's own doc comment on why later
        // writes here correctly override earlier ones.
        xt->air_stop_all = true;
        xt->air_start_slot = jtrunc(medium_random(m) * 4.0f);
        xt->stopcnt = 10;
        xt->aird = true;
      }
      xt_graphics_stub_sparkeng(xt, -1, mad->cn);
    }

    if (mad->cntdest != 0 && xt->cntwis < 7) {
      if (!xt->pwastd) xt->pwastd = true;
    } else {
      if (xt->pwastd) xt->pwastd = false;
      if (xt->cntwis == 7 && !xt->mutes) xt->pending_firewasted = true;
    }
  } else {
    xt_graphics_stub_sparkeng(xt, -2, mad->cn);
    if (xt->pwastd) xt->pwastd = false;
  }

  if (xt->stopcnt != -20) {
    if (xt->stopcnt == 1) xt->air_stop_all = true;
    xt->stopcnt--;
  }

  // THE debounce decrements -- crash()/skid()/scrape()/gscrape() set
  // these and nothing else clears them.
  if (xt->bfcrash != 0) xt->bfcrash--;
  if (xt->bfscrape != 0) xt->bfscrape--;
  if (xt->bfsc1 != 0) xt->bfsc1--;
  if (xt->bfsc2 != 0) xt->bfsc2--;
  if (xt->bfskid != 0) xt->bfskid--;

  if (mad->newcar) xt->cntwis = 0;

  // Mutes/mutem sync from the live Control -- xtGraphics.java's own fase
  // gate around this (fase===0||7001||6||-1..-5, i.e. "anywhere in or
  // just after a race") always holds whenever this function is called
  // (STATE_RACING only), so it's simplified away like the outer gate
  // above. No key currently binds control->mutem/mutes (see
  // input_linux.h), so this stays dormant -- same "wired but
  // unreachable until a key exists" state as control->lookback
  // elsewhere in this port. `mutem`'s real effect (stopping/resuming
  // the background track) needs the audio backend this file stays
  // agnostic to -- main.c's tick loop diffs xt->mutem against its own
  // remembered previous value and acts on the edge, same pattern it
  // already uses for the engine-bank change above.
  if (xt->mutes != control->mutes) xt->mutes = control->mutes;
  if (control->mutem != xt->mutem) xt->mutem = control->mutem;

  if (mad->cntdest != 0 && xt->cntwis < 7) {
    if (mad->dest) xt->cntwis++;
  } else {
    if (mad->cntdest == 0) xt->cntwis = 0;
    if (xt->cntwis == 7) xt->cntwis = 8;
  }
}
