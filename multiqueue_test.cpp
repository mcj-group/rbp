#include <cassert>
#include <iostream>
#include "multiqueue.h"

int main(int argc, const char** argv) {
    MultiQueue testCase(4, 3);
    testCase.insert(1, 0.0);
    testCase.insert(2, 1.0);
    testCase.insert(3, 2.0);
    std::cout<<testCase.check()<<"\n";
    testCase.changePriority(1, 4.0);
    std::cout<<testCase.peekPriority()<<"\n";
    std::cout<<testCase.extractMax()<<"\n";
    return 0;
}
