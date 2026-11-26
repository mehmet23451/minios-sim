#ifndef MINIOS_SCHED_H
#define MINIOS_SCHED_H

#include <stdbool.h>
#include <stddef.h>

#define MAX_PROCS    128
#define MAX_BURSTS   256
#define MAX_SEGMENTS 2048
#define MAX_PID_LEN  32

typedef enum {
    STATE_NEW = 0,
    STATE_READY,
    STATE_RUNNING,
    STATE_WAITING,
    STATE_TERMINATED
} proc_state_t;

typedef enum {
    POLICY_FCFS = 0,
    POLICY_SJF,
    POLICY_SRTF,
    POLICY_PRIO,
    POLICY_RR
} sched_policy_t;

typedef struct {
    char pid[MAX_PID_LEN];
    int arrival;
    int base_prio;
    int curr_prio;

    int bursts[MAX_BURSTS];
    int num_bursts;
    int burst_idx;
    int rem_burst;

    proc_state_t state;
    int ready_since;
    int wait_accum;
    int first_dispatch;
    int completion;
    int turnaround;
    int waiting;
    int response;
    int quantum_used;
    int order_seq;
} pcb_t;

typedef struct {
    char label[MAX_PID_LEN];
    int start;
    int end;
} gantt_seg_t;

typedef struct {
    sched_policy_t policy;
    int quantum;
    int cs_cost;
    bool trace;
    bool csv;
    bool compare;
} sched_opts_t;

typedef struct {
    sched_policy_t policy;
    pcb_t procs[MAX_PROCS];
    int num_procs;

    gantt_seg_t gantt[MAX_SEGMENTS];
    int num_segments;

    int total_ticks;
    int busy_ticks;
    int context_switches;
} sched_result_t;

bool sched_parse_policy(const char *name, sched_policy_t *out_policy);
const char *sched_policy_name(sched_policy_t policy);
bool sched_load_workload(const char *path, pcb_t *out_procs, int *out_count);
bool sched_run(const pcb_t *init_procs, int num_procs, const sched_opts_t *opts, sched_result_t *out_res);
void sched_print_report(const sched_result_t *res, const sched_opts_t *opts);
void sched_print_csv(const sched_result_t *res);
void sched_print_compare(const pcb_t *init_procs, int num_procs, const sched_opts_t *base_opts);

#endif
