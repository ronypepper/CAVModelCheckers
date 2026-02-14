//
// Created by ronypepper on 12.02.26.
//

#include "BoundedModelChecker.h"
#include "minisat/Solver.h"
#include <fstream>
#include <sstream>
#include <string>

using namespace std;

BoundedModelChecker::BoundedModelChecker(const std::string& filename) {
    // Construct initial states, transitions and property from AIGer file
    ifstream file(filename);
    if (file.is_open()) {
        string line, word;
        stringstream line_stream;

        // Read AIGer header
        getline(file, line);
        line_stream = stringstream(line);
        getline(line_stream, word, ' '); // Skip aag
        getline(line_stream, word, ' '); // Get maximum variable index
        num_vars_in_one_state = stoi(word);
        getline(line_stream, word, ' '); // Get number of input variables
        int num_inputs = stoi(word);
        getline(line_stream, word, ' '); // Get number of state variables (latches)
        int num_latches = stoi(word);
        getline(line_stream, word, ' '); // Get number of output variables (must be one)
        assert(stoi(word) == 1);
        getline(line_stream, word, ' '); // Get number of AND gates
        int num_and_gates = stoi(word);

        // Skip input variables
        for (int i = 0; i < num_inputs; i++)
            getline(file, line);

        // Process latches
        vec<int> latch_vars;
        for (int i = 0; i < num_latches; i++) {
            getline(file, line);
            line_stream = stringstream(line);
            getline(line_stream, word, ' '); // Latch to be written
            int latch = stoi(word);
            latch_vars.push(latch);
            getline(line_stream, word, ' '); // Next state of latch
            int next_state = stoi(word);

            // Encode latch update as CNF and add to transitions
            int latch_next_step = latch + 2 * num_vars_in_one_state;
            latch_transitions.push();
            vec<int>& cl_1 = latch_transitions.last();
            if (next_state == 0) // next state always false; one clause in CNF
                cl_1.push(latch_next_step ^ 1);
            else if (next_state == 1) // next state always true; one clause in CNF
                cl_1.push(latch_next_step);
            else { // (latch of next step <-> next_state; two clauses in CNF)
                cl_1.push(latch_next_step ^ 1);
                cl_1.push(next_state);
                latch_transitions.push();
                vec<int>& cl_2 = latch_transitions.last();
                cl_2.push(latch_next_step);
                cl_2.push(next_state ^ 1);
            }
        }

        // Initial state is all latches set to false.
        // Encode (x <-> ~l1 AND ~l2 AND ...) to obtain single variable x resembling initial state.
        // x (INIT_STATE_VAR) is chosen as variable index 0, since it is used by MINISAT but not by AIGer
        if (latch_vars.size() > 0) {
            // Encode (x => ~l1 AND ~l2 AND ...) as ((~x OR ~l1) AND (~x OR ~l2) AND ...)
            for (int i = 0; i < latch_vars.size(); i++) {
                initial_state_encoding.push();
                vec<Lit>& cl = initial_state_encoding.last();
                cl.push(toLit(1));
                cl.push(toLit(latch_vars[i] ^ 1));
            }
            // Encode (x <= ~l1 AND ~l2 AND ...) as (x OR l1 OR l2 OR ...)
            initial_state_encoding.push();
            vec<Lit>& cl_1 = initial_state_encoding.last();
            cl_1.push(toLit(INIT_STATE_VAR));
            for (int i = 0; i < latch_vars.size(); i++)
                cl_1.push(toLit(latch_vars[i]));
        }

        // Store output as (already negated) property (output is “bad state” detector)
        getline(file, line);
        property = stoi(line);
        assert(property > 1); // property of true or false is invalid

        // Process AND gates
        for (int i = 0; i < num_and_gates; i++) {
            getline(file, line);
            line_stream = stringstream(line);
            getline(line_stream, word, ' '); // Gate output variable
            int gate_out = stoi(word);
            getline(line_stream, word, ' '); // Gate input 1 variable
            int gate_in1 = stoi(word);
            getline(line_stream, word, ' '); // Gate input 2 variable
            int gate_in2 = stoi(word);

            // Encode AND gate as CNF and add to transitions
            and_gate_clauses.push();
            vec<int>& cl_1 = and_gate_clauses.last();
            if (gate_in1 == 0 || gate_in2 == 0) {
                cl_1.push(gate_out ^ 1);
            }
            else if (gate_in1 == 1 && gate_in2 == 1) {
                cl_1.push(gate_out);
            }
            else if (gate_in1 == 1) { // (gate_out <-> gate_in2; two clauses in CNF)
                cl_1.push(gate_out ^ 1);
                cl_1.push(gate_in2);
                and_gate_clauses.push();
                vec<int>& cl_2 = and_gate_clauses.last();
                cl_2.push(gate_out);
                cl_2.push(gate_in2 ^ 1);
            }
            else if (gate_in2 == 1) { // (gate_out <-> gate_in1; two clauses in CNF)
                cl_1.push(gate_out ^ 1);
                cl_1.push(gate_in1);
                and_gate_clauses.push();
                vec<int>& cl_2 = and_gate_clauses.last();
                cl_2.push(gate_out);
                cl_2.push(gate_in1 ^ 1);
            }
            else { // (gate_out <-> (gate_in1 & gate_in2); three clauses in CNF)
                cl_1.push(gate_out ^ 1);
                cl_1.push(gate_in1);
                and_gate_clauses.push();
                vec<int>& cl_2 = and_gate_clauses.last();
                cl_2.push(gate_out ^ 1);
                cl_2.push(gate_in2);
                and_gate_clauses.push();
                vec<int>& cl_3 = and_gate_clauses.last();
                cl_3.push(gate_out);
                cl_3.push(gate_in1 ^ 1);
                cl_3.push(gate_in2 ^ 1);
            }
        }

        file.close();
    }
    else {
        printf("ERROR: File could not be opened!");
        exit(1);
    }
}

