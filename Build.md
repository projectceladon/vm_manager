Building from source
===========================

# Pre-requisites

## Linux

```sh
 $ [sudo] apt-get install build-essential autoconf libtool pkg-config cmake
```

# Build
```sh
 $ mkdir build && cd build
 $ cmake -DCMAKE_BUILD_TYPE=Release ..
 $ cmake --build . --config Release
```
