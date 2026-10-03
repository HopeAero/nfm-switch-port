# Desktop Linux backend: SDL2 window/input/audio + real desktop OpenGL,
# requesting a compatibility profile so the GL1.1 immediate-mode calls
# (glBegin/glVertex/glColor/glEnd) that vitaGL implements on-device also run
# here unmodified. See ../../PORT_SPEC.md §4 for why that subset was chosen.
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

# nfm_game (platform/common/game.c) itself #includes this platform
# directory's audio.h, which pulls in <SDL.h> -- so nfm_game needs SDL2's
# headers too, not just the final executable link.
target_link_libraries(nfm_game PUBLIC SDL2::SDL2)

add_executable(nfm_linux main.c input.c audio.c platform.c)
target_link_libraries(nfm_linux PRIVATE nfm_game SDL2::SDL2 SDL2::SDL2main OpenGL::GL)
