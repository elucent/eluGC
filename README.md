# eluGC

# Overview

eluGC adds rudimentary support for precise tracing GC to C++. While it won't handle allocations made using
existing functions like `new` or `malloc`, it adds a new `object` template whose memory is managed using
a garbage collector.

eluGC is currently a *copying*, *non-generational* collector. This means collections only visit live objects,
and are compacting - so allocations remain quite cheap. One exception to this is object destructors - eluGC
maintains C++ RAII semantics by calling destructors for collected objects and relying on move constructors to
copy objects to the to-space.

# Performance

In terms of costs, eluGC incurs an extra pointer of memory overhead per object. Performance-wise, eluGC unfortunately
doesn't quite compete with ordinary `new` and `delete`. This seems to be primarily due to eluGC's reference discovery
algorithm. While it's theoretically possible to precompute the offsets of reference fields in an object, it generally
requires some modest compiler support. eluGC is entirely userspace, however, so I don't think this is possible
to achieve. This means that the garbage collector checks every possible pointer in an object against a global
bitmap, which ends up being quite expensive for large objects. I'll try to investigate ways of optimizing this in the
future. Overall though, the performance doesn't appear to be too bad, so I think this project still serves as a
nice proof-of-concept. :)

# Usage

Include the `"gc.h"` header in any file you'd like to use garbage collected objects in. Currently, this file will
redefine `main`, allowing the library to wrap the entry point you define. This is kind of gross and I'll see if I
can think of another way eventually. :p

The `object` template stores a garbage-collected pointer to a value. `object<int>` will store a garbage-collected 
integer, for instance.

The default constructor for an `object` will initialize it to a null value. `object`s can be converted to bool,
returning false if null, and true otherwise.

Instances of an object are created using the `make` template. `make` is variadic, accepting arguments that will be
forwarded to the constructor of the desired type. `make<int>(1)`, for instance, will return an `object<int>` with
the value `1`.

Accessing the values in an `object` can be done in one of two ways. First, `object` values can automatically convert
to references to their contained type - `object<int>` can implicitly convert to `int&` or `const int&`. Second,
methods and fields of the inner value can be accessed using the arrow (`->`) operator.

Example code:

```cpp
#include "gc.h"

struct list_t {
    int head;
    object<list_t> tail;
    list_t(int head_in, object<list_t> tail_in): 
        head(head_in), tail(tail_in) {}
};

using list = object<list_t>;

list cons(int h, list t) {
    return make<list_t>(h, t);
}

list append(list a, list b) {
    if (!a) return b;
    if (!b) return a;
    return cons(a->head, append(a->tail, b));
}

list map(list l, int(*func)(int)) {
    if (!l) return l;
    int head = func(l->head);
    return cons(head, map(l->tail, func));
}

int main(int argc, char** argv) {
    list a = cons(1, cons(2, {}));
    list b = cons(3, {});
    list c = append(a, b);
    map(c, [](int i) -> int { return printf("%d ", i), i; });
    printf("\n");
    return 0;
}
```