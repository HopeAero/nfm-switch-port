// ports web/CheckPoints.js -- see check_points.h for scope.
#include "check_points.h"
#include "java_compat.h"
#include "trackers.h"
#include "mad.h"
#include "cont_o.h"
#include "record.h"
#include <stdlib.h>
#include <string.h>

void check_points_init(CheckPoints *cp) {
  memset(cp, 0, sizeof(*cp));
  cp->stage = jtrunc_d(nfm_random() * 27.0) + 1; // trunc(random()*27.0)+1, no fr(), genuinely double
  strncpy(cp->name, "hogan rewish", sizeof(cp->name) - 1);
  cp->trackvol = 200;
  for (int32_t i = 0; i < 8; i++) cp->pos[i] = 7;
}

void check_points_calprox(CheckPoints *cp) {
  int32_t n = 0;
  for (int32_t i = 0; i < cp->n - 1; i++) {
    for (int32_t j = i + 1; j < cp->n; j++) {
      int32_t dx = abs(cp->x[i] - cp->x[j]);
      if (dx > n) n = dx;
      int32_t dz = abs(cp->z[i] - cp->z[j]);
      if (dz > n) n = dz;
    }
  }
  cp->prox = (float)n / 90.0f; // fr(n/90.0), single op, case 1, native float exact
}

int32_t check_points_py(int32_t n, int32_t n2, int32_t n3, int32_t n4) {
  return trackers_py(n, n2, n3, n4);
}

void check_points_checkstat(CheckPoints *cp, Mad **mads, ContO **contOs, Record *record,
                             int32_t n, int32_t n2, int32_t n3) {
  if (!cp->haltall) {
    cp->pcleared = mads[n2]->pcleared;
    for (int32_t i = 0; i < n; i++) {
      cp->magperc[i] = (float)mads[i]->hitmag / (float)mads[i]->cd->maxmag[mads[i]->cn];
      if (cp->magperc[i] > 1.0f) cp->magperc[i] = 1.0f;
      cp->pos[i] = 0;
      cp->onscreen[i] = contOs[i]->dist;
      cp->opx[i] = contOs[i]->x;
      cp->opz[i] = contOs[i]->z;
      cp->omxz[i] = mads[i]->mxz;
      if (cp->dested[i] == 0) cp->clear[i] = mads[i]->clear;
      else cp->clear[i] = -1;
      mads[i]->outshakedam = mads[i]->shakedam;
      mads[i]->shakedam = 0;
    }
    for (int32_t j = 0; j < n; j++) {
      for (int32_t k = j + 1; k < n; k++) {
        if (cp->clear[j] != cp->clear[k]) {
          if (cp->clear[j] < cp->clear[k]) cp->pos[j]++;
          else cp->pos[k]++;
        } else {
          int32_t n6 = mads[j]->pcleared + 1;
          if (n6 >= cp->n) n6 = 0;
          while (cp->typ[n6] <= 0) {
            if (++n6 >= cp->n) n6 = 0;
          }
          if (check_points_py(contOs[j]->x / 100, cp->x[n6] / 100, contOs[j]->z / 100, cp->z[n6] / 100) >
              check_points_py(contOs[k]->x / 100, cp->x[n6] / 100, contOs[k]->z / 100, cp->z[n6] / 100)) {
            cp->pos[j]++;
          } else {
            cp->pos[k]++;
          }
        }
      }
    }
    if (cp->stage > 2) {
      for (int32_t l = 0; l < n; l++) {
        if (cp->clear[l] == cp->nlaps * cp->nsp && cp->pos[l] == 0) {
          if (l == n2) {
            for (int32_t postwo = 0; postwo < n; postwo++) {
              if (cp->pos[postwo] == 1) cp->postwo = postwo;
            }
            if (check_points_py(cp->opx[n2] / 100, cp->opx[cp->postwo] / 100, cp->opz[n2] / 100, cp->opz[cp->postwo] / 100) < 14000 &&
                cp->clear[n2] - cp->clear[cp->postwo] == 1) {
              cp->catchfin = 30;
            }
          } else if (cp->pos[n2] == 1 &&
                     check_points_py(cp->opx[n2] / 100, cp->opx[l] / 100, cp->opz[n2] / 100, cp->opz[l] / 100) < 14000 &&
                     cp->clear[l] - cp->clear[n2] == 1) {
            cp->catchfin = 30;
            cp->postwo = l;
          }
        }
      }
    }
  }
  cp->wasted = 0;
  for (int32_t n9 = 0; n9 < n; n9++) {
    if ((n2 != n9 || n3 >= 2) && mads[n9]->dest) cp->wasted++;
  }
  if (cp->catchfin != 0 && n3 < 2) {
    cp->catchfin--;
    if (cp->catchfin == 0) {
      record_cotchinow(record, cp->postwo);
      record->closefinish = cp->pos[n2] + 1;
    }
  }
}
