""" Thin wrapper package for the CSH python bindings.
    Imports with RTLD_GLOBAL, so the module can load APMs. """

import sys as _sys
from types import ModuleType as _ModuleType
from posixpath import expanduser as _expanduser


def _import_pycsh(package_dir: str = None) -> _ModuleType:
    """ Hide as many of our importation dependencies as possible """

    import os
    from _ctypes import PyObj_FromPtr
    from ctypes import CDLL, RTLD_GLOBAL, c_void_p, cast, POINTER, py_object, PyDLL
    from sysconfig import get_config_var

    # Get the suffix for the shared object file (e.g., .cpython-310-x86_64-linux-gnu.so)
    extension_suffix = get_config_var('EXT_SUFFIX')

    # Construct the full name of our expected version of pycsh
    so_filename = f'pycsh{extension_suffix}'

    # Get the directory of the current __init__.py file
    if package_dir is None:
        package_dir = os.path.dirname(__file__)
        package_dir = f"{_expanduser('~')}/.local/lib/python3.10/site-packages/pycsh/"

    # Construct the full path to the shared object file
    so_filepath = os.path.join(package_dir, so_filename)

    # Load the shared object file using ctypes, so can expose C symbols.
    # This is probably very Linux specific, but PyCSH is quite so as well.
    pycsh = PyDLL(so_filepath, mode=RTLD_GLOBAL)

    # Retrieve the initialization function
    PyInit_pycsh = pycsh.PyInit_pycsh
    PyInit_pycsh.restype = c_void_p  # Set the return type to void* to handle the pointer

    # Call the initialization function to get the module
    pycsh_module_ptr = PyInit_pycsh()

    # Cast the returned integer to a Python object (Python module to be exact)
    pycsh_module: _ModuleType = PyObj_FromPtr(pycsh_module_ptr)
    return pycsh_module


# Add pycsh to sys.modules, so we can import everything from it.
try:  # Importing directly from the repository
    _sys.modules['pycsh'] = _import_pycsh('builddir/')
except (ImportError, ModuleNotFoundError, OSError):
    _sys.modules['pycsh'] = _import_pycsh()

# Import everything from the pycsh namespace,
# because ideally this __init__.py would just be the .so file.
from pycsh import *

try:
    from .pycsh import *
except ImportError:
    pass
