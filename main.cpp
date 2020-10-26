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