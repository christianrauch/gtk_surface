project('gtk_surface', 'c')

# GTK, wayland
gtk_dep = dependency('gtk+-3.0')
wayland_dep = dependency('wayland-client')

# wayland protocol scanner
dep_scanner = dependency('wayland-scanner', native: true)
prog_scanner = find_program(dep_scanner.get_pkgconfig_variable('wayland_scanner'))

# wayland standard protocols
dep_wp = dependency('wayland-protocols', version: '>= 1.12')
dir_wp_base = dep_wp.get_pkgconfig_variable('pkgdatadir')

# generate 'xdg-shell' bindings
base_file = 'xdg-shell'
xml_path = '@0@/stable/@1@/@1@.xml'.format(dir_wp_base, base_file)

foreach output_type: [ 'client-header', 'private-code' ]
    if output_type == 'client-header'
        output_file = '@0@-client-protocol.h'.format(base_file)
    elif output_type == 'private-code'
        output_file = '@0@-protocol.c'.format(base_file)
        if dep_scanner.version().version_compare('< 1.14.91')
            output_type = 'code'
        endif
    endif

    var_name = output_file.underscorify()
    target = custom_target(
        '@0@ @1@'.format(base_file, output_type),
        command: [ prog_scanner, output_type, '@INPUT@', '@OUTPUT@' ],
        input: xml_path,
        output: output_file,
    )

    set_variable(var_name, target)
endforeach

executable('gtk_surface', sources: ['main.c', target], dependencies: [gtk_dep, wayland_dep])
