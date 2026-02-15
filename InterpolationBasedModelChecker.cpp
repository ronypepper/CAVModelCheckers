//
// Created by ronypepper on 13.02.26.
//

#include "InterpolationBasedModelChecker.h"
#include <chrono>
#include "Solver.h"

InterpolationBasedModelChecker::InterpolationBasedModelChecker(const std::string &filename): BoundedModelChecker(filename) {
    if (first_latch_var == -1)
        printf("ERROR: Latch variables are not continuous!\n");
}

int InterpolationBasedModelChecker::detect_fixed_point(int k, bool check_initial_states, int k_max, int timeout,
                                                       bool print_info) {
    if (first_latch_var == -1)
        return -1; // Latch variables are not continuous

    chrono::time_point<chrono::steady_clock> start_time = chrono::steady_clock::now(); // Save start time for timeout
    if (k < 2) // Clip minimum k
        k = 2;

    if (print_info)
        printf("Starting Interpolation-Based Model Checking with start k = %d\n", k);

    // Check initial states for property violation
    if (check_initial_states) {
        int result = solve(0, true, false);
        if (result == -1) {
            if (print_info)
                printf("Error in initial state check BMC!\n");
            return -1; // Error
        }
        if (result == 1) {
            if (print_info)
                printf("Initial states violate property\n");
            return 1; // SATISFIABLE
        }
    }

    // ------ Perform interpolation based model checking up to unwinding depth k ------

    int num_tseitin_vars = 0; // Number of currently introduced tseitin variables
    int next_tseitin_var = num_vars_in_one_state * (k + 1) + 1; // Next free variable
    vec<vec<vec<Lit> > > interp_clauses; // Clauses encoding sub-interpolations as equivalences to tseitin variables
    reset_interp_clauses(interp_clauses); // First vec<vec<Lit> > enforces the initial state plus interpolants
    while (k_max < 0 || k <= k_max) {
        if (print_info)
            printf("Starting BMC with k=%d, interpolation iteration %d\n", k, interp_clauses[0][0].size());

        // Timeout check
        if (check_timeout(start_time, timeout, print_info))
            return 2; // TIMEOUT

        // Perform BMC with interpolation
        Interpolator interpolator(*this, next_tseitin_var, k);
        int result = solve(k, true, true, &interp_clauses, num_tseitin_vars, true, &interpolator);
        if (result == -1) {
            if (print_info)
                printf("Error in BMC!\n");
            return -1; // Error
        }
        if (result == 1) {
            // Check if found counterexample was genuine
            if (interp_clauses.size() == 1) { // Only initial state clause and no interpolant clauses
                if (print_info)
                    printf("Genuine counterexample found - Done\n");
                return 1; // SATISFIABLE
            }
            if (print_info)
                printf("Possibly spurious counterexample found\n");

            // Reset interpolants and restart BMC with increased unwinding depth
            k++;
            num_tseitin_vars = 0;
            next_tseitin_var = num_vars_in_one_state * (k + 1) + 1;
            reset_interp_clauses(interp_clauses);
            continue;
        }

        // Add new interpolant clauses and tseitin variables
        interp_clauses.push();
        interpolator.interp_clauses.moveTo(interp_clauses.last());
        num_tseitin_vars += interpolator.num_tseitin_vars;
        next_tseitin_var += interpolator.num_tseitin_vars;

        // Interpolants can be 0 (false), 1 (true), a 0th state model literal or a tseitin literal that
        // is equivalent to a sub-interpolant encoded in the interp_clauses. The last interpolant is equivalent
        // to the entire computed interpolant and in this file referred to as the interpolant.
        int new_interpolant = interpolator.interpolants.last();

        // Check new interpolant for 0 (false) and 1 (true) edge cases
        if (new_interpolant == 0) {
            if (print_info)
                printf("New interpolant is false\n"); // TODO: Only possible for models that can't fulfill the initial state?
            return 0; // UNSATISFIABLE

            // Reset interpolants and restart BMC with increased unwinding depth
            k++;
            num_tseitin_vars = 0;
            next_tseitin_var = num_vars_in_one_state * (k + 1) + 1;
            reset_interp_clauses(interp_clauses);
            continue;
        }
        if (new_interpolant == 1) {
            if (print_info)
                printf("New interpolant is true\n"); // TODO: Only possible for models where no state can violate the property?
            return 0; // UNSATISFIABLE

            // Reset interpolants and restart BMC with increased unwinding depth
            k++;
            num_tseitin_vars = 0;
            next_tseitin_var = num_vars_in_one_state * (k + 1) + 1;
            reset_interp_clauses(interp_clauses);
            continue;
        }

        // Timeout check
        if (check_timeout(start_time, timeout, print_info))
            return 2; // TIMEOUT

        // Check if interpolants and initial states converged
        if (check_interpolant_convergence(interp_clauses, new_interpolant, num_tseitin_vars, k)) {
            if (print_info)
                printf("Interpolants and initial states converged - Done\n");
            return 0; // UNSATISFIABLE
        }

        // Add new interpolant to initial state plus previous interpolants
        interp_clauses[0][0].push(toLit(new_interpolant));
    }
    if (print_info)
        printf("Exceeded max unwinding depth k - Abort\n");
    return false;
}

