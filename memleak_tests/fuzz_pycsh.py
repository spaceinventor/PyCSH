""" Don't run this directly, run `./fuzz_pycsh.py` instead """

import os
import pycsh
import inspect
import struct
from pythonfuzz.main import PythonFuzz

__fuzz_signatures__ = pycsh.__fuzz_signatures__

def generate_args_from_buf(buf, sig):
    args = []
    offset = 0
    try:
        vals = sig.parameters.values()
    except AttributeError:
        vals = sig

    for param in vals:
        if offset + 4 <= len(buf):
            val = struct.unpack_from('i', buf, offset)[0]
            offset += 4
        else:
            val = 0
        args.append(val)
    return args

initialized = False

@PythonFuzz
def fuzz(raw_bytes: bytes):
    global initialized
    # Skip csp_init and init
    for attr_name, attr_value in pycsh.__dict__.items():

        if not callable(attr_value):
            continue
        func = attr_value
        if not initialized and func in {pycsh.csp_init, pycsh.init}:
            continue
        sig = __fuzz_signatures__.get(attr_name)
        if sig is None:
            try:
                sig = inspect.signature(func)
            except ValueError:
                continue
        try:
            args = generate_args_from_buf(raw_bytes, sig)
            print(f"Calling: `{attr_name}({', '.join(str(arg) for arg in args)})`")
            func(*args)
        except RuntimeError as e:
            if not initialized and "Cannot perform operations before .init() has been called." in str(e):
                pass
            else:
                raise
        except Exception as e:
            #print(e, sig, type(e))
            pass

if __name__ == '__main__':
    # First fuzz pass - uninitialized
    fuzz()
    # Then call init
    pycsh.csp_init()
    initialized = True
    # Run fuzz again - initialized
    fuzz()
