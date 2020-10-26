#ifndef _ELUGC_OBJECT_H
#define _ELUGC_OBJECT_H

#include "defs.h"
#include "heap.h"
#include "stdlib.h"
#include "stdio.h"
#include <utility>
#include <new>

template<typename T = void>
class object final {
    struct metadata {
        HeapWord size;
        void(*destructor)(const void*);
        void(*copyctor)(void*, const void*);
    };

    struct data {
        const metadata* meta;
        T value;
    };
    data* ptr;

    static void raw_dtor(const void* obj) {
        ((data*)obj)->~data();
    }

    static void raw_copy(void* dst, const void* src) {
        new (dst) data(std::move(*(data*)src));
    }

    // need C++17 for this
    inline constexpr static const metadata META = {
        (sizeof(const metadata*) + sizeof(T) + 7) / 8 * 8, raw_dtor, raw_copy
    };

    template<typename U, typename... Args>
    friend object<U> make(Args&&... args);
    
    inline object(const T& t) {
        eluGC::alloc((void**)&ptr, META.size);
        new(ptr) data{&META, t};
    }
public:
    inline object(): ptr(nullptr) {}

    inline object(const object& other): ptr(other.ptr) {
        // printf("copy ctor\n");
        if (ptr) eluGC::claim((void* const*)&ptr);
    }

    inline object& operator=(const object& other) {
        // printf("copy assign\n");
        if (&other != this) {
            if (ptr && !other.ptr) eluGC::unclaim((void* const*)&ptr);
            ptr = other.ptr;
            if (ptr) eluGC::claim((void* const*)&ptr);
        }
        return *this;
    }

    inline object(object&& other) {
        // printf("move ctor\n");
        if (other.ptr) eluGC::unclaim((void* const*)&other.ptr);
        ptr = other.ptr;
        if (ptr) eluGC::claim((void* const*)&ptr);
    }

    inline object& operator=(object&& other) {
        // printf("move assign\n");
        if (&other != this) {
            if (other.ptr) eluGC::unclaim((void* const*)&other.ptr);
            if (ptr && !other.ptr) eluGC::unclaim((void* const*)&ptr);
            ptr = other.ptr;
            if (ptr) eluGC::claim((void* const*)&ptr);
        }
        return *this;
    }

    inline ~object() {
        // printf("destructor %lx\n", &ptr);
        eluGC::unclaim((void* const*)&ptr);
    }

    inline operator bool() const {
        return ptr;
    }

    inline const T& get() const {
        return ptr->value;
    }

    inline T& get() {
        return ptr->value;
    }

    inline const T* operator->() const {
        return &ptr->value;
    }

    inline T* operator->() {
        return &ptr->value;
    }
};

template<typename T, typename... Args>
inline object<T> make(Args&&... args) {
    return object<T>(T(std::forward<Args>(args)...));
}

#endif