void InterpolationBasedModelChecker::reset_interp_clauses(vec<vec<vec<Lit> > >& interp_clauses) {
    // First vec<vec<Lit> > enforces the initial state plus interpolants
    // Clear clauses and add INIT_STATE_VAR as new first vec<vec<Lit> >
    interp_clauses.clear();
    interp_clauses.push();
    interp_clauses[0].push();
    interp_clauses[0][0].push(toLit(INIT_STATE_VAR));
}

bool InterpolationBasedModelChecker::check_interpolant_convergence(vec<vec<vec<Lit> > > &interp_clauses,
                                                                   int new_interpolant, int num_tseitin_vars,
                                                                   int k) const {
    // If ~((Q_i OR I) => Q_i) is UNSAT, then interpolants have converged.
    // ~((Q_i OR I) => Q_i) equivalent to ((Q_i OR I) AND ~Q_i).
    // Q_i represents (Q OR I_1 OR I_2 OR ... I_n).
    // Q represents initial states, I_x represent previous interpolants, I is the new interpolant.

    Solver solver;

    // Introduce variables for k + 1 states plus INIT_STATE_VAR plus tseitin variables.
    // Only variables of the 0th state are used, but the tseitin variables are appended
    // after the last state, so we need to introduce all variables
    for (int i = 0; i <= num_vars_in_one_state * (k + 1) + num_tseitin_vars; i++) {
        solver.newVar();
    }

    // Add initial state clauses
    for (int i = 0; i < initial_state_encoding.size(); i++) {
        solver.addClause(initial_state_encoding[i]);
        if (!solver.okay()) {
            return true; // UNSAT - interpolants and initial states converged
        }
    }

    // Add interpolant clauses (skip first vec<vec<Lit> > as it contains (Q OR I_1 OR I_2 OR ... I_n))
    for (int i = 1; i < interp_clauses.size(); i++) {
        vec<vec<Lit> >& clauses = interp_clauses[i];
        for (int j = 0; j < clauses.size(); j++) {
            solver.addClause(clauses[j]);
            if (!solver.okay()) {
                return true; // UNSAT - interpolants and initial states converged
            }
        }
    }

    // Add left side of implication (Q_i OR I)
    vec<Lit> cl_1;
    interp_clauses[0][0].copyTo(cl_1);
    cl_1.push(toLit(new_interpolant));
    solver.addClause(cl_1);
    if (!solver.okay()) {
        return true; // UNSAT - interpolants and initial states converged
    }

    // Add right side of implication (~Q_i)
    for (int i = 0; i < interp_clauses[0][0].size(); i++) {
        vec<Lit> cl_2;
        cl_2.push(~interp_clauses[0][0][i]);
        solver.addClause(cl_2);
        if (!solver.okay()) {
            return true; // UNSAT - interpolants and initial states converged
        }
    }

    solver.solve();

    return !solver.okay(); // SAT or UNSAT - No convergence or convergence
}

bool InterpolationBasedModelChecker::check_timeout(const chrono::time_point<chrono::steady_clock> &start_time,
                                                   int timeout, bool print_info) {
    if (timeout > 0) {
        auto cur_time = chrono::steady_clock::now();
        auto elapsed_sec = chrono::duration_cast<chrono::seconds>(cur_time - start_time).count();
        if (elapsed_sec > timeout) {
            if (print_info)
                printf("Timeout exceeded - Done\n");
            return true;
        }
    }
    return false;
}