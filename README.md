# TR069 Manager

## Requirement

The cwmpd requires to download and install following libs:
- libevent
- libwebsockets
- libtr69-engine
- libcares
- libnetmodel
- libfiletransfer
- amx libs

### libevent
```
# download
git clone https://github.com/libevent/libevent.git
cd libevent
git checkout release-2.1.8-stable #not required but it's the latest stable release for now

# build and install: compile with fPIC flag (CMAKE_POSITION_INDEPENDENT_CODE)
mkdir build && cd build
cmake ../ -DCMAKE_POSITION_INDEPENDENT_CODE=ON
make && sudo make install
```

### libwebsocket
```
# download
git clone https://github.com/warmcat/libwebsockets.git
cd libwebsockets
git checkout v4.2.0 #required 4.2.0 or greater version

# build and install
mkdir build && cd build
cmake ../ -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DLWS_WITH_LIBEVENT=ON -DLWS_MAX_SMP=10 -DLWS_WITH_TLS=ON
make && sudo make install
```

### libtr69-engine
```
# download
git clone git@gitlab.com:prpl-foundation/components/core/libraries/libtr69-engine.git
cd libtr69-engine

# build and install
make && sudo make install
```

### libcares
```
# download
git clone https://github.com/c-ares/c-ares.git
cd c-ares
git checkout cares-1_17_2 # required 1_17_2 or greater

# build and install
./buildconf
./configure --prefix=/usr --disable-static
make && sudo make install
```

### libnetmodel
```
# download
git clone git@gitlab.com:prpl-foundation/components/core/libraries/libnetmodel.git
cd libnetmodel

# build and install
make && sudo make install
```
### libfiletransfer
```
# download
git clone git@gitlab.com:prpl-foundation/components/core/libraries/libfiletransfer.git
cd libfiletransfer

# build and install
make && sudo make install
```

### amx libs
Build and install amx libs

https://gitlab.com/prpl-foundation/components/ambiorix/libraries

## Installation
Build and install TR069 Manager
```
make && sudo make install
```

## Secure connection attributes

The secure connection attributes are available through the configuration file /etc/amx/cwmp_plugin/cwmpd-config.odl

### cwmpd_certificate

A path name of a row in the Security.Certificate table. It is according to already standard solution from HTTPAccess: https://usp-data-models.broadband-forum.org/tr-181-2-19-1-usp.html#D.Device:2.Device.UserInterface.HTTPAccess.Certificate.

E.g. cwmpd_certificate="Security.Certificate.1."

cwmpd_client_certs and cwmpd_client_privatekey are deprecated and will be ignored if cwmpd_certificate is specified.


## UNIT Tests
Unit tests
```
make test
```
