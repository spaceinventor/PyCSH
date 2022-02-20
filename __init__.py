""" Thin wrapper package for the libparam python bindings. """

try:  # Import everything from the bindings themselves.
    from builddir.libparam_py3 import *
except (ImportError, ModuleNotFoundError):  # Let's try and recompile the bindings.
    import os as _os
    _abspath = _os.path.dirname(__file__)  # Use full paths for safety.
    print('Failed to import libparam Python bindings attempting to recompile.')
    _os.system(f"rm -r {_abspath}/builddir; meson {_abspath}/ {_abspath}/builddir && ninja -C {_abspath}/builddir")
    from builddir.libparam_py3 import *  # Welp, here goes nothing...

# Also try importing the interface documentation, so that autocomplete starts working.
try: from libparam_py3 import *
except ImportError: pass
