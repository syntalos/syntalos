# Syntalos ImGui / plotting support library

# This internal library can be used by modules to plot graphs
# using ImGui or to render their UI in a Qt window using ImGui.

syntalos_imgui_src = [
    'imgui-renderer.h',
    'imgui-renderer.cpp',
    'qtimgui.h',
    'qtimgui.cpp',
]

syntalos_imgui_inc_dir = include_directories('.')

syntalos_imgui_h = []
syntalos_imgui_s = []
foreach s : syntalos_imgui_src
    if s.endswith('.h')
        syntalos_imgui_h += s
    elif s.endswith('.cpp')
        syntalos_imgui_s += s
    endif
endforeach

syntalos_imgui_moc = qt.compile_moc(
    headers: syntalos_imgui_h,
    sources: syntalos_imgui_s,
    extra_args: ['--no-notes']
)

syntalos_imgui_lib = shared_library('syntalos-imgui',
    [syntalos_imgui_src,
     syntalos_imgui_moc,
     config_h],
    dependencies: [
        qt_gui_dep,
        qt_opengl_dep,
        imgui_dep,
        implot_dep,
    ],
    include_directories: [
        root_include_dir,
        syntalos_imgui_inc_dir
    ],
    install: true,
    install_dir: sy_libdir,
    install_rpath: sy_libdir,
)

syntalos_imgui_dep = declare_dependency(
    link_with: syntalos_imgui_lib,
    dependencies: [
        qt_gui_dep,
        qt_opengl_dep,
    ],
    include_directories: [
        syntalos_imgui_inc_dir,
        imgui_inc,
        implot_inc,
    ],
    compile_args: ['-DIMGUI_USER_CONFIG="sy_imconfig.h"'],
)
