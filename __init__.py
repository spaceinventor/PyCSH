""" Thin wrapper package for the CSH python bindings. Automatically attempts to compile when import fails. """

try:  # Import everything from the bindings themselves.
    try:  # Using relative import.
        from .builddir.pycsh import *
    except ImportError:  # When attempting to import with no known parent package.
        from builddir.pycsh import *
except ModuleNotFoundError:  # Let's try and recompile the bindings.
    import os as _os
    import sys as _sys
    _abspath = _os.path.dirname(__file__)  # Use full paths for safety.
    print('Failed to import libparam Python bindings attempting to recompile.')
    # Compile for our current version of Python.
    _os.system(f"rm -r {_abspath}/builddir; "
               f"meson {_abspath}/ {_abspath}/builddir -Dpython3_versions=python3.{_sys.version_info.minor} && "
               f"ninja -C {_abspath}/builddir")
    try:  # Using relative import.
        from .builddir.pycsh import *  # Welp, here goes nothing...
    except ImportError:  # When attempting to import with no known parent package.
        from builddir.pycsh import *

# Also try importing the interface documentation, so that autocomplete starts working.
try: from .pycsh import *
except ImportError: pass
