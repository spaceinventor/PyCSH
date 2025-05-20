project('PyCSH', 'c', subproject_dir: 'lib', default_options: [
	'buildtype=debug',
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
	'slash:builtins=true',
	'param:have_fopen=true',
	'param:collector=false',
	'param:list_dynamic=true',
	],
	version: run_command('./pycsh_core/get_pip_ersion.py', check: true).stdout().strip(),)

add_project_arguments('-I../include', language: 'c')

dependencies = []

# TODO Kevin: Perhaps let us inspire by the meson.build of numpy,
#	they check for the presence of the python.h header.
#	- https://github.com/numpy/numpy/blob/main/meson.build#L44

pycsh_sources = [
]

subdir('pycsh_core')
