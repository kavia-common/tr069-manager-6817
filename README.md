# TR069 Manager

## Requirement

The cwmpd require libevent/libwebsockets and libtr69-engine to be installed on the target
if you are compiling and using this on docker please download and install them manually.
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
cmake ../ -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DLWS_WITH_LIBEVENT=ON -DLWS_WITH_SYS_FAULT_INJECTION=ON -DLWS_WITH_SECURE_STREAMS=ON -DLWS_WITH_SYS_ASYNC_DNS=ON 
make
sudo make install
```
### libtr69-engine

Download and install libtr69-engine

Download libtr69-engine
```
git clone git@gitlab.com:soft.at.home/libraries/libtr69-engine.git
cd libtr69-engine
git checkout dev_init
```
Configure and build the lib

```
make
sudo make install
```

## Installation

You can build and install TR069 Manager
```
make && sudo make install
```
## UNIT Tests
Unit tests require ubusd and they should be executed with root access
```
sudo -E make test
```
