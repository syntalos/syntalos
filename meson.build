project('Syntalos', 'c', 'cpp',
  meson_version: '>=1.2.2',
  default_options: ['c_std=gnu17',
                    'cpp_std=gnu++20',
                    'b_lto=true'],

  license: 'LGPL-3.0+ and GPL-3.0+',
  version: '2.1.1',
)

bug_report_url = 'https://github.com/syntalos/syntalos/issues'

cpp_compiler = meson.get_compiler('cpp')
source_root = meson.project_source_root()

sy_version = meson.project_version()
varr = sy_version.split('.')
sy_major_version = varr[0]
sy_minor_version = varr[1]
sy_micro_version = varr[2]

# Syntalos module link ABI version
sy_mlink_abi_level = 0

root_include_dir = include_directories('.')

#
# Modules
#
fs     = import('fs')
qt     = import('qt6')
pkgc   = import('pkgconfig')
python = import('python').find_installation('python3', modules: ['numpy'])

#
# Feature tests
#
have_sigdescr_np = cpp_compiler.has_function('sigdescr_np', prefix : '#include <string.h>')

#
# Configure files
#
sy_libdir = join_paths(get_option('prefix'),
                       get_option('libdir'),
                       'syntalos')
sy_modules_dir = join_paths(sy_libdir, 'modules')

conf = configuration_data()
conf.set('SY_MAJOR_VERSION_CONF', sy_major_version)
conf.set('SY_MINOR_VERSION_CONF', sy_minor_version)
conf.set('SY_MICRO_VERSION_CONF', sy_micro_version)
conf.set_quoted('PROJECT_VERSION', sy_version)
conf.set10('HAVE_SIGDESCR_NP', have_sigdescr_np)
conf.set_quoted('LIBEXECDIR',
                join_paths(get_option('prefix'),
                           get_option('libexecdir')))
conf.set_quoted('DATADIR',
                join_paths(get_option('prefix'),
                           get_option('datadir')))
conf.set_quoted('LIBDIR',
                join_paths(get_option('prefix'),
                           get_option('libdir')))
conf.set_quoted('BINDIR',
                join_paths(get_option('prefix'),
                           get_option('bindir')))
conf.set_quoted('SYSCONFDIR',
                join_paths(get_option('prefix'),
                           get_option('sysconfdir')))
conf.set_quoted('SY_LIBDIR', sy_libdir)
conf.set_quoted('SY_MODULESDIR', sy_modules_dir)
conf.set_quoted('PYTHON_LANG_VERSION', python.language_version())
conf.set_quoted('PYTHON_INSTALL_PATH', python.get_install_dir())
conf.set_quoted('SY_VCS_TAG', '@VCS_TAG@')
conf.set_quoted('SY_BUG_REPORT_URL', bug_report_url)
config_h = configure_file(output: 'config.h.in', configuration: conf)
config_h = vcs_tag(input: config_h, output: 'config.h')

mconf = configuration_data()
mconf.set_quoted('SY_MODULE_API_TAG', 'sy-' + sy_version)
modconfig_h = configure_file(output: 'modconfig.h', configuration: mconf)

#
# Custom compile flags
#
if get_option('maintainer')
    maintainer_c_args = [
         '-Werror',
         '-Wall',
         '-Wextra',
         '-Wcast-align',
         '-Wno-uninitialized',
         '-Wempty-body',
         '-Wformat-security',
         '-Winit-self',
         '-Wnull-dereference',
         '-Winline',
         '-Wmaybe-uninitialized',
    ]
    maintainer_cpp_args = [
        '-Wsuggest-final-methods'
    ]

    add_project_arguments(maintainer_c_args, language: 'c')
    add_project_arguments([maintainer_c_args, maintainer_cpp_args], language: 'cpp')
endif

# a few compiler warning/error flags we always want enabled
add_project_arguments(
  '-Werror=shadow',
  '-Werror=empty-body',
  '-Werror=missing-prototypes',
  '-Werror=implicit-function-declaration',
  '-Werror=missing-declarations',
  '-Werror=return-type',
  '-Werror=int-conversion',
  '-Werror=incompatible-pointer-types',
  '-Werror=misleading-indentation',
  '-Werror=format-security',

  '-Wno-missing-field-initializers',
  '-Wno-error=missing-field-initializers',
  '-Wno-unused-parameter',
  '-Wno-error=unused-parameter',
  language: 'c'
)
add_project_arguments(
  cpp_compiler.get_supported_arguments([
    '-Werror=empty-body',
    '-Werror=pointer-arith',
    '-Werror=missing-declarations',
    '-Werror=return-type',
    '-Werror=misleading-indentation',
    '-Werror=format-security',
    '-Werror=suggest-override',

    '-Wno-missing-field-initializers',
    '-Wno-error=missing-field-initializers',
    '-Wno-unused-parameter',
    '-Wno-error=unused-parameter',
  ]),
  language: 'cpp'
)

if get_option('tracing')
    add_project_arguments(
        '-pg',
        '-fno-omit-frame-pointers',
        language: 'c'
    )
    add_project_arguments(
        '-pg',
        '-fno-omit-frame-pointers',
        language: 'cpp'
    )
    add_project_link_arguments('-pg')
endif

