# NML -- The nice markup language

[Readme](https://ef3d0c3e.github.io/NML/readme)

# Build instructions

**Debug build**
```
mkdir build && cd build
cmake .. -DBUILD_TYPE=Debug
make
```

**Release build**
```
mkdir build && cd build
cmake .. -DBUILD_TYPE=Release
make
```

# TODO List
 * Fix `#+Line` variable used for code fragments
 * More toggling with variables
 * Stateless compiler (for threaded compiler)
 * Proper testing
 * Refactoring
 * Language server

# License

NML is licensed under the GNU AGPL version 3.

# Licenses for third party libraries
 * [guile](https://www.gnu.org/software/guile/manual/html_node/Guile-License.html)
 * [fmt](https://github.com/fmtlib/fmt/blob/master/LICENSE.rst)
 * [cxxopts](https://github.com/jarro2783/cxxopts/blob/master/LICENSE)
 * [Catch2](https://github.com/catchorg/Catch2/blob/devel/LICENSE.txt)
 * [utfcpp](https://github.com/nemtrif/utfcpp/blob/master/LICENSE)
