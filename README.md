# Phalanx

This is *Phalanx*, an open source ASIC cell layout composer. It creates new standard cells by composing existing cells, raw GDS data, routes, and custom geometry. Timing and energy characteristics of the new cell are derived from its constituent parts. Phalanx reads and writes GDS, LEF, and LIB files. Additionally, it supports basic plotting of cell geometries.


## Building

Phalanx is written in plain C and uses CMake to derive the build system for the current platform. Compile the tool as follows:

    mkdir build
    cd build

    # if you want to hack away
    cmake -DCMAKE_BUILD_TYPE=debug ..
    make

    # if you want to install
    cmake ..
    make
    sudo make install


## Usage

Phalanx uses a custom command syntax that is based on scope and context. The API is very unstable and the source code is the best documentation, but generally speaking the following commands are available:

    load_gds "/path/to/file.gds";  # load a GDS file
    load_lef "/path/to/file.lef";  # load a LEF file
    load_lib "/path/to/file.lib";  # load a LIB file

    # Create or edit a cell.
    cell "<name>" {
        set_size <w> <h>;

        # Create or edit an instance of another cell.
        inst "<cellname>" "<instname>" {
            set_postition <x> <y>;
            set_orientation <MX|MY|R90|R180|R270>;
        }
    }

    # Generate GDS output.
    gds "<libname>" {
        add_cell "<cellname>";   # add cell to GDS library
        write_gds "<filename>";  # write GDS library to disk
    }

Other commands exist to connect the pins of instances and calculate timing information. Refer to the `src/main.c` file for more details.


## Roadmap

Phalanx is very much incomplete and has a lot of potential for extension. The following lists some of the corner stones that would help the tool grow. Generally speaking, it would be beneficial to switch to a higher-level language, for example Rust or C++.

- Add a dependency on [libasic](https://github.com/fabianschuiki/libasic) and remove the `src/lib*` and `src/lef*` files.

- Add support for busses that cover multiple pins. This allows for more compact LIB output, but requires that a bus also keeps track of its contamination delay. This then captures the shortest and longest timing arc of the pins that make up thebus.

- Overhaul the way timings are calculated and improve the LIB data generated from that.

- Add proper support for user-guided routing. By using primitive routing cells (e.g. turns, layer changes, etc.), assemble larger routes from them. The `A*` algorithm would be suited for such a controlled routing task.

- Instead of relying on the custom command syntax, create a daemon-based approach where regular command line tools can be used top communicate with a running instance.
