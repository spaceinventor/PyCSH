project('libcsh', 'c')

#conf = configuration_data()

csh_inc = include_directories('.', 'include')

dependencies = [
    dependency('csp', fallback: ['csp', 'csp_dep'], required: true),
    dependency('param', fallback: ['param', 'param_dep'], required: true),#.as_link_whole(),  # NOTE: .as_link_whole() gives linking errors here, but is otherwise needed to preserve symbols
]

curl_dep = dependency('libcurl', required: false)
if not curl_dep.found()
    error('libcurl not found! Please install libcurl4-openssl-dev or the appropriate package for your system.')
endif
dependencies += curl_dep

# libcsh can't link slash.as_link_whole(), if libcsh needs to .as_link_whole() itself.
slash_dep = dependency('slash', fallback: ['slash', 'slash_dep'], required: false)#.as_link_whole()
dependencies += slash_dep
dependencies += dependency('apm', fallback: ['apm_csh', 'apm_csh_dep'], required: true).partial_dependency(includes: true, links: false)

libcsh_src = [
    'src/vts.c',
    'src/lock.c',
    'src/serial.c',
    'src/url_utils.c',
    'src/prometheus.c',
    'src/known_hosts.c',
    'src/slash_utils.c',
    'src/environment.c',
    'src/param_sniffer.c',
    'src/hk_param_sniffer.c',
    'src/victoria_metrics.c',
]

if slash_dep.found()
    #conf.set('CSH_HAVE_SLASH', slash_dep.found())  # NOTE: We don't have any need for this #define, at least as of now.
    libcsh_src += [
        'src/known_hosts_cmd.c',
    ]
endif

#pycsh_config_h = configure_file(output: 'cshconfig.h', configuration: conf, install_dir: 'include/csh/', install: false)

csh_lib = library('csh',
                         libcsh_src,
                         include_directories: csh_inc,
                         dependencies : dependencies,
                         link_args : ['-Wl,-Map=cshmodtest.map', '-lm'],
                         install : false)

csh_dep = declare_dependency(link_with : csh_lib, dependencies : dependencies, include_directories: csh_inc)

meson.override_dependency('csh', csh_dep)
