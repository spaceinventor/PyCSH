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

endif

dependencies += dependency('csp', fallback: ['csp', 'csp_dep'], required: true).partial_dependency(links: false, includes: true)
slash_dep = dependency('slash', fallback: ['slash', 'slash_dep'], required: false).partial_dependency(links: false, includes: true)
dependencies += dependency('param', fallback: ['param', 'param_dep'], required: true).partial_dependency(links: false, includes: true)
dependencies += dependency('python3', required: true)  # We don't actually Python as dep here when we have the linker args, but it might help VSC with highlighting.

# CSH (and or libcsh) is reponsible for static linking of libcurl
curl_dep = dependency('libcurl', required: false).partial_dependency(links: false, includes: true)
if not curl_dep.found()
	error('libcurl not found! Please install libcurl4-openssl-dev or the appropriate package for your system.')
endif
dependencies += curl_dep

dependencies += slash_dep


# TODO Kevin: Perhaps let us inspire by the meson.build,
#	they check for the presence of the python.h header.
#	- https://github.com/numpy/numpy/blob/main/meson.build#L44

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
	'src/csp_classes/ident.c',
	#'src/csp_classes/node.c',  # Coming soon...

	# Wrapper functions
	'src/wrapper/py_csp.c',
	'src/wrapper/apm_py.c',
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
endif

if slash_dep.found()
	pycsh_sources += [
		'src/wrapper/slash_py.c',

		'src/slash_command/slash_command.c',
		'src/slash_command/python_slash_command.c',
	]
	if get_option('build_apm') == true
		pycsh_sources += [
			# apm_slash.c currently only supports extending the CSH "apm load" slash command.
			#	So don't build our version when the original isn't present.
			'src/apm/apm_slash.c',
		]
	endif
	conf.set('PYCSH_HAVE_SLASH', slash_dep.found())
endif

# Build/declare CSH dependencies
libcsh_proj = subproject('csh')
libcsh_lib = libcsh_proj.get_variable('csh_lib')
libcsh_lib_path = libcsh_lib.full_path()

dependencies += dependency('csh', fallback: ['csh', 'csh_dep'], required: true).partial_dependency(links: false, includes: true)

pycsh_config_h = configure_file(output: 'pycshconfig.h', configuration: conf, install_dir: 'include/pycsh/', install: false)
#install_headers(pycsh_config_h, install_dir : 'include/pycsh')

pycsh_sources += vcs_tag(input: files('src/version.c.in'), output: 'version.c', command: ['git', 'describe', '--long', '--always', '--dirty=+'])

# Add .pyi file for type-hints
pyi = configure_file(input: 'pycsh.pyi', output: 'pycsh.pyi', copy: true)

# Also __init__.py that ensures we can expose CSH symbols/dependencies.
__init__py = configure_file(input: '__init__.py', output: '__init__.py', copy: true)

py = import('python').find_installation('python'+get_option('python3_version'), pure: false)

if get_option('build_apm')

	# TODO Kevin: .so file doesn't exist at this point, and install_data() isn't lazy. So this doesn't work.
	#install_data(libcsh_lib_path, install_dir : get_option('datadir'))
	py.install_sources(pyi)

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
					# link_with : [static_library('python3')]
					link_args : python_ldflags,
					install : true,
					install_dir : join_paths(get_option('prefix'), 'lib', 'csh'),
					gnu_symbol_visibility: 'default',
					)
	
else  # Not building APM
	py.extension_module('pycsh', pycsh_sources,
					dependencies : dependencies,
					subdir : 'pycsh',
					install : true,
					gnu_symbol_visibility: 'default',
					)

	# TODO Kevin: Is there any sense in also installing sources when building for APM?
	#	We will probably always build both anyway, and linters will look in dist-packages/
	#	rather than ~/.local/lib/csh/
	py.install_sources([pyi, __init__py], subdir: 'pycsh')
endif
