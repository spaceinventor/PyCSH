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

Requirements: build-essential, libsocketcan-dev, can-utils, libzmq3-dev, libyaml-dev, meson

```
sudo apt-get install build-essential libsocketcan-dev can-utils libzmq3-dev libyaml-dev
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
sudo ninja -C builddir/ install
```

### Run
```
import PyCSH

def main():
    PyCSH.param_init()  # Accepts many options, including .yaml file with config.
    # <CODE GOES HERE>

if __name__ == '__main__':
    main()
```