project('libcsh', 'c')

conf = configuration_data()

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

slash_dep = dependency('slash', fallback: ['slash', 'slash_dep'], required: false).as_link_whole()
dependencies += slash_dep

if slash_dep.found()
    conf.set('PYCSH_HAVE_SLASH', slash_dep.found())
endif

libcsh_src = [
    'src/vts.c',
    'src/lock.c',
    'src/serial.c',
    'src/prometheus.c',
    'src/known_hosts.c',
    'src/param_sniffer.c',
    'src/hk_param_sniffer.c',
    'src/victoria_metrics.c',
]

pycsh_config_h = configure_file(output: 'pycshconfig.h', configuration: conf, install_dir: 'include/csh/', install: false)

csh_lib = shared_library('csh',
                         libcsh_src,
                         include_directories: csh_inc,
                         dependencies : dependencies,
                         link_args : ['-Wl,-Map=cshmodtest.map', '-lm'],
                         install : false)

csh_dep = declare_dependency(link_with : csh_lib, dependencies : dependencies, include_directories: csh_inc)

meson.override_dependency('csh', csh_dep)
