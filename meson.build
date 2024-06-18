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
	'param:list_dynamic=true',],
	version: run_command('./get_pip_ersion.py').stdout().strip())

add_global_arguments('-I../include', language: 'c')

conf = configuration_data()

dependencies = []

if get_option('build_apm')
	dependencies += dependency('apm_csh', fallback: ['apm_csh', 'apm_csh_dep'], required: true)
	conf.set('PYCSH_HAVE_APM', 1)  # If we havn't errored, we have APM

	csp_dep = dependency('csp', fallback: ['csp', 'csp_dep'], required: true).partial_dependency(links: false, includes: true)
	slash_dep = dependency('slash', fallback: ['slash', 'slash_dep'], required: false).partial_dependency(links: false, includes: true)
	param_dep = dependency('param', fallback: ['param', 'param_dep'], required: true).partial_dependency(links: false, includes: true)
	dependencies += dependency('python3', required: true)  # We don't actually Python as dep here when we have the linker args, but it might help VSC with highlighting.
else
	dependencies += dependency('csp', fallback: ['csp', 'csp_dep'], required: true)
	dependencies += dependency('param', fallback: ['param', 'param_dep'], required: true).as_link_whole()
	slash_dep = dependency('slash', fallback: ['slash', 'slash_dep'], required: false).as_link_whole()
	dependencies += slash_dep

	curl_dep = dependency('libcurl', required: false)
	if not curl_dep.found()
		error('libcurl not found! Please install libcurl4-openssl-dev or the appropriate package for your system.')
	endif
	dependencies += curl_dep
endif



pycsh_sources = [

	# Module
	'src/pycsh.c',

	# Classes
	'src/parameter/parameter.c',
	'src/parameter/parameterarray.c',
	'src/parameter/pythonparameter.c',
	'src/parameter/pythonarrayparameter.c',
	'src/parameter/pythongetsetparameter.c',
	'src/parameter/pythongetsetarrayparameter.c',
	'src/parameter/parameterlist.c',

	# Wrapper functions
	'src/wrapper/py_csp.c',
	'src/wrapper/apm_py.c',
	'src/wrapper/slash_py.c',
	'src/wrapper/param_py.c',
	'src/wrapper/dflopt_py.c',
	'src/wrapper/csp_init_py.c',
	'src/wrapper/spaceboot_py.c',
	'src/wrapper/param_list_py.c',
	'src/wrapper/vmem_client_py.c',

	# Utilities
	'src/utils.c',
	'src/apm/apm_utils.c',
]

if get_option('build_apm')
	pycsh_sources += [
		# apm.c will start the Python interpreter,
		#	which we aren't interested in when building a normal Python C extension.
		'src/apm/apm.c',
	]
else
	pycsh_sources += [
		# Dependencies from CSH
		'src/csh/vts.c',
		'src/csh/lock.c',
		'src/csh/serial.c',
		'src/csh/prometheus.c',
		'src/csh/known_hosts.c',
		'src/csh/param_sniffer.c',
		'src/csh/hk_param_sniffer.c',
		'src/csh/victoria_metrics.c',
	]
endif

if slash_dep.found()
	pycsh_sources += [
		'src/wrapper/slash_py.c',

		'src/slash_command/slash_command.c',
		'src/slash_command/python_slash_command.c',
	]
	if get_option('build_apm') == false
		pycsh_sources += [
			'src/csh/csp_init_cmd.c',
		]
	else
		pycsh_sources += [
			# apm_slash.c currently only supports extending the CSH "apm load" slash command.
			#	So don't build our version when the original isn't present.
			'src/apm/apm_slash.c',
		]
	endif
	conf.set('PYCSH_HAVE_SLASH', slash_dep.found())
endif

pycsh_config_h = configure_file(output: 'pycshconfig.h', configuration: conf, install_dir: 'include/pycsh/', install: false)
#install_headers(pycsh_config_h, install_dir : 'include/pycsh')

pycsh_sources += vcs_tag(input: files('src/version.c.in'), output: 'version.c', command: ['git', 'describe', '--long', '--always', '--dirty=+'])

# Add .pyi file for type-hints
pyi = configure_file(input: 'pycsh.pyi', output: 'pycsh.pyi', copy: true)

py = import('python').find_installation('python'+get_option('python3_version'), pure: false)

if get_option('build_apm')

	# Automatically generate linker arguments for embedding a Python interpreter.
	# Raw output should look something like: "-L/usr/lib/python3.10/config-3.10-x86_64-linux-gnu -L/usr/lib/x86_64-linux-gnu -lpython3.10 -lcrypt -ldl  -lm -lm"
	# It should be a list when passed to Meson, so we use .split() to get:
	# ['-L/usr/lib/python'+python_version+'/config-'+python_version+'-x86_64-linux-gnu',
	# 		'-L/usr/lib/x86_64-linux-gnu',
	# 		'-lpython'+python_version,
	# 		'-lcrypt',
	# 		'-ldl',
	# 		'-lm']
	python_ldflags = run_command('python'+py.language_version()+'-config', '--ldflags', '--embed').stdout().strip().split()

	# APM names must be prefixed with 'libcsh_'
	py.extension_module('libcsh_pycsh', pycsh_sources,
					dependencies : dependencies,
					install : true,
					# link_with : [static_library('python3')]
					link_args : python_ldflags,
					install_dir : join_paths(get_option('prefix'), 'lib', 'csh'),
					)
	
else  # Not building APM
	py.extension_module('pycsh', pycsh_sources,
					dependencies : dependencies,
					install : true,
					)
endif

py.install_sources(pyi)
