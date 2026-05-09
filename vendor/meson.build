# Build definitions for vendored 3rd-party code

if 'plot' in get_option('modules')

#
# ImGUI
#
if not fs.is_file('imgui/imgui.h')
    error('ImGui submodule is missing. Did you run `git submodule update --init`?')
endif
imgui_inc = include_directories('imgui/')
imgui_src = files(
  'imgui/imconfig.h',
  'imgui/imgui.cpp',
  'imgui/imgui.h',
  'imgui/imgui_demo.cpp',
  'imgui/imgui_draw.cpp',
  'imgui/imgui_internal.h',
  'imgui/imgui_widgets.cpp',
  'imgui/imgui_tables.cpp',
  'imgui/imstb_rectpack.h',
  'imgui/imstb_textedit.h',
  'imgui/imstb_truetype.h',
)
imgui_lib = static_library(
  'imgui',
  imgui_src,
  include_directories: [imgui_inc, include_directories('.')],
  cpp_args: ['-DIMGUI_USER_CONFIG="sy_imconfig.h"'],
)
imgui_dep = declare_dependency(
  link_with : imgui_lib,
  include_directories: [imgui_inc, include_directories('.')],
  compile_args: ['-DIMGUI_USER_CONFIG="sy_imconfig.h"'],
)

#
# ImPlot
#
if not fs.is_file('implot/implot.h')
    error('ImPlot submodule is missing. Did you run `git submodule update --init`?')
endif
implot_inc = include_directories('implot/')
implot_src = files(
  'implot/implot.h',
  'implot/implot.cpp',
  'implot/implot_items.cpp',
  'implot/implot_internal.h',
)
implot_lib = static_library(
  'implot',
  implot_src,
  dependencies: imgui_dep,
  include_directories: implot_inc,
  cpp_args: ['-Wno-missing-declarations'],
)
implot_dep = declare_dependency(
  link_with: implot_lib,
  include_directories: implot_inc,
)

endif # 'plot' in get_option('modules')

if 'open-ephys-acq' in get_option('modules')

#
# liboni
#
if not fs.is_file('liboni/api/liboni/oni.h')
    error('liboni submodule is missing. Did you run `git submodule update --init`?')
endif
liboni_inc = include_directories('liboni/api/liboni')
liboni_src = files(
    'liboni/api/liboni/oni.c',
    'liboni/api/liboni/onix.c',
    'liboni/api/liboni/onidriverloader.c',
)
liboni_lib = shared_library(
    'oni',
    liboni_src,
    include_directories: liboni_inc,
    c_args: ['-Wall', '-W', '-O3', '-Wno-error=shadow'],
    dependencies: [c_compiler.find_library('dl', required: true)],
    soversion: '1',
    install: true,
    install_dir: sy_libdir,
)
liboni_dep = declare_dependency(
    link_with: liboni_lib,
    include_directories: liboni_inc,
)

endif # 'open-ephys-acq' in get_option('modules')
