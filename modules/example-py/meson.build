
module_data = [
]

#
# Generic Python module setup
#
module_name = fs.name(meson.current_source_dir()).to_lower().underscorify().replace('_', '-')
mod_install_dir = join_paths(sy_modules_dir, fs.name(meson.current_source_dir()))
install_subdir(meson.current_source_dir(),
    install_dir: mod_install_dir,
    strip_directory: true,
    exclude_files: ['meson.build']
)
run_command(ln_exe,
    ['-snf',
    meson.current_source_dir(),
    join_paths(meson.current_build_dir(), '..', module_name + '.srclink')],
    check: true
)
