
# Static library with Qt-free common interface that we statically link
# into multiple places.
# This is done to avoid having public interface-libraries depend on
# private-interface libraries to share code.
# This library must only contain the minimal amount of code, if something
# *can* go into datactl or mlink or fabric, it should go there.

sypriv_src = [

]

sypriv_lib = static_library('sypriv',
    [sypriv_src,
     config_h],
    dependencies: [
        rt_lib,
        thread_dep,
        gio_dep,
    ],
    include_directories: [
        root_include_dir,
        include_directories('.'),
    ],
    install: false,
)

sypriv_dep = declare_dependency(
    link_with: sypriv_lib,
    include_directories: [
        include_directories('.'),
    ]
)
