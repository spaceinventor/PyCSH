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

### Run
```
import PyCSH

def main():
    PyCSH.init()  # Accepts many options, including .yaml file with config.
    # <CODE GOES HERE>

if __name__ == '__main__':
    main()
```