# Host build (plain gcc/clang, no VitaSDK) for native/core/*.c, which has no
# platform dependency. This is the cheap, fast check -- see
# ../PORT_SPEC.md §6. Run with:
#   cmake -S native/tests -B native/tests/build && cmake --build native/tests/build && native/tests/build/java_compat_test
cmake_minimum_required(VERSION 3.13)
project(nfm_native_port_tests C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra -fwrapv)

find_package(ZLIB REQUIRED) # core/vfs.c's zip reader

add_executable(java_compat_test
  java_compat_test.c
  ../core/java_compat.c
  ../core/trig.c
)
target_link_libraries(java_compat_test m)

add_executable(trackers_test
  trackers_test.c
  ../core/trackers.c
)

add_executable(medium_test
  medium_test.c
  ../core/medium.c
  ../core/java_compat.c
  ../core/trig.c
  ../core/gfx.c
)
target_link_libraries(medium_test m)

add_executable(gfx_test
  gfx_test.c
  ../core/gfx.c
)
target_link_libraries(gfx_test m)

add_executable(vfont_test
  vfont_test.c
  ../core/vfont.c
  ../core/gfx.c
)
target_link_libraries(vfont_test m)

add_executable(vfs_test
  vfs_test.c
  ../core/vfs.c
)
target_link_libraries(vfs_test ZLIB::ZLIB)

add_executable(gif_decode_test
  gif_decode_test.c
  ../core/gif_decode.c
  ../core/vfs.c
)
target_link_libraries(gif_decode_test ZLIB::ZLIB)

add_executable(png_decode_test
  png_decode_test.c
  ../core/png_decode.c
  ../core/vfs.c
)
target_link_libraries(png_decode_test ZLIB::ZLIB)

add_executable(jpeg_decode_test
  jpeg_decode_test.c
  ../core/jpeg_decode.c
  ../core/vfs.c
)
target_link_libraries(jpeg_decode_test m ZLIB::ZLIB)

add_executable(hud_recolor_test
  hud_recolor_test.c
  ../core/hud_recolor.c
  ../core/java_compat.c
  ../core/gif_decode.c
  ../core/vfs.c
)
target_link_libraries(hud_recolor_test ZLIB::ZLIB m)

add_executable(plane_test
  plane_test.c
  ../core/plane.c
  ../core/medium.c
  ../core/trackers.c
  ../core/java_compat.c
  ../core/trig.c
  ../core/gfx.c
)
target_link_libraries(plane_test m)

add_executable(wheels_test
  wheels_test.c
  ../core/wheels.c
  ../core/plane.c
  ../core/medium.c
  ../core/trackers.c
  ../core/java_compat.c
  ../core/trig.c
  ../core/gfx.c
)
target_link_libraries(wheels_test m)

add_executable(cont_o_test
  cont_o_test.c
  ../core/cont_o.c
  ../core/wheels.c
  ../core/plane.c
  ../core/medium.c
  ../core/trackers.c
  ../core/java_compat.c
  ../core/trig.c
  ../core/gfx.c
  ../core/vfs.c
)
target_link_libraries(cont_o_test m ZLIB::ZLIB)

add_executable(game_sparker_test
  game_sparker_test.c
  ../core/game_sparker.c
  ../core/check_points.c
  ../core/record.c
  ../core/mad.c
  ../core/car_define.c
  ../core/xt_graphics.c
  ../core/cont_o.c
  ../core/wheels.c
  ../core/plane.c
  ../core/medium.c
  ../core/trackers.c
  ../core/java_compat.c
  ../core/trig.c
  ../core/gfx.c
  ../core/vfs.c
)
target_link_libraries(game_sparker_test m ZLIB::ZLIB)

add_executable(check_points_test
  check_points_test.c
  ../core/check_points.c
  ../core/trackers.c
  ../core/java_compat.c
  ../core/trig.c
  ../core/record.c
  ../core/mad.c
  ../core/cont_o.c
  ../core/wheels.c
  ../core/plane.c
  ../core/medium.c
  ../core/gfx.c
  ../core/car_define.c
  ../core/xt_graphics.c
  ../core/vfs.c
)
target_link_libraries(check_points_test m ZLIB::ZLIB)

