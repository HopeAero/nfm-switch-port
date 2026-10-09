# Top-level build for the native C port. Builds two static libraries --
# `nfm_core` (the platform-agnostic modules ported from web/*.js -- see
# PORT_SPEC.md) and `nfm_game` (the menu/asset/physics loop that USES
# nfm_core but isn't itself a web/*.js port, see platform/common/game.c's
# own top comment for why it's shared too, Part 18) -- then adds exactly
# one thin platform backend on top of both, picked by
# -DNFM_PLATFORM=linux|vita. That backend supplies four small files
# (main.c, input.c, audio.c, platform.c -- see platform/common/{game,
# platform,input}.h for the contracts they implement) and nothing else;
# every menu screen, asset loader, and physics tick lives once, in
# nfm_game, for both targets.
#
#   Linux dev target (buildable and runnable right here, no special hardware):
#     cmake -S native -B native/build-linux -DNFM_PLATFORM=linux
#     cmake --build native/build-linux
#     native/build-linux/platform/linux/nfm_linux
#
#   PS Vita target (needs VitaSDK, not installed in this environment — see
#   platform/vita/CMakeLists.txt for why this half is unverified):
#     export VITASDK=/usr/local/vitasdk && export PATH=$VITASDK/bin:$PATH
#     cmake -S native -B native/build-vita -DNFM_PLATFORM=vita \
#       -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake
#     cmake --build native/build-vita
#
# Both backends link the SAME nfm_core AND nfm_game sources. That is the
# entire point of this split: the simulation/math port (and, since Part
# 18, the whole game built on top of it) gets tested on Linux, where it
# can actually run, before it ever needs to touch a Vita.
cmake_minimum_required(VERSION 3.13)

# Default to an optimized build when the caller doesn't pick one. Left
# unset before, every build in this repo -- including every Linux dev
# build this whole port has ever been verified against -- was silently
# compiling at the toolchain's own default of NO optimization at all
# (gcc/clang add zero -O flags without an explicit CMAKE_BUILD_TYPE or
# -O switch). Invisible on a fast desktop CPU, which is exactly why this
# went unnoticed for so long, but the PS Vita's much weaker embedded ARM
# core pays for it directly: unoptimized physics (mad_drive()/mad_colide()
# for up to 7 cars every tick) plus this port's immediate-mode
# glBegin/glVertex-per-triangle rendering (gfx_gl.c's own top comment
# already flags that as "slower than a real VBO... optimize later if
# profiling says so") is a very plausible root cause for reported FPS far
# below what the game's actual polycount should need. `CACHE ... FORCE`
# only backfills the cache entry so it shows correctly in a GUI/ccmake --
# the `if(NOT ...)` guard means an explicit `-DCMAKE_BUILD_TYPE=...` the
# caller passes on the command line always wins.
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

project(nfm_native C CXX) # CXX only for the Vita link step below (vitaGL's
                          # preprocessor/expression/gxm objects are C++) --
                          # every source file in this repo is still plain C.

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
# -fwrapv: see core/java_compat.c. -ffp-contract=off: GNU C lets GCC fuse
# a*b+c into one FMA (rounded once), which AArch64 has and Java never does,
# so the Switch build drifted from the Java float math the port mirrors.
add_compile_options(-Wall -Wextra -fwrapv -ffp-contract=off)

set(NFM_PLATFORM "linux" CACHE STRING "Target platform: linux, vita or switch")
set_property(CACHE NFM_PLATFORM PROPERTY STRINGS linux vita switch)

# core/vfs.c's zip reader (data/models.zip) uses zlib's raw inflate.
# VitaSDK ships a zlib portlib, so this is assumed to resolve there too via
# the toolchain's CMAKE_FIND_ROOT_PATH -- unverified, no VitaSDK in this
# environment; flag it if the Vita build ever fails to find it.
find_package(ZLIB REQUIRED)

