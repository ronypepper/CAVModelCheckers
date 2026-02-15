//
// Created by ronypepper on 13.02.26.
//

#pragma once
#include "BoundedModelChecker.h"

using namespace std;

class InterpolationBasedModelChecker : public BoundedModelChecker {
public:
    InterpolationBasedModelChecker(const std::string& filename);

    int detect_fixed_point(int k, bool check_initial_states, int k_max = -1, int timeout = -1, bool print_info = false);
private:
    static void reset_interp_clauses(vec<vec<vec<Lit> > >& interp_clauses);

    bool check_interpolant_convergence(vec<vec<vec<Lit> > > &interp_clauses, int new_interpolant, int num_tseitin_vars,
                                       int k) const;

    static bool check_timeout(const chrono::time_point<chrono::steady_clock>& start_time, int timeout, bool print_info);
};
