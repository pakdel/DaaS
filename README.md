DaaS
====

Data as a Service
-----------------

I know.... Yet another "* as a Service".
This time I had a simple need, so I designed a simple application: I want a simple and fast service to serve my _Data_.

### Simple
It should be less than 1k lines of code.

### Fast
The overhead on top of the database should be less than 1 ms.
As a rule of thumb, the binary should be smaller than 20 KB.


Compilation and execution
-------------------------

[microhttpd](http://www.gnu.org/software/libmicrohttpd/) and libpq-dev needed as a prerequisite.

```bash
MICROHTTPPD_PATH=/where/libmicrohttpd/is/installed
gcc daemon.c -Wl,-rpath,"${MICROHTTPPD_PATH}/src/microhttpd/.libs" -I${MICROHTTPPD_PATH}/src/include -L${MICROHTTPPD_PATH}/src/microhttpd/.libs -lmicrohttpd -lpq -o micro-daemon

PORT=9000
CONNECTION_STRING='user=username password=password dbname=dbname host=host port=port'
./micro-daemon ${PORT} "${CONNECTION_STRING}"
```

