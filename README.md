### PyCSH

Python bindings to spaceinventor/csh

Exposes most CSH commands as functions. 
Also exposes wrapper classes for param_t's and a class providing an interface to param_queue_t.
These provide an object-oriented interface to libparam, but are largely meant for convenience.

See pycsh.pyi for full interface documentation.

Compilation happens automatically when importing, 
but may be done manually for specific Python versions by editing python3_versions in meson_options.txt. 
The versions specified there must be installed when compiling manually.

### Dependencies

Requirements: build-essential, libsocketcan-dev, can-utils, libzmq3-dev, libyaml-dev, meson, pkg-config

```
sudo apt-get install build-essential libsocketcan-dev can-utils libzmq3-dev libyaml-dev pkg-config
sudo pip3 install meson ninja
```

### Normal installation

Github pip install
```
pip install git+https://github.com/spaceinventor/PyCSH.git
```

PyCSH can be used in a pip `requirement.txt` file (using a git reference) or as a [poetry](https://python-poetry.org/) dependency.

Example in a `requirements.txt` file:

```
-e git+https://github.com/spaceinventor/PyCSH.git
```

### Setup for developers

Setup
```
git clone --recurse-submodules https://github.com/spaceinventor/PyCSH.git 
cd PyCSH

./configure
```

Build & Install
```
# Recompiling the repository will not affect system-wide imports.
# Does not include .pyi file API documentation for IDEs

./install
```

Install repo as package
```
# Recompiling the repository will affect system-wide imports.
# Includes .pyi file API documentation for IDEs

pip3 -e install.
```

Build (No install)
```
./build
```

### Run
```
import pycsh

def main():
    pycsh.init()
    pycsh.csp_init()
    # <CODE GOES HERE>

if __name__ == '__main__':
    main()
```
