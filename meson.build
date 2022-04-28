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
	'param:have_fopen=true',
	'param:slash=false',
	'param:commands=false',
	'param:scheduler=false',
	'param:commands_client=true',
	'param:scheduler_client=true',
	'param:collector=true',
	'param:list_dynamic=true',])

add_global_arguments('-I../include', language: 'c')

csp_dep = dependency('csp', fallback: ['csp', 'csp_dep'], required: true)
param_dep = dependency('param', fallback: ['param', 'param_dep'], required: true).as_link_whole()


pycsh_sources = [

	# Module
	'src/pycsh.c',

	# Classes
	'src/parameter/parameter.c',
	'src/parameter/parameterarray.c',
	'src/parameter/parameterlist.c',

	# Wrapper functions
	'src/wrapper.c',

	# Utilities
	'src/utils.c',

	# Dependencies from CSH
	'src/csh/serial.c',
	'src/csh/prometheus.c',
	'src/csh/param_sniffer.c',
]

# Compile PyCSH for the versions specified in 'python3_versions'.
# It may only be imported for these versions.
foreach python_version : get_option('python3_versions')

	py = import('python').find_installation(python_version)
	# py.dependency() doesn't work with version constraint. Use plain
	# dependency() instead
	pydep = dependency('python3', version : '>=3.5', required : true)
	py.extension_module('pycsh', pycsh_sources,
						dependencies : [param_dep, pydep, csp_dep],
						install : true)

endforeach
