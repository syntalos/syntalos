# Build definitions for module: rhd2000

# Intan Rhythm FPGA code blob
rhythm_bitfile_fname = join_paths(rhd2000_mod_install_dir, 'main.bit')
install_data('main.bit',
    install_dir: rhd2000_mod_install_dir
)

if build_machine.cpu_family() != 'x86_64'
    error('Due to the proprietary OpalKelly FrontPanel in the Intan RHD2000 part of this software, ' +
          'it will only work on Intel/Amd x86 and x86_64 architectures. It looks like your CPU ' + build_machine.cpu() + 'is not supported.')
endif

oklib_install_fullpath = join_paths(rhd2000_mod_install_dir, 'libokFrontPanel.so')
install_data('OpalKelly-FrontPanel/ubuntu64/libokFrontPanel.so',
    install_dir: rhd2000_mod_install_dir
)


# Install udev rules to make Opal Kelly USB device readable for normal users
install_data('udev/90-syntalos-intan.rules',
    install_dir: join_paths(udevdir, 'rules.d')
)
