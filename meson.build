project('csh_bindings', 'c', subproject_dir: 'lib', default_options: [
	'buildtype=release',
	'c_std=gnu11',
	'b_lto=false',
	'default_library=static',
	'csp:packet_padding_bytes=42',
	'csp:buffer_count=1000',
	'csp:buffer_size=2048',
	'csp:conn_max=20',
	'csp:conn_rxqueue_len=1000',
	'csp:qfifo_len=1000',
	'csp:rdp_max_window=1000',
	'csp:port_max_bind=16',
	'csp:use_rtable=true',  # TODO Kevin: rtable should not be necessary
	'param:have_fopen=true',
	'param:slash=true',  # TODO Kevin: How should param:slash work if slash is optional
	'slash:builtins=true',
	'param:commands=true',
	'param:scheduler=true',
	'param:commands_client=true',
	'param:scheduler_client=true',
	'param:collector=false',
	'param:list_dynamic=true',])

add_global_arguments('-I../include', language: 'c')

conf = configuration_data()

dependencies = []

if get_option('build_apm')
	dependencies += dependency('apm_csh', fallback: ['apm_csh', 'apm_csh_dep'], required: true)
	conf.set('PYCSH_HAVE_APM', 1)  # If we havn't errored, we have APM

	csp_dep = dependency('csp', fallback: ['csp', 'csp_dep'], required: true).partial_dependency(links: false, includes: true)
	slash_dep = dependency('slash', fallback: ['slash', 'slash_dep'], required: false).partial_dependency(links: false, includes: true)
	param_dep = dependency('param', fallback: ['param', 'param_dep'], required: true).partial_dependency(links: false, includes: true)
	dependencies += dependency('python3', required: true)
else
	dependencies += dependency('csp', fallback: ['csp', 'csp_dep'], required: true)
	dependencies += dependency('param', fallback: ['param', 'param_dep'], required: true).as_link_whole()
	slash_dep = dependency('slash', fallback: ['slash', 'slash_dep'], required: false).as_link_whole()
	dependencies += slash_dep
endif



pycsh_sources = [

	# Module
	'src/pycsh.c',

	# Classes
	'src/parameter/parameter.c',
	'src/parameter/parameterarray.c',
	'src/parameter/pythonparameter.c',
	'src/parameter/parameterlist.c',

	# Wrapper functions
	'src/wrapper/py_csp.c',
	'src/wrapper/slash_py.c',
	'src/wrapper/param_py.c',
	'src/wrapper/dflopt_py.c',
	'src/wrapper/csp_init_py.c',
	'src/wrapper/spaceboot_py.c',
	'src/wrapper/param_list_py.c',
	'src/wrapper/vmem_client_py.c',

	# Utilities
	'src/utils.c',

	# Dependencies from CSH
	'src/csh/lock.c',
	'src/csh/serial.c',
	'src/csh/prometheus.c',
	'src/csh/known_hosts.c',
	'src/csh/param_sniffer.c',
]

if slash_dep.found()
	pycsh_sources += [
		'src/wrapper/slash_py.c',
		'src/csh/csp_init_cmd.c',
	]
	conf.set('PYCSH_HAVE_SLASH', slash_dep.found())
endif

pycsh_config_h = configure_file(output: 'pycshconfig.h', configuration: conf, install_dir: 'include/pycsh/', install: false)
#install_headers(pycsh_config_h, install_dir : 'include/pycsh')

pycsh_sources += vcs_tag(input: files('src/version.c.in'), output: 'version.c', command: ['git', 'describe', '--long', '--always', '--dirty=+'])

# Add .pyi file for type-hints
pyi = configure_file(input: 'pycsh.pyi', output: 'pycsh.pyi', copy: true)

if get_option('build_apm')
	# TODO Kevin: Either statically link with Python, or make sure CSH can find the symbols
	apm_lib = library('csh_pycsh',
					pycsh_sources,
					dependencies : dependencies,
					link_args : ['-Wl,-Map=cshmodtest.map', '-lm'],
					install : true)
else
	# Compile PyCSH for the versions specified in 'python3_versions'.
	# It may only be imported for these versions.
	foreach python_version : get_option('python3_versions')

		py = import('python').find_installation(python_version, pure: false)

		# TODO Kevin: Either statically link with Python, or make sure CSH can find the symbols
		if get_option('build_apm')
			module_name = 'libcsh_pycsh'  # APMs must be prefixed with 'libcsh_'
		else
			module_name = 'pycsh'
		endif

		py.extension_module(module_name, pycsh_sources,
							dependencies : dependencies,
							install : true)
		py.install_sources(pyi)

	endforeach
endif
