project('csh_bindings', 'c', subproject_dir: 'lib', default_options: [
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
	'param:have_fopen=true',
	'param:slash=true',  # TODO Kevin: How should param:slash work if slash is optional
	'slash:builtins=true',
	'param:commands=false',
	'param:scheduler=false',
	'param:commands_client=true',
	'param:scheduler_client=true',
	'param:collector=false',
	'param:list_dynamic=true',],
	version: run_command('./lib/pycsh_core/get_pip_ersion.py').stdout().strip(),)

add_global_arguments('-I../include', language: 'c')

dependencies = [
	dependency('pycsh_core', fallback: ['pycsh_core', 'pycsh_core_dep'], required: true),
	#dependency('csp', fallback: ['csp', 'csp_dep'], required: true).partial_dependency(links: false, includes: true),
	#dependency('param', fallback: ['param', 'param_dep'], required: true).partial_dependency(links: false, includes: true),
	#dependency('slash', fallback: ['slash', 'slash_dep'], required: false).partial_dependency(links: false, includes: true),
]

# TODO Kevin: Perhaps let us inspire by the meson.build of numpy,
#	they check for the presence of the python.h header.
#	- https://github.com/numpy/numpy/blob/main/meson.build#L44

pycsh_sources = [
]

subproject('pycsh_core')
