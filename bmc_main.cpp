//
// Created by ronypepper on 12.02.26.
//

#include <cstdio>
#include "BoundedModelChecker.h"

using namespace std;

int main(int argc, char** argv)
{
    // Extract command line parameters
    if (argc != 3) {
        printf("ERROR: Exactly 2 command line parameters needed: unwinding depth k and target filename!");
        exit(1);
    }
    const int k = atoi(argv[1]);
    char* filename = argv[2];

    BoundedModelChecker bmc(filename);
    int result = bmc.solve(k, true, false, nullptr, 0, false, nullptr, true);
    if (result == -1)
        printf("ERROR\n");
    else if (result == 0)
        printf("OK\n");
    else if (result == 1)
        printf("FAIL\n");
}