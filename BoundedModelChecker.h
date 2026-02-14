//
// Created by ronypepper on 12.02.26.
//

#pragma once
#include <string>
#include "Global.h"
#include "Interpolator.h"
#include "SolverTypes.h"

#define INIT_STATE_VAR 0

class BoundedModelChecker {
protected:
    int num_vars_in_one_state;
    vec<vec<Lit> > initial_state_encoding;
    vec<vec<int> > latch_transitions;
    vec<vec<int> > and_gate_clauses;
    int property;
public:
    BoundedModelChecker(const std::string& filename);

    int getNumVarsInOneState() const { return num_vars_in_one_state; }

    bool solve(int k, bool check_properties, bool skip_initial_property,
               vec<vec<vec<Lit> > >* extra_clauses = nullptr, int num_extra_vars = 0,
               bool override_initial_state = false, Interpolator* interpolator = nullptr, bool print_info = false);
};