# Syntalos Data Types and Time Sync Library

# This library contains common data types and data manipulation
# code used by Syntalos and its modules. It is public API.
#
# It also contains some private interfaces that the public
# (and private) Syntalos libraries may use internally.

sy_datactl_pub_hdr = [
    'binarystream.h',
    'datatypes.h',
    'flags.h',
    'frametype.h',
    'edlstorage.h',
    'eigenaux.h',
    'logging.h',
    'streammeta.h',
    'syclock.h',
    'timesync.h',
    'tsyncfile.h',
    'uuid.h',
]

sy_datactl_priv_hdr = [
    'edlutils.h',
    'loginternal.h',
    'monikers.h',

    'priv/rtkit.h',
    'priv/cpuaffinity.h',
]

sy_datactl_src = [
    'datatypes.cpp',
    'edlstorage.cpp',
    'edlutils.cpp',
    'logging.cpp',
    'monikers.cpp',
    'streammeta.cpp',
    'syclock.cpp',
    'timesync.cpp',
    'tsyncfile.cpp',
    'uuid.cpp',

    'priv/cpuaffinity.cpp',
    'priv/rtkit.cpp',
]

# Detect #embed support (GCC 15+, Clang 19+)
has_embed = cpp_compiler.compiles(
    '''static constexpr unsigned char d[] = {
#embed __FILE__
};
int main() { return (int)sizeof(d); }
''',
    name: 'C++ #embed support',
)

sy_datactl_cpp_args = []
sy_datactl_gen_hdr = []
if has_embed
    sy_datactl_cpp_args = [
        '--embed-dir=' + source_root / 'data',
    ]
else
    gen_script = files(source_root / 'data' / 'words' / 'gen_word_header.py')
    words_src = source_root / 'data' / 'words'

    sy_datactl_gen_hdr = [
        custom_target('words_embed_hdr',
            input: [
                words_src / 'adjectives.txt',
                words_src / 'animals.txt',
                words_src / 'intermediate.txt',
                words_src / 'nouns.txt',
            ],
            output: 'words_embed.h',
            command: [
                python, gen_script, '@OUTPUT@',
                'adjectives_data:' + words_src / 'adjectives.txt',
                'animals_data:' + words_src / 'animals.txt',
                'intermediate_data:' + words_src / 'intermediate.txt',
                'nouns_data:' + words_src / 'nouns.txt',
            ],
        ),
    ]
endif

syntalos_datactl_lib = shared_library('syntalos-datactl',
    [sy_datactl_pub_hdr,
     sy_datactl_priv_hdr,
     sy_datactl_src,
     sy_datactl_gen_hdr],
    soversion: sy_mlink_abi_level,
    version: sy_version,
    dependencies: [
        gio_dep,
        eigen_dep,
        xxhash_dep,
        opencv_dep,
        toml_dep,
    ],
    include_directories: [
        root_include_dir,
        include_directories('../'),
    ],
    cpp_args: sy_datactl_cpp_args,
    install: true,
    install_rpath: sy_libdir,
)

syntalos_datactl_dep = declare_dependency(
    link_with: syntalos_datactl_lib,
    dependencies: [
        eigen_dep,
    ],
    include_directories: [
        include_directories('./include'),
        include_directories('../'),
    ]
)

install_headers(sy_datactl_pub_hdr, subdir: 'syntalos' / 'datactl')
install_headers('include/syntalos-datactl', subdir: 'syntalos')

pkgc.generate(
    syntalos_datactl_lib,
    version: sy_version,
    name: 'syntalos-datactl',
    description: 'Library containing Syntalos data management, streaming and time synchronization primitives.',
    filebase: 'syntalos-datactl',
    subdirs: 'syntalos',
    requires: [
        'eigen3',
    ],
)
