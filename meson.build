project('PyCSH', 'c', 
	subproject_dir: 'lib', 
	meson_version: '>=1.8.1',
	default_options: [
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
	version: run_command('./get_pip_ersion.py', check: true).stdout().strip()
)

py = import('python').find_installation('python'+get_option('python3_version'), pure: false)

dependencies = [
	dependency('pycsh_core', fallback: ['pycsh_core', 'pycsh_core_dep']).as_link_whole()
]
python_ldflags = run_command('python'+py.language_version()+'-config', '--ldflags', '--embed', check: true).stdout().strip().split()
pycsh_ext = py.extension_module(
	'pycsh',
	[
		'src/python_host.c',
	],
	dependencies : dependencies,
	link_args : python_ldflags + ['-Wl,-Map=' + meson.project_name() + '.map'],
	install : true,
	subdir: 'pycsh',
	gnu_symbol_visibility: 'default',
)

# Add .pyi file for type-hints
pyi = configure_file(input: 'pycsh.pyi', output: 'pycsh.pyi', copy: true)
# Also __init__.py that ensures we can expose CSH symbols/dependencies.
__init__py = configure_file(input: '__init__.py', output: '__init__.py', copy: true)
py.install_sources([pyi, __init__py], subdir: 'pycsh')