add_library(nfm_core STATIC
  core/java_compat.c
  core/trig.c
  core/trackers.c
  core/medium.c
  core/gfx.c
  core/gfx_gl.c
  core/font.c
  core/font_data.c
  core/i18n.c
  core/i18n_es.c
  core/i18n_port_es.c
  core/gif_decode.c
  core/png_decode.c
  core/jpeg_decode.c
  core/hud_recolor.c
  core/vfs.c
  core/plane.c
  core/wheels.c
  core/cont_o.c
  core/game_sparker.c
  core/check_points.c
  core/control.c
  core/car_define.c
  core/xt_graphics.c
  core/record.c
  core/mad.c
  core/specials.c
  core/ext_stage.c
  core/ext_mode.c
  core/ext_pt.c
  core/new_cars.c
  core/career.c
  core/career_lineup.c
  core/career_stats.c
  core/career_bots.c
  core/career_stage.c
  core/car_identity.c
  core/progress.c
  core/wav_decode.c
  core/audio_mixer.c
  core/radical_mod.c
  core/ogg_music.c
  core/bots.c
)
target_include_directories(nfm_core PUBLIC core)
target_link_libraries(nfm_core PUBLIC m ZLIB::ZLIB)
# gfx_gl.c's `#include "gl_include.h"` resolves against whichever platform
# directory is on this path -- see platform/{linux,vita}/gl_include.h.
target_include_directories(nfm_core PRIVATE platform/${NFM_PLATFORM})

# The whole game (menu/asset/physics loop) -- shared verbatim by every
# platform target, see platform/common/game.c's own top comment (Part 18).
# Needs the SAME per-platform include dir as nfm_core above: game.c's
# `#include "gl_include.h"`/`"audio.h"` resolve against whichever platform
# directory is active, exactly like gfx_gl.c's does.
add_library(nfm_game STATIC platform/common/game.c platform/common/diag.c)
target_include_directories(nfm_game PUBLIC platform/common)
target_include_directories(nfm_game PRIVATE platform/${NFM_PLATFORM})
target_link_libraries(nfm_game PUBLIC nfm_core)

# Draws a small "<fps>/<worst frame ms>" readout over the finished frame.
# OFF by default -- this exists because the Vita has no console to print
# to and no profiler attached, so "the car select screen feels bad" can
# only become a number by putting one on the screen. Build with
# -DNFM_SHOW_FPS=ON to measure, then rebuild without it.
option(NFM_SHOW_FPS "Draw an on-screen FPS / worst-frame readout" OFF)
if(NFM_SHOW_FPS)
  target_compile_definitions(nfm_game PUBLIC NFM_SHOW_FPS=1)
  message(STATUS "NFM: on-screen FPS readout ENABLED (diagnostic build)")
endif()

if(NFM_PLATFORM STREQUAL "linux")
  add_subdirectory(platform/linux)
elseif(NFM_PLATFORM STREQUAL "vita")
  # Lets the shared game.c name the RIGHT control in its on-screen help.
  # The Instructions screen is written around a keyboard ("ARROW KEYS",
  # "SPACEBAR", "[ A ]"), which on a Vita instructs the player to press
  # things the device does not have -- so that text, and it alone,
  # switches to real button names on this target. Everything else stays
  # byte-identical between the two builds.
  target_compile_definitions(nfm_game PUBLIC NFM_TARGET_VITA=1)
  add_subdirectory(platform/vita)
elseif(NFM_PLATFORM STREQUAL "switch")
  # Nintendo Switch homebrew (.nro). The Instructions screen still names the
  # keyboard's keys here (Joy-Con labels: TASKS_SWITCH.md).
  target_compile_definitions(nfm_game PUBLIC NFM_TARGET_SWITCH=1)
  add_subdirectory(platform/switch)
else()
  message(FATAL_ERROR "Unknown NFM_PLATFORM '${NFM_PLATFORM}' -- expected 'linux', 'vita' or 'switch'")
endif()
