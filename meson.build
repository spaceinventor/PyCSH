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
	'param:scheduler_client=true',
	'param:collector=true',
	'param:list_dynamic=true',])

add_global_arguments('-I../include', language: 'c')

conf = configuration_data()

csp_dep = dependency('csp', fallback: ['csp', 'csp_dep'], required: true)
slash_dep = dependency('slash', fallback: ['slash', 'slash_dep'], required: false).as_link_whole()
param_dep = dependency('param', fallback: ['param', 'param_dep'], required: true).as_link_whole()


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
	'src/wrapper/param_py.c',
	'src/wrapper/dflopt_py.c',
	'src/wrapper/csp_init_py.c',
	'src/wrapper/spaceboot_py.c',
	'src/wrapper/param_list_py.c',
	'src/wrapper/vmem_client_py.c',

	# Utilities
	'src/utils.c',

	# Dependencies from CSH
	'src/csh/serial.c',
	'src/csh/prometheus.c',
	'src/csh/param_sniffer.c',
]

if slash_dep.found()
	pycsh_sources += ['src/wrapper/slash_py.c',]
	conf.set('PYCSH_HAVE_SLASH', slash_dep.found())
endif

pycsh_config_h = configure_file(output: 'pycshconfig.h', configuration: conf, install_dir: 'include/pycsh/')
install_headers(pycsh_config_h, install_dir : 'include/pycsh')

pycsh_sources += vcs_tag(input: files('src/version.c.in'), output: 'version.c', command: ['git', 'describe', '--long', '--always', '--dirty=+'])

# Compile PyCSH for the versions specified in 'python3_versions'.
# It may only be imported for these versions.
foreach python_version : get_option('python3_versions')

	py = import('python').find_installation(python_version)
	# py.dependency() doesn't work with version constraint. Use plain
	# dependency() instead
	pydep = dependency('python3', version : '>=3.5', required : true)
	py.extension_module('pycsh', [pycsh_sources, pycsh_config_h],
						dependencies : [param_dep, pydep, csp_dep, slash_dep],
						install : true)

endforeach

# Add .pyi file for type-hints
configure_file(input: 'pycsh.pyi', output: 'pycsh.pyi', copy: true)