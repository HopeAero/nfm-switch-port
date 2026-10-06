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

# RomFS: the game's own asset folders, copied verbatim from the repo root.
set(NFM_ROMFS "${CMAKE_CURRENT_BINARY_DIR}/romfs")
foreach(dir data stages music mycars mystages)
  file(COPY "${PROJECT_SOURCE_DIR}/../${dir}" DESTINATION "${NFM_ROMFS}")
endforeach()

nx_generate_nacp(nfm_switch.nacp
  NAME "Need for Madness"
  AUTHOR "Radicalplay (port: community)"
  VERSION "0.1.0")

nx_create_nro(nfm_switch
  NACP nfm_switch.nacp
  ICON "${CMAKE_CURRENT_SOURCE_DIR}/icon.jpg"
  ROMFS "${NFM_ROMFS}")
