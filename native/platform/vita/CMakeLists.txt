# PS Vita backend: vitaGL over sceGxm. See ../../PORT_SPEC.md §8. Only
# included when the top-level native/CMakeLists.txt is configured with
# -DNFM_PLATFORM=vita (which requires the vitasdk toolchain file to even
# reach this point, so a plain host build never sees the Sce*_stub
# libraries below). Build-tested against a real VitaSDK install (compiles
# and links clean); running .vpk on real hardware is still unverified.
#
# Requires vitaGL (vdpm: `vdpm vitaGL`) for the rendering target described in
# PORT_SPEC.md §4, and zlib (vdpm: `vdpm zlib`) for nfm_core's own
# core/vfs.c zip reader -- see the top-level CMakeLists.txt's own note on
# find_package(ZLIB) resolving against vdpm's portlib via the toolchain's
# CMAKE_FIND_ROOT_PATH.
include("${VITASDK}/share/vita.cmake" REQUIRED)

# vitaGL draws its own branded splashscreen before handing the display
# over, which this port does not want in front of the game's own title
# screen. There is no runtime toggle: upstream vitaGL
# (https://github.com/Rinnegatamante/vitaGL) guards the whole feature
# behind a `SKIP_SPLASHSCREEN` #ifdef in source/splashscreen.c, set via
# `make NO_SPLASHSCREEN=1` at the LIBRARY's own build time -- and `vdpm
# vitaGL` always installs the default build with it compiled IN. A
# library built either way looks identical from vitaGL.h, so the only way
# to tell is to look for the symbol in the archive itself.
#
# Deliberately a WARNING, not an error: the splash is cosmetic, and this
# check was briefly a FATAL_ERROR on the mistaken belief that it caused
# the first real-hardware crash (that was actually a stack overflow in
# our own gif_decode/lzw_decode path -- see ../../TASKS_NATIVE.md).
# Failing someone's whole build over a logo would be the wrong trade.
set(NFM_VITAGL_LIB "${VITASDK}/arm-vita-eabi/lib/libvitaGL.a")
if(EXISTS "${NFM_VITAGL_LIB}")
  find_program(NFM_VITA_NM arm-vita-eabi-nm PATHS "${VITASDK}/bin")
  if(NFM_VITA_NM)
    execute_process(
      COMMAND "${NFM_VITA_NM}" "${NFM_VITAGL_LIB}"
      COMMAND grep -q vgl_splash_data
      RESULT_VARIABLE NFM_VITAGL_HAS_SPLASH
      OUTPUT_QUIET ERROR_QUIET
    )
    if(NFM_VITAGL_HAS_SPLASH EQUAL 0)
      message(WARNING
        "libvitaGL.a still has vgl_splash_data in it, so the build will "
        "show vitaGL's boot splashscreen before the game. To remove it, "
        "rebuild vitaGL with the splash compiled out and reinstall over "
        "vdpm's default, then reconfigure:\n"
        "  git clone https://github.com/Rinnegatamante/vitaGL\n"
        "  cd vitaGL && make clean && make NO_SPLASHSCREEN=1 install\n")
    endif()
  endif()
endif()

set(VITA_APP_NAME "Need for Madness")
set(VITA_TITLEID  "NFMD00001")
set(VITA_VERSION  "01.00")

