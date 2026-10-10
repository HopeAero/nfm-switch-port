# Nintendo Switch backend (libnx homebrew): SDL2 window/audio, Mesa GL through
# EGL with the entry points loaded at runtime (gl_include.h), libnx pads.
# Built with devkitPro's toolchain, e.g. in its Docker image:
#
#   docker run --rm -v "$PWD:/src" -w /src devkitpro/devkita64 bash -c \
#     'cmake -S native -B native/build-switch -DNFM_PLATFORM=switch \
#        -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Switch.cmake && \
#      cmake --build native/build-switch -j'
#
# Output: native/build-switch/platform/switch/nfm_switch.nro -- every asset is
# inside it (RomFS), so it is the only file to copy to sdmc:/switch/.
find_package(PkgConfig REQUIRED)
pkg_check_modules(NFM_SDL2 REQUIRED IMPORTED_TARGET sdl2)

# nfm_game (game.c) includes this directory's audio.h, which pulls in <SDL.h>.
target_link_libraries(nfm_game PUBLIC PkgConfig::NFM_SDL2)

add_executable(nfm_switch main.c input.c audio.c platform.c)
target_link_libraries(nfm_switch PRIVATE nfm_game PkgConfig::NFM_SDL2)

option(NFM_NXLINK "Send stdout/stderr to nxlink on the PC" OFF)
if(NFM_NXLINK)
  target_compile_definitions(nfm_switch PRIVATE NFM_NXLINK=1)
endif()
option(NFM_SVCLOG "Send stderr to svcOutputDebugString (emulator logs)" OFF)
if(NFM_SVCLOG)
  target_compile_definitions(nfm_switch PRIVATE NFM_SVCLOG=1)
  target_compile_definitions(nfm_game PRIVATE NFM_SVCLOG=1)   # game.c's diagnostics
endif()

# RomFS: the game's own asset folders, copied verbatim from the repo root.
# The copy happens at configure time, so every asset is also a configure
# dependency (an added or edited file reconfigures and recopies), and the
# copies are tracked by an asset target the .nro depends on -- a plain folder
# passed to nx_create_nro is not tracked, and a new file never reached the .nro.
include(dkp-asset-folder)
get_filename_component(NFM_ROOT "${PROJECT_SOURCE_DIR}/.." ABSOLUTE)
set(NFM_ROMFS "${CMAKE_CURRENT_BINARY_DIR}/romfs")
set(NFM_ROMFS_FILES "")
foreach(dir data stages music mycars mystages ext)
  file(GLOB_RECURSE files CONFIGURE_DEPENDS RELATIVE "${NFM_ROOT}" "${NFM_ROOT}/${dir}/*")
  list(APPEND NFM_ROMFS_FILES ${files})
  file(COPY "${NFM_ROOT}/${dir}" DESTINATION "${NFM_ROMFS}")
endforeach()
list(TRANSFORM NFM_ROMFS_FILES PREPEND "${NFM_ROOT}/" OUTPUT_VARIABLE NFM_ROMFS_SOURCES)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${NFM_ROMFS_SOURCES})
dkp_add_asset_target(nfm_romfs "${NFM_ROMFS}")
dkp_track_assets(nfm_romfs FILES ${NFM_ROMFS_FILES})

# The extended branch builds its own homebrew: another title, another file
# (nfm_extended.nro) and its own save folder, so it sits beside the original.
nx_generate_nacp(nfm_extended.nacp
  NAME "Need for Madness 2 Extended"
  AUTHOR "Radicalplay, Extended Mode (port: community)"
  VERSION "0.2.1")

nx_create_nro(nfm_switch
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/nfm_extended.nro
  NACP nfm_extended.nacp
  ICON "${CMAKE_CURRENT_SOURCE_DIR}/icon.jpg"
  ROMFS nfm_romfs)
