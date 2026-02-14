//
// Created by ronypepper on 13.02.26.
//

#include <cstdio>
#include "InterpolationBasedModelChecker.h"

using namespace std;

int main(int argc, char** argv)
{
    // Extract command line parameters
    if (argc != 4) {
        printf("ERROR: Exactly 3 command line parameters needed: unwinding depth k, timeout (sec) and target filename!");
        exit(1);
    }
    const int k = atoi(argv[1]);
    const int timeout = atoi(argv[2]);
    char* filename = argv[3];

    InterpolationBasedModelChecker imc(filename);
    int result = imc.detect_fixed_point(k, true, -1, timeout, true);
    if (result == 2)
        printf("TIMEOUT");
    else if (result == 0)
        printf("OK\n");
    else if (result == 1)
        printf("FAIL\n");
}