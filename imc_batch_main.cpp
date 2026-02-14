//
// Created by ronypepper on 14.02.26.
//

#include <cstdio>
#include <filesystem>
#include <fstream>

#include "InterpolationBasedModelChecker.h"

using namespace std;
namespace fs = std::filesystem;

int main(int argc, char** argv)
{
    // Extract command line parameters
    if (argc != 5) {
        printf("ERROR: Exactly 4 command line parameters needed: unwinding depth k, timeout (sec), "
               "maximum variables in one state, and target directory!");
        exit(1);
    }
    const int k = atoi(argv[1]);
    const int timeout = atoi(argv[2]);
    const int max_vars_in_one_state = atoi(argv[3]);
    char* directory = argv[4];

    // Collect all .aag files in directory
    if (!fs::is_directory(directory)) {
        printf("ERROR: Given directory is not a directory!");
        exit(1);
    }
    vector<string> filenames;
    for (auto const& dir_entry : fs::recursive_directory_iterator(directory))
        if (dir_entry.path().extension().string() == ".aag")
            filenames.push_back(dir_entry.path().string());
    if (filenames.empty()) {
        printf("ERROR: Given directory contains no AIGer (.aag) files!");
        exit(1);
    }

    // Extract expected results, indicated by SATISFIABLE or UNSATISFIABLE in last line of file
    printf("Extracting expected results from last line of %lu files.\n", filenames.size());
    vector<string> valid_filenames;
    vector<int> expected_results;
    for (auto const& filename : filenames) {
        ifstream file(filename);
        if (file.is_open()) {
            // Get the last line of the file
            file.seekg(-1, std::ios_base::end); // Moves to last \n of file
            while (file.tellg() > 0) { // Loop until second-to-last \n is found
                file.seekg(-1, std::ios_base::cur);
                if (file.peek() == '\n') { // Peek does not remove a character
                    file.get(); // Get to move to the first character of the next line
                    break;
                }
            }
            std::string last_line;
            getline(file, last_line);

            // Check for SATISFIABLE or UNSATISFIABLE in last line (discard file if not present)
            if (last_line.find("UNSATISFIABLE") != string::npos) {
                valid_filenames.push_back(filename);
                expected_results.push_back(0);
            }
            else if (last_line.find("SATISFIABLE") != string::npos) {
                valid_filenames.push_back(filename);
                expected_results.push_back(1);
            }
        }
        file.close();
    }
    size_t num_total = valid_filenames.size();
    printf("Extraction successful for %lu/%lu files.\n", num_total, filenames.size());

    // Perform interpolated model checking on all valid files with timeout
    printf("Performing interpolated model checking on %lu files.\n", num_total);
    int num_correct = 0, num_wrong = 0, num_timeout = 0, num_too_many_vars = 0, i = 0;
    for (auto const& filename : valid_filenames) {
        InterpolationBasedModelChecker imc(filename);
        if (max_vars_in_one_state > 0 && imc.getNumVarsInOneState() > max_vars_in_one_state) {
            num_too_many_vars++;
            printf("TOO MANY VARS");
        }
        else {
            int result = imc.detect_fixed_point(k, true, -1, timeout, false);
            if (result == 2) {
                num_timeout++;
                printf("TIMEOUT      ");
            }
            else if (result == expected_results[i]) {
                num_correct++;
                printf("CORRECT      ");
            }
            else {
                num_wrong++;
                printf("WRONG        ");
            }
        }
        i++;
        printf(" - %d/%lu - %s\n", i, num_total, filename.c_str());
    }

    printf("\n"
           "------ Result ------\n"
           "CORRECT      : %d\n"
           "WRONG        : %d\n"
           "TIMEOUT      : %d\n"
           "TOO MANY VARS: %d\n"
           "------  Done  ------\n", num_correct, num_wrong, num_timeout, num_too_many_vars);
}