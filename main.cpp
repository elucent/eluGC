#include "gc.h"
#include <vector>

using namespace std;

struct list {
    int val[100];
    object<list> next;
    list(object<list> next_in): next(next_in) {}
};

object<list> construct_list(int i) {
    object<list> l;
    while (i > 0) l = make<list>(l), i --;
    return l;
}

struct large {
    int data[1024];
};

int main(int argc, char** argv) {
    auto i = construct_list(100);
    for (int j = 1; j <= 100000; j ++) i = construct_list(100);
    while (i) printf("%d ", i->val), i = i->next;
    printf("\n");
    return i;
}