conf = configuration_data()

include_dir = include_directories('.', 'include')

dependencies = [
	dependency('python3', required: true),  # We don't actually Python as dep here when we have the linker args, but it might help VSC with highlighting.
]

slash_dep = dependency('slash', fallback: ['slash', 'slash_dep'], required: true)

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
endif
conf.set('PYCSH_HAVE_SLASH', slash_dep.found())

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
dependencies += [
	dependency('apm_csh', fallback: ['apm_csh', 'apm_csh_dep'], required: true),
	dependency('param', fallback: ['param', 'param_dep'], required: true),
	dependency('csp', fallback: ['csp', 'csp_dep'], required: true),
	slash_dep,
]

python_host_dep = dependency('python_host', fallback: ['csh', 'csh_dep'], required: true)

python_ldflags = run_command('python'+py.language_version()+'-config', '--ldflags', '--embed', check: true).stdout().strip().split()
pycsh_ext = py.extension_module(
	'pycsh',
	[
		'python_host.c',
		vcs_tag(input: files('src/version.c.in'), output: 'version.c', command: ['git', 'describe', '--long', '--always', '--dirty=+'])
	] + pycsh_sources,
	dependencies : dependencies + [python_host_dep],
	link_args : python_ldflags + ['-Wl,-Map=' + meson.project_name() + '.map'],
	install : true,
	subdir: 'pycsh',
	gnu_symbol_visibility: 'default',
)

# TODO Kevin: Is there any sense in also installing sources when building for APM?
#	We will probably always build both anyway, and linters will look in dist-packages/
#	rather than ~/.local/lib/csh/
py.install_sources([pyi, __init__py], subdir: 'pycsh')