bool BoundedModelChecker::solve(int k, bool check_properties, bool skip_initial_property,
                                vec<vec<vec<Lit> > > *extra_clauses, int num_extra_vars, bool override_initial_state,
                                Interpolator *interpolator, bool print_info) {
    if (k < 0) // Clip minimum k
        k = 0;

    if (print_info)
        printf("Starting Bounded Model Checking with k = %d\n", k);

    Solver solver;
    if (interpolator) {
        solver.proof = new Proof(*interpolator);
    }

    // Introduce variables for k + 1 states plus INIT_STATE_VAR plus num_extra_vars
    for (int i = 0; i <= num_vars_in_one_state * (k + 1) + num_extra_vars; i++) {
        solver.newVar();
    }

    // Add initial state clauses
    for (int i = 0; i < initial_state_encoding.size(); i++) {
        solver.addClause(initial_state_encoding[i]);
        if (!solver.okay()) {
            if (print_info)
                printf("UNSAT while adding initial states\n");
            return false; // UNSAT
        }
    }

    // Enforce initial state
    if (!override_initial_state) {
        vec<Lit> init_cl;
        init_cl.push(toLit(INIT_STATE_VAR));
        solver.addClause(init_cl);
        if (!solver.okay()) {
            if (print_info)
                printf("UNSAT while enforcing initial states\n");
            return false; // UNSAT
        }
    }

    // Add AND-gates for 0th to kth state
    int num_clauses = and_gate_clauses.size();
    int unwind_offset = 0;
    for (int i = 0; i <= k; i++) {
        for (int j = 0; j < num_clauses; j++) {
            // Adjust literals to reflect current unwinding step
            vec<Lit> cl;
            const vec<int>& cl_base = and_gate_clauses[j];
            int cl_size = cl_base.size();
            for (int l = 0; l < cl_size; l++) {
                cl.push(toLit(cl_base[l] + unwind_offset));
            }

            solver.addClause(cl);
            if (!solver.okay()) {
                if (print_info)
                    printf("UNSAT while adding AND gate clauses\n");
                return false; // UNSAT
            }
        }

        // Increase offset to reflect next unwinding step
        unwind_offset += 2 * num_vars_in_one_state;
    }

    // Add transition function k times
    num_clauses = latch_transitions.size();
    unwind_offset = 0;
    for (int i = 0; i < k; i++) {
        for (int j = 0; j < num_clauses; j++) {
            // Adjust literals to reflect current unwinding step
            vec<Lit> cl;
            const vec<int>& cl_base = latch_transitions[j];
            int cl_size = cl_base.size();
            for (int l = 0; l < cl_size; l++) {
                cl.push(toLit(cl_base[l] + unwind_offset));
            }

            solver.addClause(cl);
            if (!solver.okay()) {
                if (print_info)
                    printf("UNSAT while adding transition clauses\n");
                return false; // UNSAT
            }
        }

        // Increase offset to reflect next unwinding step
        unwind_offset += 2 * num_vars_in_one_state;
    }

    // Add properties
    if (check_properties) {
        vec<Lit> cl;
        for (int i = skip_initial_property ? 1 : 0; i <= k; i++) {
            cl.push(toLit(property + 2 * num_vars_in_one_state * i)); // property already negated
        }
        if (cl.size() > 0) {
            solver.addClause(cl);
            if (!solver.okay()) {
                if (print_info)
                    printf("UNSAT while adding properties\n");
                return false; // UNSAT
            }
        }
    }

    // Add extra clauses
    if (extra_clauses) {
        for (int i = 0; i < extra_clauses->size(); i++) {
            vec<vec<Lit> >& clauses = (*extra_clauses)[i];
            for (int j = 0; j < clauses.size(); j++) {
                solver.addClause(clauses[j]);
                if (!solver.okay()) {
                    if (print_info)
                        printf("UNSAT while adding extra clauses\n");
                    return false; // UNSAT
                }
            }
        }
    }

    if (print_info)
        printf("Solving over %d variables and %d clauses\n", solver.nVars(), solver.nClauses());
    solver.solve();

    if (print_info && solver.okay())
        printf("SAT after solving\n");
    if (print_info && !solver.okay())
        printf("UNSAT after solving\n");

    return solver.okay(); // SAT or UNSAT
}