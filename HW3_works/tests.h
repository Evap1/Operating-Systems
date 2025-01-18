#ifndef TESTS_H
#define TESTS_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>


/*
    To run, compile the relavent files like so:
    gcc -o test_queue tests.c queue.c -Wall
    execute:
    ./test_queue > output_queue.txt
    diff marrge: (If the files are identical, there will be no output)
    diff -u expected_queue.txt output_queue.txt


*/
//-------------------------------------------
void runQueueTests();


#endif // TESTS_H