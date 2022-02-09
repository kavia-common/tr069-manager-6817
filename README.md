# TR069 Manager

## Requirement

The cwmpd require libevent, libwebsockets, libtr69-engine and libcares + amx libs

### libevent

Download and install libevent

```
git clone https://github.com/libevent/libevent.git
cd libevent
git checkout release-2.1.8-stable #not required but it's the latest stable release for now
```
Configure and build the lib , it should be compiled with fPIC flag (CMAKE_POSITION_INDEPENDENT_CODE)

```
mkdir build
cd build
cmake ../ -DCMAKE_POSITION_INDEPENDENT_CODE=ON
make
sudo make install
```

### libwebsocket

Download and install libwebsocket

Download libwebsockets
```
git clone https://github.com/warmcat/libwebsockets.git
cd libwebsockets
git checkout v4.2.0 #required 4.2.0 or greater version
```
Configure and build the lib

```
mkdir build
cd build
cmake ../ -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DLWS_WITH_LIBEVENT=ON -DLWS_MAX_SMP=10 -DLWS_WITH_TLS=ON
make
sudo make install
```
### libtr69-engine

Download and install libtr69-engine

Download libtr69-engine
```
git clone git@gitlab.com:soft.at.home/libraries/libtr69-engine.git
cd libtr69-engine
```
Configure and build the lib

```
make
sudo make install
```
### libcares

Download and install libcares

Download libtr69-engine
```
git clone https://github.com/c-ares/c-ares.git
cd c-ares
git checkout c-ares-1_17_2 # required 1_17_2 or greater
```
Configure and build the lib

```
./buildconf
./configure --prefix=/usr --disable-static
make
sudo make install
```

## Installation

You can build and install TR069 Manager
```
make && sudo make install
```
## UNIT Tests
Unit tests
```
make test
```
