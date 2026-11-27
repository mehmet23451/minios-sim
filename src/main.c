#define _POSIX_C_SOURCE 200809L
#include "sched.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void die_err(const char *msg) {
    fprintf(stderr, "Error: %s\n", msg);
    exit(1);
}

static int cmd_sched(int argc, char **argv) {
    sched_opts_t opts;
    opts.policy = POLICY_FCFS;
    opts.quantum = 4;
    opts.cs_cost = 0;
    opts.trace = false;
    opts.csv = false;
    opts.compare = false;

    const char *input_path = NULL;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--algo") == 0 || strcmp(argv[i], "--policy") == 0) {
            if (i + 1 >= argc) die_err("missing policy name");
            if (!sched_parse_policy(argv[++i], &opts.policy)) {
                die_err("unknown scheduling policy");
            }
        } else if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quantum") == 0) {
            if (i + 1 >= argc) die_err("missing quantum value");
            char *endp = NULL;
            long q = strtol(argv[++i], &endp, 10);
            if (*endp != '\0' || q <= 0) die_err("invalid quantum value");
            opts.quantum = (int)q;
        } else if (strcmp(argv[i], "--cs") == 0) {
            if (i + 1 >= argc) die_err("missing context switch cost");
            char *endp = NULL;
            long cs = strtol(argv[++i], &endp, 10);
            if (*endp != '\0' || cs < 0) die_err("invalid context switch cost");
            opts.cs_cost = (int)cs;
        } else if (strcmp(argv[i], "--trace") == 0) {
            opts.trace = true;
        } else if (strcmp(argv[i], "--csv") == 0) {
            opts.csv = true;
        } else if (strcmp(argv[i], "--compare") == 0) {
            opts.compare = true;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("Usage: minios sched [-a fcfs] [--trace] [--csv] <file>\n");
            return 0;
        } else if (argv[i][0] == '-') {
            die_err("unknown option for sched");
        } else {
            if (input_path != NULL) die_err("multiple input files provided");
            input_path = argv[i];
        }
    }

    if (!input_path) die_err("missing workload input file");
    if (opts.compare) die_err("--compare not implemented yet");
    if (opts.policy != POLICY_FCFS) die_err("only fcfs is implemented in this step");

    pcb_t procs[MAX_PROCS];
    int num_procs = 0;
    if (!sched_load_workload(input_path, procs, &num_procs)) {
        die_err("failed to load or parse workload file");
    }

    sched_result_t res;
    if (!sched_run(procs, num_procs, &opts, &res)) {
        die_err("fcfs scheduler failed");
    }

    if (opts.csv) {
        sched_print_csv(&res);
    } else {
        sched_print_report(&res, &opts);
    }

    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) die_err("missing subcommand");
    if (strcmp(argv[1], "sched") == 0) return cmd_sched(argc, argv);
    die_err("subcommand not implemented yet");
    return 1;
}
