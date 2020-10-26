#ifndef _ELUGC_HEAP_H
#define _ELUGC_HEAP_H

#include "defs.h"

namespace eluGC {
    void alloc(void** ptr, HeapWord size);
    void claim(void* const* ptr);
    void unclaim(void* const* ptr);
}

#endif