add_executable(nfm_vita main.c input.c audio.c platform.c)
# vitaGL's own shader preprocessor (preprocessor.o/expression.o) and gxm.o
# are compiled from C++ -- linking this C-only executable with the plain C
# driver leaves every std::/operator new/delete/__cxa_*/__gxx_personality_v0
# symbol they pull in unresolved. Confirmed against a real build: hundreds
# of "undefined reference to std::__cxx11::..." until this was set.
set_target_properties(nfm_vita PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(nfm_vita PRIVATE
  nfm_game
  vitaGL
  # vitaGL's own shader-compile pipeline (glCompileShader/glLinkProgram in
  # custom_shaders.o and ffp.o) calls into vitaShaRK's shark_* functions
  # directly (vdpm: `vdpm vitashark`).
  vitashark             # libvitashark.a
  # vitaShaRK's own shader-compiler backend -- shark_compile_shader_extended/
  # shark_clear_output stayed undefined with just `vitashark` linked until
  # these two were added alongside it (confirmed against a real build).
  SceShaccCgExt         # libSceShaccCgExt.a
  SceShaccCg_stub       # libSceShaccCg_stub.a
  taihen_stub           # libtaihen_stub.a -- resolves the remaining tai* symbols
  # matmul4_neon/normalize3_neon/normalize4_neon/sincosf_c/tanf_neon_hfp
  # (matrices.o, splashscreen.o) -- vitaGL's NEON-optimised math helpers,
  # a separate portlib. The real vdpm package name is `libmathneon`
  # (`vdpm libmathneon`) -- `vdpm math-neon`/`vdpm mathneon` 404 silently
  # (vdpm doesn't check the HTTP status, so a 404 page gets piped into tar
  # and fails as "xz: unrecognised file format" instead of a clear error).
  # The produced archive is libmathneon.a either way, hence plain `mathneon`
  # here.
  mathneon              # libmathneon.a
  SceDisplay_stub       # libSceDisplay_stub.a
  SceGxm_stub
  SceCtrl_stub
  SceTouch_stub
  SceAudio_stub
  # sceKernelCreateThread/CreateMutex/GetProcessTimeWide (platform.c,
  # audio.c). Confirmed against a real VitaSDK install: the stub lib is
  # SceKernelThreadMgr_stub, not SceThreadmgr_stub (that name doesn't
  # exist -- ld error "cannot find -lSceThreadmgr_stub" is what caught
  # this; the ForKernel/ForDriver variants alongside it are kernel-side,
  # not what this usermode app links against).
  SceKernelThreadMgr_stub
  # scePowerSet*ClockFrequency (platform.c's platform_init).
  ScePower_stub
  SceSysmodule_stub
  SceCommonDialog_stub
  # sceAppMgrGetBudgetInfo (vgl.o's vglInitWithCustomThreshold, called
  # from vglInit) and the sceSharedFb* family (gxm.o's display-surface
  # setup and vglSwapBuffers) both live in the AppMgr module.
  SceAppMgr_stub        # libSceAppMgr_stub.a
  # sceDmacMemcpy (buffers.o, gpu_utils.o's texture/mipmap upload paths).
  SceKernelDmacMgr_stub # libSceKernelDmacMgr_stub.a
  # Pulled in explicitly rather than relying on the C++ link step's
  # defaults -- stdc++/libm/libc are what vitaGL's C++ objects and this
  # target's own C sources ultimately need.
  stdc++
  m
  c
)

vita_create_self(nfm_vita.self nfm_vita)

# Bundles data/, stages/, music/, and mycars/ into the .vpk itself, read
# back at runtime through the app0: prefix (see platform_asset_prefix()
# in platform.c) -- this app has no separate installer step to push
# assets to ux0: separately, unlike some larger homebrew titles (see
# ../../TASKS_NATIVE.md's Part 18 entry: total asset size is only a few
# MB, small enough to just bundle). vita_create_vpk's own FILE directive
# takes one src/dest pair at a time, not a directory, so this globs each
# real asset directory at configure time rather than hardcoding a file
# list that would silently go stale the next time an asset gets added or
# renamed. mycars/ bundles both .rad files (only "Simple_Car.rad" is
# actually read by car_define.c's custom-car flow -- the other one rides
# along harmlessly, a few KB).
set(NFM_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
set(NFM_VPK_FILE_ARGS "")
foreach(assetdir data stages music mycars)
  file(GLOB_RECURSE _nfm_assets RELATIVE "${NFM_REPO_ROOT}" "${NFM_REPO_ROOT}/${assetdir}/*")
  foreach(_nfm_asset ${_nfm_assets})
    # Skip any path with a space in it. vita_create_vpk turns each FILE pair
    # into a `-a src=dst` argument on vita-pack-vpk's command line, and a
    # space splits that argument in two: the packer then exits without ever
    # writing nfm_vita.vpk.out, and the build fails one step later on
    # "Error copying file ... nfm_vita.vpk.out ... No such file or directory",
    # which points at the copy rather than at the real cause. Exactly one
    # shipped asset trips this (an example car in mycars/ that nothing loads
    # -- car_define.c only ever reads Simple_Car.rad), but the guard is
    # written against the character rather than that one name so a future
    # asset cannot reintroduce the same very confusing failure.
    string(FIND "${_nfm_asset}" " " _nfm_space)
    if(_nfm_space GREATER -1)
      message(STATUS "VPK: skipping '${_nfm_asset}' -- spaces break vita-pack-vpk's argument list")
      continue()
    endif()
    list(APPEND NFM_VPK_FILE_ARGS FILE "${NFM_REPO_ROOT}/${_nfm_asset}" "${_nfm_asset}")
  endforeach()
endforeach()

vita_create_vpk(nfm_vita.vpk ${VITA_TITLEID} nfm_vita.self
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
  ${NFM_VPK_FILE_ARGS}
)
