# SPDX-License-Identifier: MIT
add_library(rc_hle STATIC
    modules.c
    advapi32/advapi32.c
    dinput/dinput.c
    dmusic/dmusic.c
    dmusic/dls.c
    dmusic/player.c
    dsound/dsound.c
    shell32/shell32.c
    wsock/wsock.c
    dshow/dshow.c
    kernel32/process.c
    kernel32/memory.c
    kernel32/locale.c
    kernel32/file.c
    kernel32/profile.c
    kernel32/sync.c
    kernel32/version.c
    kernel32/ole32.c
    user32/user32.c
    user32/window.c
    gdi32/gdi32.c
    gdi32/dc.c
    gdi32/text.c
    gdi32/palette.c
    ddraw/ddraw.c
    ddraw/d3d.c
    frames.c
    mss32/mss32.c
    winmm/winmm.c
    winmm/mci.c
)
target_include_directories(rc_hle PUBLIC include PRIVATE . "${PROJECT_SOURCE_DIR}/third_party/pl_mpeg")
target_link_libraries(rc_hle PUBLIC rc_runtime rc_platform)
