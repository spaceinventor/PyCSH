### PyCSH

Python bindings to spaceinventor/csh

Exposes most CSH commands as functions. 
Also exposes wrapper classes for param_t's and a class providing an interface to param_queue_t.
These provide an object-oriented interface to libparam, but are largely meant for convenience.

See pycsh.pyi for full interface documentation.

Compilation happens automatically when importing, 
but may be done manually for specific Python versions by editing python3_versions in meson_options.txt. 
The versions specified there must be installed when compiling manually.

### Build

Requirements: build-essential, libsocketcan-dev, can-utils, libzmq3-dev, libyaml-dev, meson, pkg-config

```
sudo apt-get install build-essential libsocketcan-dev can-utils libzmq3-dev libyaml-dev pkg-config
sudo pip3 install meson ninja
```

Sometimes needed:
```
link /usr/sbin/ninja /usr/local/lib/python3.5/dist-packages/ninja
```

Setup
```
git clone https://github.com/spaceinventor/PyCSH.git
cd PyCSH
git submodule update --init --recursive

meson . builddir
ninja -C builddir
```

Build (No install)
```
ninja -C builddir
```

Build & Install (Optional)
```
# Recompiling the repository will not affect system-wide imports.
# Does not include .pyi file API documentation for IDEs

sudo ninja -C builddir/ install
# Uninstall with: sudo rm /usr/local/lib/python3/dist-packages/pycsh.cp*.so
```

Install repo as package (Optional)
```
# Recompiling the repository will affect system-wide imports.
# Includes .pyi file API documentation for IDEs

sudo ln -s <FULL/PATH/TO/PyCSH/REPO>/PyCSH /usr/lib/python3/dist-packages/PyCSH
# Uninstall by deleting: /usr/lib/python3/dist-packages/PyCSH
```

## Build a pip installable binary wheel package

The `install` method above is not very friendly to using PyCSH in a virtual environment (why should we not do that ?), the following method allows PyCSH to be more Python-scosystem friendly.

```
jbl@jbl-ThinkPad-T15-Gen-1:~/workspace/PyCSH$ python3 -m pip wheel .
Processing /home/jbl/workspace/PyCSH
  Installing build dependencies ... done
  Getting requirements to build wheel ... done
  Installing backend dependencies ... done
  Preparing metadata (pyproject.toml) ... done
Building wheels for collected packages: PyCSH
  Building wheel for PyCSH (pyproject.toml) ... done
  Created wheel for PyCSH: filename=pycsh-0.0.1-cp310-cp310-linux_x86_64.whl size=147628 sha256=c669edfe492a46027ab316c648b706d149dae92a9486d6ed704085a05df9e7d2
  Stored in directory: /tmp/pip-ephem-wheel-cache-9yval4e6/wheels/80/73/10/7d67ed7195a8b108e2e143578f305e3ae9c55e39b807c4cc8f
Successfully built PyCSH
jbl@jbl-ThinkPad-T15-Gen-1:~/workspace/PyCSH$ 
```

This will produce a `pycsh-0.0.1-cp310-cp310-linux_x86_64.whl` that can be installed using pip:

```
jbl@jbl-ThinkPad-T15-Gen-1:~/workspace/PyCSH$ pip install pycsh-0.0.1-cp310-cp310-linux_x86_64.whl
Defaulting to user installation because normal site-packages is not writeable
Processing ./pycsh-0.0.1-cp310-cp310-linux_x86_64.whl
Installing collected packages: pycsh
Successfully installed pycsh-0.0.1
jbl@jbl-ThinkPad-T15-Gen-1:~/workspace/PyCSH$
```

This also means that PyCSH can be used in a pip `requirement.txt` file (using a git reference) or as a [poetry](https://python-poetry.org/) dependency.

Example in a `requirements.txt` file:

-e git+https://github.com/spaceinventor/PyCSH.git

### Run
```
import PyCSH

def main():
    PyCSH.init()  # Accepts many options, including .yaml file with config.
    # <CODE GOES HERE>

if __name__ == '__main__':
    main()
```