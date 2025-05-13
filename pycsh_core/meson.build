conf = configuration_data()

include_dir = include_directories('.', 'include')

dependencies = [
	dependency('python3', required: true),  # We don't actually Python as dep here when we have the linker args, but it might help VSC with highlighting.
]

# We link slash.as_link_whole() through libcsh.as_link_whole()
slash_dep = dependency('slash', fallback: ['slash', 'slash_dep'], required: false)#.as_link_whole()

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
	'src/csp_classes/vmem.c',
	'src/csp_classes/ifstat.c',
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
	# 'src/wrapper/victoria_metrics_py.c',

	# Utilities
	'src/utils.c',
	'src/apm/apm_utils.c',

]

if slash_dep.found()
	pycsh_sources += [
		'src/wrapper/slash_py.c',

		'src/slash_command/slash_command.c',
		'src/slash_command/python_slash_command.c',
	]
		pycsh_sources += [
			# apm_slash.c currently only supports extending the CSH "apm load" slash command.
			#	So don't build our version when the original isn't present.
			'src/apm/apm_slash.c',
		]
	conf.set('PYCSH_HAVE_SLASH', slash_dep.found())
endif




pycsh_config_h = configure_file(output: 'pycshconfig.h', configuration: conf, install_dir: 'include/csh/', install: false)

# Add .pyi file for type-hints
pyi = configure_file(input: 'pycsh.pyi', output: 'pycsh.pyi', copy: true)

# Also __init__.py that ensures we can expose CSH symbols/dependencies.
__init__py = configure_file(input: '__init__.py', output: '__init__.py', copy: true)

py = import('python').find_installation('python'+get_option('python3_version'), pure: false)

# # Include CSH headers, to be fulfilled when loading as APM
# dependencies += [
# 	dependency('csh', fallback: ['csh', 'csh_dep'], required: true).partial_dependency(links: false, includes: true),
# 	dependency('csp', fallback: ['csp', 'csp_dep'], required: true).partial_dependency(links: false, includes: true),
# 	dependency('param', fallback: ['param', 'param_dep'], required: true).partial_dependency(links: false, includes: true),#.as_link_whole(),  # NOTE: .as_link_whole() gives linking errors here, but is otherwise needed to preserve symbols
# 	slash_dep.partial_dependency(links: false, includes: true),
# ]

# # TODO Kevin: .so file doesn't exist at this point, and install_data() isn't lazy. So this doesn't work.
# #install_data(libcsh_lib_path, install_dir : get_option('datadir'))
# py.install_sources(pyi)

# # Automatically generate linker arguments for embedding a Python interpreter.
# # Raw output should look something like: "-L/usr/lib/python3.10/config-3.10-x86_64-linux-gnu -L/usr/lib/x86_64-linux-gnu -lpython3.10 -lcrypt -ldl  -lm -lm"
# # It should be a list when passed to Meson, so we use .split() to get:
# # ['-L/usr/lib/python'+python_version+'/config-'+python_version+'-x86_64-linux-gnu',
# # 		'-L/usr/lib/x86_64-linux-gnu',
# # 		'-lpython'+python_version,
# # 		'-lcrypt',
# # 		'-ldl',
# # 		'-lm']
# python_ldflags = run_command('python'+py.language_version()+'-config', '--ldflags', '--embed').stdout().strip().split()

# # APM names must be prefixed with 'libcsh_'
# pycsh_core_lib = py.extension_module('libcsh_pycsh', pycsh_sources,
# 				dependencies : dependencies,
# 				# link_with : [static_library('python3')]
# 				link_args : python_ldflags,
# 				install : true,
# 				install_dir : join_paths(get_option('prefix'), 'lib', 'csh'),
# 				gnu_symbol_visibility: 'default',
# 				)
dependencies += [
	# dependency('csh', fallback: ['csh', 'csh_dep'], required: true).as_link_whole(),
	dependency('apm', fallback: ['apm', 'apm_dep'], required: true),
	dependency('csp', fallback: ['csp', 'csp_dep'], required: true),
	dependency('param', fallback: ['param', 'param_dep'], required: true),
	slash_dep,
]
python_ldflags = run_command('python'+py.language_version()+'-config', '--ldflags', '--embed').stdout().strip().split()
pycsh_core_lib = library(
	'pycsh', 
	pycsh_sources,
	dependencies : dependencies,
	install : true,
)

# TODO Kevin: Is there any sense in also installing sources when building for APM?
#	We will probably always build both anyway, and linters will look in dist-packages/
#	rather than ~/.local/lib/csh/
py.install_sources([pyi, __init__py], subdir: 'pycsh')

pycsh_core_dep = declare_dependency(link_args: python_ldflags, link_with: pycsh_core_lib, dependencies : dependencies, include_directories: include_dir)
meson.override_dependency('pycsh_core', pycsh_core_dep)
