""" Thin wrapper package for the CSH python bindings.
    Imports with RTLD_GLOBAL, so the module can load APMs. """

import ast as _ast
import sys as _sys
from os.path import dirname
from pathlib import Path as _Path
from types import ModuleType as _ModuleType
from posixpath import expanduser as _expanduser


# def _parse_pyi_signatures():
#     # Locate the .pyi file next to this __init__.py
#     pyi_path = _Path(__file__).with_suffix(".pyi")
#     sig_map = {}
#     if not pyi_path.exists():
#         return sig_map
#     tree = _ast.parse(pyi_path.read_text())
#     for node in tree.body:
#         if isinstance(node, _ast.FunctionDef):
#             arg_count = len(node.args.args)
#             sig_map[node.name] = arg_count
#     return sig_map
def _parse_pyi_for_fuzz(pyi_path: _Path) -> dict[str, list[str]]:
    """Return map: function_name -> list of parameter type hints (strings).
    Type hints are normalized to 'int', 'str', 'bytes', 'float', 'bool', or 'any'.
    """
    sig_map = {}
    assert pyi_path.exists(), f"{pyi_path}"

    try:
        tree = _ast.parse(pyi_path.read_text())
    except Exception:
        return sig_map

    for node in tree.body:
        if isinstance(node, _ast.FunctionDef):
            types = []
            for arg in node.args.args:
                hint = "any"
                if arg.annotation:
                    # annotation could be Name, Subscript, Attribute, etc.
                    if isinstance(arg.annotation, _ast.Name):
                        hint = arg.annotation.id
                    elif isinstance(arg.annotation, _ast.Subscript) and isinstance(arg.annotation.value, _ast.Name):
                        # e.g. Optional[str], List[int] -> take the base name
                        hint = arg.annotation.value.id
                    elif isinstance(arg.annotation, _ast.Attribute):
                        hint = arg.annotation.attr
                    else:
                        hint = "any"
                types.append(hint.lower())
            sig_map[node.name] = types
    return sig_map


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
import_installed_version: bool = False
try:  # Importing directly from the repository
    # Try builddir first, so it's prioritized over system version.
    _sys.modules['pycsh'] = _import_pycsh('builddir/')
except (ImportError, ModuleNotFoundError, OSError):
    # Don't import in except cluase, because we don't want to chain exceptions.
    import_installed_version = True

if import_installed_version:
    _sys.modules['pycsh'] = _import_pycsh()

# Import everything from the pycsh namespace,
# because ideally this __init__.py would just be the .so file.
from pycsh import *

try:
    # Trick stubborn IDEs into showing type-hints, by trying to import them.
    from .pycsh import *
except ImportError:
    pass


# locate pycsh.pyi (assumed to live next to this __init__.py)
_pyi_path = _Path(dirname(__file__)).joinpath('pycsh.pyi')
__fuzz_signatures__ = _parse_pyi_for_fuzz(_pyi_path)
_sys.modules['pycsh'].__fuzz_signatures__ = __fuzz_signatures__
