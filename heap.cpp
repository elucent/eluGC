#include "heap.h"
#include "sys/mman.h"
#include "string.h"
#include "stdio.h"

uint8_t* getSP() {
    void* ptr = nullptr;
    return (uint8_t*)(((uintptr_t)&ptr + 4095) & 4095); // bad! page size should not be hardcoded
}

constexpr const HeapWord HEAP_SIZE = 1 << 23;
constexpr const HeapWord GEN_SIZE = HEAP_SIZE / 2;
constexpr const HeapWord HEAPMAP_SIZE = HEAP_SIZE / (8 * sizeof(HeapWord));
constexpr const HeapWord STACK_SIZE = 1 << 23; // bad! stack size should not be hardcoded
constexpr const HeapWord STACKMAP_SIZE = STACK_SIZE / (8 * sizeof(HeapWord));

static uint8_t* heap = (uint8_t*)mmap(0, HEAP_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
static uint8_t* gen = heap;
static uint8_t* heapMap = (uint8_t*)mmap(0, HEAPMAP_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
static uint8_t* stack = 0;
static uint8_t* stackMap = (uint8_t*)mmap(0, STACKMAP_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
static uint8_t* heapTop = heap;

static int _1 = (memset(heapMap, 0, HEAPMAP_SIZE), 0),
           _2 = (memset(stackMap, 0, STACKMAP_SIZE), 0);

namespace eluGC {
    void printHeap(void* heap) {
        for (uint64_t i = 0; i < GEN_SIZE / 4; i ++) {
            printf("%04d", *((int*)gen + i));
            if (i % 16 == 0) printf("\n");
            else printf(" ");
        }
        printf("\n");
        fflush(stdout);
    }

    void printMap(void* map) {
        for (uint64_t i = 0; i < STACKMAP_SIZE / 8; i ++) {
            printf("%08lx ", ((uint64_t*)stackMap)[i]);
            if (i % 8 == 0) printf("\n");
        }
        printf("\n");
    }

    using copier_t = void(*)(void*, const void*);
    using destructor_t = void(*)(const void*);

    HeapWord META_MASK = 0x7ffffffffffffffful;

    HeapWord get_size(void* meta) {
        return *(HeapWord*)((HeapWord)meta & META_MASK);
    }

    copier_t get_copier(void* meta) {
        return *(void(**)(void*, const void*))((uint8_t*)meta + sizeof(HeapWord) + sizeof(void(*)(const void*)));
    }

    destructor_t get_destructor(void* meta) {
        return *(void(**)(const void*))((uint8_t*)meta + sizeof(HeapWord));
    }

    inline bool markedHeap(void* ptr) {
        if (ptr >= gen && ptr < gen + GEN_SIZE) {
            HeapWord idx = ((uint8_t*)ptr - gen) / (8 * sizeof(HeapWord));
            return stackMap[idx] & 1 << ((uint8_t*)ptr - gen) / sizeof(HeapWord) % 8;  
        }
        return false;
    }

    void copy(uint8_t**, uint8_t**);

    inline void visitFields(HeapWord remainingBytes, uint8_t* fields, uint8_t** to) {
        for (int i = 0; i < remainingBytes; i += sizeof(HeapWord)) {
            if (markedHeap(fields + i)) copy((uint8_t**)fields + i, to);
        }
    }

    void copy(uint8_t** from, uint8_t** to) {
        void* meta = *(void**)*from;
        if ((HeapWord)meta & ~META_MASK) return; // visited already
        HeapWord size = get_size(meta);
        get_copier(meta)(*to, *from);
        auto base = *from + sizeof(const void*); // visit fields;
        *from = (uint8_t*)*to;
        *to += size;
        visitFields(size - sizeof(HeapWord), *from + sizeof(HeapWord), to);
    }

    void collect() {
        uint8_t* to = gen == heap ? heap + GEN_SIZE : heap;
        uint8_t* toTop = to;

        HeapWord beforeSize = heapTop - gen;
        // printf("from: \n");
        // printHeap(gen);
        // fflush(stdout);

        for (uint64_t i = 0; i < STACKMAP_SIZE / 8; i ++) {
            uint64_t map = ((uint64_t*)stackMap)[i];
            if (map != 0) {
                ((uint64_t*)stackMap)[i] = 0;
                for (uint8_t j = 0; j < 8; j ++) {
                    uint8_t byte = ((uint8_t*)&map)[j];
                    for (uint8_t k = 0; k < 8; k ++) if (byte & 1 << k) {
                        uint8_t** ref = (uint8_t**)(stack + sizeof(HeapWord) * (i * 64 + j * 8 + k));
                        copy(ref, &toTop);
                        **(HeapWord**)ref |= ~META_MASK;
                    }
                }
            }
        }

        uint8_t* obj = gen;
        while (obj < heapTop) {
            void* meta = *(void**)obj;
            // if (!meta) break;
            if ((HeapWord)meta & ~META_MASK) meta = (void*)((HeapWord)meta & META_MASK);
            else get_destructor(meta)(obj);
            auto size = get_size(meta);
            // printf("size = %lx\n", size);
            if (!size) break;
            obj += size;
        }

        memset(heapMap + (gen == heap ? 0 : GEN_SIZE / (8 * sizeof(HeapWord))), 0, GEN_SIZE / (8 * sizeof(HeapWord)));

        // printf("to: \n");
        // printHeap(to);

        gen = to;
        heapTop = toTop;
        uint64_t afterSize = heapTop - gen;
        
        // printf("collected %lx bytes\n", beforeSize - afterSize);
    }

    void alloc(void** ptr, HeapWord size) {
        if ((heapTop - gen) + size > GEN_SIZE) collect();
        claim(ptr);
        *ptr = heapTop;
        heapTop += size;
    }

    void claim(void* const* ptr) {
        // printf("ptr: %lx; stackmap: %lx; heapmap: %lx; stack: %lx; heap: %lx\n", ptr, stackMap, heapMap, stack, heap);
        // fflush(stdout);
        if ((uint8_t*)ptr >= stack) {
            HeapWord idx = ((uint8_t*)ptr - stack) / (8 * sizeof(HeapWord));
            stackMap[idx] |= 1 << ((uint8_t*)ptr - stack) / sizeof(HeapWord) % 8;   
        }
        else {
            HeapWord idx = ((uint8_t*)ptr - heap) / (8 * sizeof(HeapWord));
            heapMap[idx] |= 1 << ((uint8_t*)ptr - heap) / sizeof(HeapWord) % 8;
        }
    }

    void unclaim(void* const* ptr) {
        // printf("ptr: %lx; stackmap: %lx; heapmap: %lx; stack: %lx; heap: %lx\n", ptr, stackMap, heapMap, stack, heap);
        // fflush(stdout);
        if ((uint8_t*)ptr >= stack) {
            HeapWord idx = ((uint8_t*)ptr - stack) / (8 * sizeof(HeapWord));
            stackMap[idx] &= ~(1 << ((uint8_t*)ptr - stack) / sizeof(HeapWord) % 8);   
        }
        else {
            HeapWord idx = ((uint8_t*)ptr - heap) / (8 * sizeof(HeapWord));
            heapMap[idx] &= ~(1 << ((uint8_t*)ptr - heap) / sizeof(HeapWord) % 8);
        }
    }
}

extern int __ELUGC__main(int argc, char** argv);

int main(int argc, char** argv) {
    void* ptr = nullptr;
    stack = (uint8_t*)&ptr - STACK_SIZE;
    return __ELUGC__main(argc, argv);
}