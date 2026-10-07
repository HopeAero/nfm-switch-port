// Engine-wide limits shared by several modules.
#ifndef NFM_LIMITS_H
#define NFM_LIMITS_H

// Cars in one race, the player included. NFM 2 races 7 (NFM 1 five);
// Extended's normal mode races 11 and its career up to 20. Every per-car
// array (Mad, CheckPoints, Record, the HUD, game.c's race slots) is sized by
// this; how many actually race is the race setup's `nplayers`.
#define NFM_MAX_CARS 20

#endif
