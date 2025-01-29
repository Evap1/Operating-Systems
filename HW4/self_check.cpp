#include "my_own_lib.h"
#include <iostream>

int main() {
    void* a = smalloc(((128*1024) / 2) - 32);
    if (a) {
        printHeapStatistics();
        sfree(a);
        printHeapStatistics();
    }
    return 0;
}