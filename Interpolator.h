//
// Created by ronypepper on 13.02.26.
//

#pragma once
#include "Proof.h"

class InterpolationBasedModelChecker;

struct Interpolator : ProofTraverser {
    const InterpolationBasedModelChecker& imc; // Model checker that uses this Interpolator

    // Ranges of pivot variable partitions
    int partition_shared_start;
    int partition_shared_end;
    int partition_b_start;
    int partition_b_end;

    int num_tseitin_vars; // Number of currently introduced tseitin variables
    int next_tseitin_var; // Next free variable

    // Interpolants corresponding to model/resolved clauses.
    // Either 0 (false), 1 (true), a 0th state model literal or a tseitin literal
    vec<int> interpolants;

    // Clauses encoding sub-interpolations as equivalences to tseitin variables
    vec<vec<Lit> > interp_clauses;

    // Root and resolved clauses of the model
    vec<vec<Lit> > model_clauses;

    Interpolator(const InterpolationBasedModelChecker& imc, int next_tseitin_var, int k);

    void root(const vec<Lit>& c);

    void chain(const vec<ClauseId>& cs, const vec<Var>& xs);

private:
    static void resolve(vec<Lit>& main, vec<Lit>& other, Var x);

    int encode_or_interpolant(int I1, int I2);

    int encode_and_interpolant(int I1, int I2);

    int encode_pivot_interpolant(Var x, int I_pos, int I_neg);
};
