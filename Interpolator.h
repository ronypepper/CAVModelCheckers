//
// Created by ronypepper on 13.02.26.
//

#pragma once
#include "Proof.h"

struct Interpolator : ProofTraverser {
    int num_vars_in_one_state; // Number of variables in one state of the model (fixed)
    int num_tseitin_vars; // Number of currently introduced tseitin variables
    int next_tseitin_var; // Next free variable

    // Interpolants corresponding to model/resolved clauses.
    // Either 0 (false), 1 (true), a 0th state model variable or a tseitin variable
    vec<int> interpolants;

    // Clauses encoding sub-interpolations as equivalences to tseitin variables
    vec<vec<Lit> > interp_clauses;

    // Root and resolved clauses of the model
    vec<vec<Lit> > model_clauses;

    Interpolator(int num_vars_in_one_state, int next_tseitin_var) : num_vars_in_one_state(num_vars_in_one_state),
                                                                    num_tseitin_vars(0),
                                                                    next_tseitin_var(next_tseitin_var) {}

    void root(const vec<Lit>& c);

    void chain(const vec<ClauseId>& cs, const vec<Var>& xs);

private:
    static void resolve(vec<Lit>& main, vec<Lit>& other, Var x);

    int encode_or_interpolant(int I1, int I2);

    int encode_and_interpolant(int I1, int I2);

    int encode_pivot_interpolant(int x, int I_pos, int I_neg);
};
