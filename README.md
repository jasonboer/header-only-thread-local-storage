
A portable header-only TLS implementation.

Distributed under the Boost Software License.

It has the advantage of working when thread creation and destruction is not
under the control of your code as required with C++11 and Boost TLS.

It provides an optional callback the first time a thread's TLS is requested
and when a thread exits for easy setup and tear down.

```

Tested with:

clang version 3.6.0
g++ version 4.9.2
Visual Studio 2013 and 2015

Untested on OSX.

To use:

#define TLS_IMPLEMENT_ 1

#include "ThreadLocalStorage.h"

struct tls_data_t {
	int * ptr;
	tls_data_t() : ptr(nullptr) {}
};

using tls = TLS::BASE<tls_data_t>;

Then define:

tls::thread_enter(thread_enter);

and

tls::thread_exit(thread_exit);

if desired.

The per-thread data is accessed with:

auto & data = tls::acquire();

and 

data._data is the provided type in the "using tls" above.

example.cpp demonstrates usage.

```


