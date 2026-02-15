//
// Created by ronypepper on 13.02.26.
//

#include "Interpolator.h"

#include "InterpolationBasedModelChecker.h"
#include "Sort.h"

Interpolator::Interpolator(const InterpolationBasedModelChecker &imc, int next_tseitin_var, int k) : imc(imc),
    num_tseitin_vars(0), next_tseitin_var(next_tseitin_var) {
    partition_shared_start = imc.first_latch_var + imc.num_vars_in_one_state;
    partition_shared_end = imc.last_latch_var + imc.num_vars_in_one_state;
    partition_b_start = partition_shared_end + 1;
    partition_b_end = imc.num_vars_in_one_state * (k + 1);
}

void Interpolator::root(const vec<Lit> &c) {
    // Interpolants of root clauses are either True if they are part of the B
    // partition or False if they are part of the A partition.
    // A clause is part of the A partition if it contains a variable belonging
    // to the 0th state.

    // Determine interpolant of root clause
    int interpolant = 0; // False - belongs to A partition
    for (int i = 0; i < c.size(); i++) {
        if (var(c[i]) >= partition_b_start && var(c[i]) <= partition_b_end) {
            interpolant = 1; // True - belongs to B partition
            break;
        }
    }
    interpolants.push(interpolant);

    // Store clause (to be able to identify pivot variable polarity when interpolating)
    model_clauses.push();
    c.copyTo(model_clauses.last());
}

void Interpolator::chain(const vec<ClauseId> &cs, const vec<Var> &xs) {
    // If pivot is only part of the A partition (it is a variable of the
    // 0th state), OR together interpolants of resolvent clauses.
    // If pivot is only part of the B partition (it is a variable of the
    // 2nd to kth state), AND together interpolants of resolvent clauses.
    // If pivot is part of both A and B partition (it is a variable of the
    // 1st state), resolved interpolant is: (x OR I1) AND (~x OR I2).

    // Add new to-be resolved clause
    model_clauses.push();
    vec<Lit>& cl = model_clauses.last();
    model_clauses[cs[0]].copyTo(cl);

    // Interpolate and resolve over chain
    int new_interpolant = interpolants[cs[0]];
    for (int i = 0; i < xs.size(); i++) {
        // Interpolate
        if (xs[i] >= partition_shared_start && xs[i] <= partition_shared_end) { // pivot variable in both partitions
            // Determine which clause contains the positive pivot literal
            int pos_pivot_interpolant = new_interpolant;
            int neg_pivot_interpolant = interpolants[cs[i+1]];
            for (int j = 0; j < cl.size(); j++) {
                if (var(cl[j]) == xs[i]) {
                    if (sign(cl[j]) == 1) {
                        pos_pivot_interpolant = interpolants[cs[i+1]];
                        neg_pivot_interpolant = new_interpolant;
                        break;
                    }
                }
            }

            new_interpolant = encode_pivot_interpolant(xs[i], pos_pivot_interpolant, neg_pivot_interpolant);
        }
        else if (xs[i] >= partition_b_start && xs[i] <= partition_b_end) { // pivot variable only in B partition
            new_interpolant = encode_and_interpolant(new_interpolant, interpolants[cs[i+1]]);
        }
        else { // pivot variable only in A partition
            new_interpolant = encode_or_interpolant(new_interpolant, interpolants[cs[i+1]]);
        }

        // Resolve
        resolve(cl, model_clauses[cs[i+1]], xs[i]);
    }

    // Add interpolant (equivalent literal) of new resolved clause
    interpolants.push(new_interpolant);
}

void Interpolator::resolve(vec<Lit> &main, vec<Lit> &other, Var x) { // From minisat/Main.C
    Lit     p;
    bool    ok1 = false, ok2 = false;
    for (int i = 0; i < main.size(); i++){
        if (var(main[i]) == x){
            ok1 = true, p = main[i];
            main[i] = main.last();
            main.pop();
            break;
        }
    }

    for (int i = 0; i < other.size(); i++){
        if (var(other[i]) != x)
            main.push(other[i]);
        else{
            if (p != ~other[i])
                printf("PROOF ERROR! Resolved on variable with SAME polarity in both clauses: %d\n", x+1);
            ok2 = true;
        }
    }

    if (!ok1 || !ok2)
        printf("PROOF ERROR! Resolved on missing variable: %d\n", x+1);

    sortUnique(main);
}

int Interpolator::encode_or_interpolant(int I1, int I2) {
    if (I1 == 1 || I2 == 1) // I1 or I2 is true - Simplify to true
        return 1;
    if (I1 == 0) // I1 is false - Simplify to I2
        return I2;
    if (I2 == 0) // I2 is false - Simplify to I1
        return I1;

    // Add new variable and clauses to encode (new_var <-> I1 OR I2); three clauses in CNF
    int new_interpolant = next_tseitin_var * 2;
    next_tseitin_var++;
    num_tseitin_vars++;
    interp_clauses.push();
    vec<Lit>& cl_1 = interp_clauses.last();
    cl_1.push(toLit(new_interpolant ^ 1));
    cl_1.push(toLit(I1));
    cl_1.push(toLit(I2));
    interp_clauses.push();
    vec<Lit>& cl_2 = interp_clauses.last();
    cl_2.push(toLit(new_interpolant));
    cl_2.push(toLit(I1 ^ 1));
    interp_clauses.push();
    vec<Lit>& cl_3 = interp_clauses.last();
    cl_3.push(toLit(new_interpolant));
    cl_3.push(toLit(I2 ^ 1));

    return new_interpolant;
}

int Interpolator::encode_and_interpolant(int I1, int I2) {
    if (I1 == 0 || I2 == 0) // I1 or I2 is false - Simplify to false
        return 0;
    if (I1 == 1) // I1 is true - Simplify to I2
        return I2;
    if (I2 == 1) // I2 is true - Simplify to I1
        return I1;

    // Add new variable and clauses to encode (new_var <-> I1 AND I2); three clauses in CNF
    int new_interpolant = next_tseitin_var * 2;
    next_tseitin_var++;
    num_tseitin_vars++;
    interp_clauses.push();
    vec<Lit>& cl_1 = interp_clauses.last();
    cl_1.push(toLit(new_interpolant ^ 1));
    cl_1.push(toLit(I1));
    interp_clauses.push();
    vec<Lit>& cl_2 = interp_clauses.last();
    cl_2.push(toLit(new_interpolant ^ 1));
    cl_2.push(toLit(I2));
    interp_clauses.push();
    vec<Lit>& cl_3 = interp_clauses.last();
    cl_3.push(toLit(new_interpolant));
    cl_3.push(toLit(I1 ^ 1));
    cl_3.push(toLit(I2 ^ 1));

    return new_interpolant;
}

int Interpolator::encode_pivot_interpolant(Var x, int I_pos, int I_neg) {
    // Change time frame of pivot variable from state 1 to state 0
    x = (x - imc.num_vars_in_one_state) * 2; // Times 2 to make it a literal

    // Add interpolant for (I1 <-> x OR I_pos)
    int I1;
    if (I_pos == 0)
        I1 = x;
    else if (I_pos == 1)
        I1 = 1;
    else
        I1 = encode_or_interpolant(x, I_pos);

    // Add interpolant for (I2 <-> ~x OR I_neg)
    int I2;
    if (I_neg == 0)
        I2 = x ^ 1;
    else if (I_neg == 1)
        I2 = 1;
    else
        I2 = encode_or_interpolant(x ^ 1, I_neg);

    // Add interpolant for (I3 <-> I1 AND I2)
    return encode_and_interpolant(I1, I2);
}