# set x86_64 specific optimization flags
o_flags = []
if build_machine.cpu_family() == 'x86_64' and not get_option('optimize-native')
    if get_option('optimize-modern-amd64')
        # check if we should optimize for modern x86_64, but still be generic enough to run on many
        # recent CPUs
        if cpp_compiler.has_argument('-march=x86-64-v3')
            message('Enabling x86-64-v3 base microarchitecture level with ' + cpp_compiler.get_id() + ' ' + cpp_compiler.version())
            o_flags = ['-march=x86-64-v3']
        else
            o_flags = ['-mavx', '-mavx2',
                    '-msse4', '-msha',
                    '-msse4.1', '-msse4.2']
            message('Compiler ' + cpp_compiler.get_id() + ' ' + cpp_compiler.version() + ' ' +
                    'does not support x86-64 microarchitecture feature levels.\nExplicitly enabling extensions: ' + ','.join(o_flags))

        endif
    else
        # check if we can tune for modern x86_64 CPUs, but still run on older x86_64 based CPUs
        if cpp_compiler.has_argument('-mtune=x86-64-v3')
            message('Tuning for x86-64-v3 base microarchitecture level')
            o_flags = ['-mtune=x86-64-v3']
        endif
    endif
endif

# vectoization helps a tiny bit for some loops, so we enable it (especially for the Intan module)
# NOTE: This has only been properly tested with GCC >= 10.x, and GCC 12
# enables this already at -O2, so we limit setting the flags to older GCC versions.
if cpp_compiler.get_id() == 'gcc' and cpp_compiler.version().version_compare('<12')
    message('Vectorization enabled explicitly (very-cheap cost model)')
    o_flags += ['-ftree-vectorize']
    if cpp_compiler.has_argument('-fvect-cost-model=very-cheap')
        o_flags += ['-fvect-cost-model=very-cheap']
    else
        o_flags += ['-fvect-cost-model=cheap']
    endif
endif

# apply optimization flags set so far
add_project_arguments(o_flags, language: 'c')
add_project_arguments(o_flags, language: 'cpp')

# optimize for the current (native) CPU only
if get_option('optimize-native')
    message('Optimizing for native CPU (micro)architecture')
    add_project_arguments('-march=native', language: 'c')
    add_project_arguments('-march=native', language: 'cpp')
endif

#
# Dependencies
#
qt_core_dep = dependency('qt6', modules: ['Core'], version: '>= 6.4.2', include_type: 'system')
qt_gui_dep = dependency('qt6', modules: ['Gui', 'Widgets'])
qt_test_dep = dependency('qt6', modules: ['Test'])
qt_opengl_dep = dependency('qt6', modules: ['OpenGL', 'OpenGLWidgets'])
qt_svg_dep = dependency('qt6', modules: ['Svg', 'SvgWidgets'])
qt_dbus_dep = dependency('qt6', modules: ['DBus'])
qt_net_dep = dependency('qt6', modules: ['Network'])
qt_multimedia_dep = dependency('qt6', modules: ['Multimedia'])
qt_concurrent_dep = dependency('qt6', modules: ['Concurrent'])

rt_lib = cpp_compiler.find_library('rt')
thread_dep = dependency('threads')
opengl_dep = dependency('GL')
opencl_dep = dependency('OpenCL')
xxhash_dep = dependency('libxxhash')
eigen_dep = dependency('eigen3', version: '>= 3.3', include_type: 'system')
toml_dep = dependency('tomlplusplus', version: '>=3.0')
opencv_dep = dependency('opencv4', include_type: 'system')
libusb_dep = dependency('libusb-1.0')
libsystemd_dep = dependency('libsystemd')
systemd_dep = dependency('systemd', required: false)
iceoryx_posh_dep = dependency('iceoryx_posh', version: '>= 2.0.5',
                              modules: ['iceoryx_posh::iceoryx_posh'])
iceoryx_hoofs_dep = dependency('iceoryx_hoofs', version: '>= 2.0.5')

glib_dep = dependency('glib-2.0', version: '>=2.58')
gobject_dep = dependency('gobject-2.0', version: '>=2.58')

kfarchive_dep = dependency('KF6Archive')
kfdbusaddons_dep = dependency('KF6DBusAddons')
kfconfigwidgets_dep = dependency('KF6ConfigWidgets')
kftexteditor_dep = dependency('KF6TextEditor')

pipewire_dep = dependency('libpipewire-0.3', version: '>= 0.3.48')

avcodec_dep = dependency('libavcodec', version: '>= 58.20')
avformat_dep = dependency('libavformat', version: '>= 58.20')
avutil_dep = dependency('libavutil', version: '>= 56.20')
swscale_dep = dependency('libswscale', version: '>= 5.5')

gstreamer_dep = dependency('gstreamer-1.0')
gstreamer_app_dep = dependency('gstreamer-app-1.0')
gstreamer_video_dep = dependency('gstreamer-video-1.0')

python_dep = dependency('python3')
python_embed_dep = dependency('python3-embed')
pybind11_dep = dependency('pybind11')

# check for OpenCL headers
cpp_compiler.has_header('CL/cl.h', required: true)

# essential dependencies definitely required by any Syntalos binary
sy_base_deps = [rt_lib,
                thread_dep,
                qt_core_dep,
]

# sed binary for some file manipulation
sed_prog = find_program('sed')

# we may need to run some Python code during build
python3_exe = find_program('python3')

# vendored 3rd-party code
if 'plot' in get_option('modules')
    subdir('contrib/vendor')
endif

#
# Options
#
udevdir = get_option('udevdir')
if udevdir == ''
    udev = dependency('udev')
    udevdir = udev.get_variable(pkgconfig: 'udevdir')
endif

#
# Source info file
#

# This file is only created to allow some modules to detect
# whether Syntalos is run from a source tree
si_data = configuration_data()
si_data.set('SY_SOURCE_ROOT', source_root)
configure_file(output : 'sy-source-info.txt',
               configuration : si_data)

#
# Subdirectories
#
subdir('src')
subdir('modules')
subdir('tests')
subdir('tools')
subdir('data')
