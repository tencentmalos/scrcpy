project('scrcpy',
		['c','cpp'],
        version: '3.2',
        meson_version: '>= 0.49',
        default_options: [
            'c_std=c11',
            'cpp_std=c++20',
            'warning_level=2',
            'b_ndebug=if-release',
        ])

# add_project_arguments('-static-libgcc', '-static-libstdc++', '-Wmissing-prototypes', language: 'cpp')
# add_project_link_arguments('-static-libgcc', '-static-libstdc++', language: 'cpp')

add_project_arguments('-Wmissing-prototypes', language: 'c')

if get_option('compile_app')
    subdir('app')
endif

if get_option('compile_server')
    subdir('server')
endif
