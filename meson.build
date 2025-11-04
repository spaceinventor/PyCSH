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
		'csp:rtable_size=100',
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
	dependency('slash', fallback: ['slash', 'slash_dep'], required: true).as_link_whole(),
	dependency('pycsh_core', fallback: ['pycsh_core', 'pycsh_core_dep']).as_link_whole(),
	dependency('libcurl', not_found_message: 'libcurl not found! Please install libcurl4-openssl-dev or the appropriate package for your system.'),
]
python_ldflags = run_command('python'+py.language_version()+'-config', '--ldflags', '--embed', check: true).stdout().strip().split()
pycsh_ext = py.extension_module(
	'pycsh',
	[
		'src/python_host.c',
		'src/csh_defaults.c',
		'src/known_hosts.c',

		# HK symbol dependencies
		# We did remove these from PyCSH at one point,
		#	but now they're needed again.
		'src/hk_param_sniffer.c',
		'src/param_sniffer.c',
		'src/victoria_metrics.c',
		'src/vts.c',
	],
	dependencies : dependencies,
	link_args : python_ldflags + ['-Wl,-Map=' + meson.project_name() + '.map'],
	install : true,
	subdir: 'pycsh',
	gnu_symbol_visibility: 'default',
)

# Also __init__.py that ensures we can expose CSH symbols/dependencies.
__init__py = configure_file(input: '__init__.py', output: '__init__.py', copy: true)
py.install_sources([__init__py], subdir: 'pycsh')
