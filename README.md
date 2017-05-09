DaaS
====

Data as a Service
-----------------

I know.... Yet another "* as a Service".
This time I had a simple need, so I designed a simple application: I want a simple and fast service to serve my _Data_.

Compilation and execution
-------------------------
- Clone the repository
- Generate SSL / TLS certificate and key files: server.crt and server.key
- Compile
```
export CC=/usr/bin/clang
export CXX=/usr/bin/clang++
cmake -DCMAKE_BUILD_TYPE=Debug PATH_TO_GIT_CLONE
make  # VERBOSE=1

```