add_executable(control_test
  control_test.c
  ../core/control.c
  ../core/check_points.c
  ../core/medium.c
  ../core/trackers.c
  ../core/java_compat.c
  ../core/trig.c
  ../core/gfx.c
  ../core/record.c
  ../core/mad.c
  ../core/cont_o.c
  ../core/wheels.c
  ../core/plane.c
  ../core/car_define.c
  ../core/xt_graphics.c
  ../core/vfs.c
)
target_link_libraries(control_test m ZLIB::ZLIB)

add_executable(car_define_test
  car_define_test.c
  ../core/car_define.c
  ../core/cont_o.c
  ../core/wheels.c
  ../core/plane.c
  ../core/medium.c
  ../core/trackers.c
  ../core/java_compat.c
  ../core/trig.c
  ../core/gfx.c
  ../core/vfs.c
)
target_link_libraries(car_define_test m ZLIB::ZLIB)

add_executable(record_test
  record_test.c
  ../core/record.c
  ../core/mad.c
  ../core/cont_o.c
  ../core/wheels.c
  ../core/plane.c
  ../core/medium.c
  ../core/trackers.c
  ../core/java_compat.c
  ../core/trig.c
  ../core/gfx.c
  ../core/vfs.c
  ../core/car_define.c
  ../core/xt_graphics.c
  ../core/control.c
  ../core/check_points.c
)
target_link_libraries(record_test m ZLIB::ZLIB)

add_executable(progress_test
  progress_test.c
  ../core/progress.c
)

add_executable(wav_decode_test
  wav_decode_test.c
  ../core/wav_decode.c
  ../core/vfs.c
)
target_link_libraries(wav_decode_test ZLIB::ZLIB)

add_executable(audio_mixer_test
  audio_mixer_test.c
  ../core/audio_mixer.c
)

add_executable(radical_mod_test
  radical_mod_test.c
  ../core/radical_mod.c
  ../core/vfs.c
)
target_link_libraries(radical_mod_test ZLIB::ZLIB)

add_executable(bots_test
  bots_test.c
  ../core/bots.c
  ../core/progress.c
  ../core/java_compat.c
  ../core/trig.c
)
target_link_libraries(bots_test m)

add_executable(mad_test
  mad_test.c
  ../core/mad.c
  ../core/cont_o.c
  ../core/wheels.c
  ../core/plane.c
  ../core/medium.c
  ../core/trackers.c
  ../core/java_compat.c
  ../core/trig.c
  ../core/gfx.c
  ../core/vfs.c
  ../core/car_define.c
  ../core/record.c
  ../core/xt_graphics.c
  ../core/control.c
  ../core/check_points.c
)
target_link_libraries(mad_test m ZLIB::ZLIB)

enable_testing()
add_test(NAME java_compat_test COMMAND java_compat_test)
add_test(NAME trackers_test COMMAND trackers_test)
add_test(NAME medium_test COMMAND medium_test)
add_test(NAME gfx_test COMMAND gfx_test)
add_test(NAME vfont_test COMMAND vfont_test)
add_test(NAME plane_test COMMAND plane_test)
add_test(NAME wheels_test COMMAND wheels_test)
add_test(NAME cont_o_test COMMAND cont_o_test WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME game_sparker_test COMMAND game_sparker_test WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME check_points_test COMMAND check_points_test)
add_test(NAME control_test COMMAND control_test)
add_test(NAME car_define_test COMMAND car_define_test)
add_test(NAME record_test COMMAND record_test)
add_test(NAME mad_test COMMAND mad_test WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME progress_test COMMAND progress_test)
add_test(NAME wav_decode_test COMMAND wav_decode_test WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME audio_mixer_test COMMAND audio_mixer_test)
add_test(NAME radical_mod_test COMMAND radical_mod_test WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME bots_test COMMAND bots_test)
# vfs_test reads the real stages/1.txt with a path relative to this build
# dir (native/tests/build/) -- WORKING_DIRECTORY pins that regardless of
# where ctest itself is invoked from.
add_test(NAME vfs_test COMMAND vfs_test WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME gif_decode_test COMMAND gif_decode_test WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME png_decode_test COMMAND png_decode_test WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME jpeg_decode_test COMMAND jpeg_decode_test WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME hud_recolor_test COMMAND hud_recolor_test WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
