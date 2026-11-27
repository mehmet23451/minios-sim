#define _POSIX_C_SOURCE 200809L
#include "sched.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *trim_ws(char *s) {
    while (*s && isspace((unsigned char)*s)) {
        s++;
    }
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[--len] = '\0';
    }
    return s;
}

bool sched_parse_policy(const char *name, sched_policy_t *out_policy) {
    if (!name || !out_policy) return false;
    if (strcmp(name, "fcfs") == 0) {
        *out_policy = POLICY_FCFS;
        return true;
    }
    if (strcmp(name, "sjf") == 0) {
        *out_policy = POLICY_SJF;
        return true;
    }
    if (strcmp(name, "srtf") == 0) {
        *out_policy = POLICY_SRTF;
        return true;
    }
    if (strcmp(name, "prio") == 0 || strcmp(name, "priority") == 0) {
        *out_policy = POLICY_PRIO;
        return true;
    }
    if (strcmp(name, "rr") == 0) {
        *out_policy = POLICY_RR;
        return true;
    }
    return false;
}

const char *sched_policy_name(sched_policy_t policy) {
    switch (policy) {
        case POLICY_FCFS: return "fcfs";
        case POLICY_SJF:  return "sjf";
        case POLICY_SRTF: return "srtf";
        case POLICY_PRIO: return "prio";
        case POLICY_RR:   return "rr";
        default:          return "unknown";
    }
}

bool sched_load_workload(const char *path, pcb_t *out_procs, int *out_count) {
    if (!path || !out_procs || !out_count) return false;
    FILE *fp = fopen(path, "r");
    if (!fp) return false;

    char line[4096];
    int count = 0;

    while (fgets(line, sizeof(line), fp) != NULL) {
        char *comment = strchr(line, '#');
        if (comment) *comment = '\0';
        char *p = trim_ws(line);
        if (*p == '\0') continue;

        if (count >= MAX_PROCS) {
            fclose(fp);
            return false;
        }

        pcb_t pcb;
        memset(&pcb, 0, sizeof(pcb));
        pcb.first_dispatch = -1;

        char *tok = strtok(p, " \t\r\n");
        if (!tok || strlen(tok) >= MAX_PID_LEN) {
            fclose(fp);
            return false;
        }
        snprintf(pcb.pid, sizeof(pcb.pid), "%s", tok);

        tok = strtok(NULL, " \t\r\n");
        if (!tok) { fclose(fp); return false; }
        char *endp = NULL;
        long arr = strtol(tok, &endp, 10);
        if (*endp != '\0' || arr < 0) { fclose(fp); return false; }
        pcb.arrival = (int)arr;

        tok = strtok(NULL, " \t\r\n");
        if (!tok) { fclose(fp); return false; }
        long prio = strtol(tok, &endp, 10);
        if (*endp != '\0') { fclose(fp); return false; }
        pcb.base_prio = (int)prio;
        pcb.curr_prio = (int)prio;

        int bcount = 0;
        while ((tok = strtok(NULL, " \t\r\n")) != NULL) {
            if (bcount >= MAX_BURSTS) { fclose(fp); return false; }
            long bval = strtol(tok, &endp, 10);
            if (*endp != '\0' || bval <= 0) { fclose(fp); return false; }
            pcb.bursts[bcount++] = (int)bval;
        }

        /* Must start and end with CPU burst -> odd count >= 1 */
        if (bcount == 0 || (bcount % 2) == 0) {
            fclose(fp);
            return false;
        }

        pcb.num_bursts = bcount;
        pcb.burst_idx = 0;
        pcb.rem_burst = pcb.bursts[0];
        pcb.state = STATE_NEW;
        pcb.order_seq = count;

        out_procs[count++] = pcb;
    }

    fclose(fp);
    if (count == 0) return false;
    *out_count = count;
    return true;
}

static void record_gantt(sched_result_t *res, const char *label, int start, int end) {
    if (end <= start) return;
    if (res->num_segments > 0) {
        gantt_seg_t *prev = &res->gantt[res->num_segments - 1];
        if (strcmp(prev->label, label) == 0 && prev->end == start) {
            prev->end = end;
            return;
        }
    }
    if (res->num_segments < MAX_SEGMENTS) {
        gantt_seg_t *seg = &res->gantt[res->num_segments++];
        snprintf(seg->label, sizeof(seg->label), "%s", label);
        seg->start = start;
        seg->end = end;
    }
}

static void move_to_ready(pcb_t *p, int proc_idx, int tick, int *ready_q, int *rq_len, bool trace) {
    if (p->state == STATE_READY) return;
    proc_state_t old = p->state;
    p->state = STATE_READY;
    p->ready_since = tick;
    p->quantum_used = 0;
    ready_q[(*rq_len)++] = proc_idx;
    if (trace) {
        const char *old_s = (old == STATE_NEW) ? "NEW" :
                            (old == STATE_WAITING) ? "WAITING" :
                            (old == STATE_RUNNING) ? "RUNNING" : "OTHER";
        printf("[t=%d] %s: %s -> READY\n", tick, p->pid, old_s);
    }
}

static void pop_ready_front(int *ready_q, int *rq_len) {
    if (*rq_len <= 0) return;
    for (int i = 0; i + 1 < *rq_len; i++) {
        ready_q[i] = ready_q[i + 1];
    }
    (*rq_len)--;
}

static int cmp_pid_asc(const void *a, const void *b) {
    const pcb_t *pa = (const pcb_t *)a;
    const pcb_t *pb = (const pcb_t *)b;
    return strcmp(pa->pid, pb->pid);
}

bool sched_run(const pcb_t *init_procs, int num_procs, const sched_opts_t *opts, sched_result_t *out_res) {
    if (!init_procs || num_procs <= 0 || !opts || !out_res) return false;
    if (opts->policy != POLICY_FCFS) return false;

    memset(out_res, 0, sizeof(*out_res));
    out_res->policy = POLICY_FCFS;
    out_res->num_procs = num_procs;

    pcb_t p[MAX_PROCS];
    for (int i = 0; i < num_procs; i++) {
        p[i] = init_procs[i];
        p[i].state = STATE_NEW;
        p[i].burst_idx = 0;
        p[i].rem_burst = p[i].bursts[0];
        p[i].wait_accum = 0;
        p[i].first_dispatch = -1;
        p[i].quantum_used = 0;
        p[i].curr_prio = p[i].base_prio;
    }

    qsort(p, (size_t)num_procs, sizeof(pcb_t), cmp_pid_asc);
    for (int i = 0; i < num_procs; i++) {
        p[i].order_seq = i;
    }

    int ready_q[MAX_PROCS];
    int rq_len = 0;
    int running = -1;
    int terminated = 0;
    int tick = 0;
    int seg_start = 0;
    char cur_label[MAX_PID_LEN] = "";
    bool has_cur_label = false;

    while (terminated < num_procs) {
        /* 1. Arrivals and completed I/O -> READY in PID order */
        for (int i = 0; i < num_procs; i++) {
            if (p[i].state == STATE_WAITING && p[i].rem_burst <= 0) {
                p[i].burst_idx++;
                p[i].rem_burst = p[i].bursts[p[i].burst_idx];
                move_to_ready(&p[i], i, tick, ready_q, &rq_len, opts->trace);
            }
        }
        for (int i = 0; i < num_procs; i++) {
            if (p[i].state == STATE_NEW && p[i].arrival <= tick) {
                move_to_ready(&p[i], i, tick, ready_q, &rq_len, opts->trace);
            }
        }

        /* 2. Handle running process completion (FCFS is non-preemptive) */
        if (running >= 0) {
            pcb_t *rp = &p[running];
            if (rp->rem_burst <= 0) {
                if (has_cur_label) {
                    record_gantt(out_res, cur_label, seg_start, tick);
                    has_cur_label = false;
                }
                if (rp->burst_idx + 1 >= rp->num_bursts) {
                    rp->state = STATE_TERMINATED;
                    rp->completion = tick;
                    rp->turnaround = rp->completion - rp->arrival;
                    rp->response = (rp->first_dispatch >= 0) ? (rp->first_dispatch - rp->arrival) : 0;
                    terminated++;
                    if (opts->trace) {
                        printf("[t=%d] %s: RUNNING -> TERMINATED\n", tick, rp->pid);
                    }
                } else {
                    rp->burst_idx++;
                    rp->rem_burst = rp->bursts[rp->burst_idx];
                    rp->state = STATE_WAITING;
                    if (opts->trace) {
                        printf("[t=%d] %s: RUNNING -> WAITING (io=%d)\n", tick, rp->pid, rp->rem_burst);
                    }
                }
                running = -1;
            }
        }

        if (terminated >= num_procs) break;

        /* 3. Dispatch next FCFS process if CPU is free */
        if (running < 0) {
            if (rq_len > 0) {
                if (has_cur_label && strcmp(cur_label, "IDLE") == 0) {
                    record_gantt(out_res, "IDLE", seg_start, tick);
                    has_cur_label = false;
                }
                int next = ready_q[0];
                pop_ready_front(ready_q, &rq_len);
                pcb_t *np = &p[next];
                if (np->first_dispatch < 0) {
                    np->first_dispatch = tick;
                }
                if (!has_cur_label || strcmp(cur_label, np->pid) != 0) {
                    if (has_cur_label) {
                        record_gantt(out_res, cur_label, seg_start, tick);
                    }
                    snprintf(cur_label, sizeof(cur_label), "%s", np->pid);
                    seg_start = tick;
                    has_cur_label = true;
                    if (tick > 0 || out_res->context_switches > 0) {
                        out_res->context_switches++;
                    }
                }
                np->state = STATE_RUNNING;
                running = next;
                if (opts->trace) {
                    printf("[t=%d] %s: READY -> RUNNING\n", tick, np->pid);
                }
            } else {
                if (!has_cur_label || strcmp(cur_label, "IDLE") != 0) {
                    if (has_cur_label) {
                        record_gantt(out_res, cur_label, seg_start, tick);
                    }
                    snprintf(cur_label, sizeof(cur_label), "IDLE");
                    seg_start = tick;
                    has_cur_label = true;
                }
            }
        }

        /* Advance one tick */
        for (int i = 0; i < num_procs; i++) {
            if (p[i].state == STATE_READY) {
                p[i].wait_accum++;
            } else if (p[i].state == STATE_WAITING) {
                p[i].rem_burst--;
            }
        }

        if (running >= 0) {
            p[running].rem_burst--;
            out_res->busy_ticks++;
        }

        tick++;
        if (tick > 1000000) return false;
    }

    out_res->total_ticks = tick;

    for (int i = 0; i < num_procs; i++) {
        p[i].waiting = p[i].wait_accum;
        out_res->procs[i] = p[i];
    }

    return true;
}

void sched_print_report(const sched_result_t *res, const sched_opts_t *opts) {
    printf("Policy: %s\n", sched_policy_name(res->policy));

    printf("Gantt: ");
    for (int i = 0; i < res->num_segments; i++) {
        printf("| %s %d-%d ", res->gantt[i].label, res->gantt[i].start, res->gantt[i].end);
    }
    printf("|\n");

    printf("PID\tArrival\tCompletion\tTurnaround\tWaiting\tResponse\n");
    double sum_tat = 0.0, sum_wt = 0.0, sum_rt = 0.0;
    for (int i = 0; i < res->num_procs; i++) {
        const pcb_t *p = &res->procs[i];
        printf("%s\t%d\t%d\t\t%d\t\t%d\t%d\n",
               p->pid, p->arrival, p->completion, p->turnaround, p->waiting, p->response);
        sum_tat += p->turnaround;
        sum_wt += p->waiting;
        sum_rt += p->response;
    }

    int n = res->num_procs;
    double util = (res->total_ticks > 0) ? (100.0 * res->busy_ticks / res->total_ticks) : 0.0;
    double thr  = (res->total_ticks > 0) ? (100.0 * n / res->total_ticks) : 0.0;

    printf("Avg Turnaround: %.2f | Avg Waiting: %.2f | Avg Response: %.2f\n",
           sum_tat / n, sum_wt / n, sum_rt / n);
    printf("CPU Utilization: %.2f%% | Throughput/100 ticks: %.2f | Context Switches: %d\n",
           util, thr, res->context_switches);
    (void)opts;
}

void sched_print_csv(const sched_result_t *res) {
    printf("pid,arrival,completion,turnaround,waiting,response\n");
    for (int i = 0; i < res->num_procs; i++) {
        const pcb_t *p = &res->procs[i];
        printf("%s,%d,%d,%d,%d,%d\n",
               p->pid, p->arrival, p->completion, p->turnaround, p->waiting, p->response);
    }
}

void sched_print_compare(const pcb_t *init_procs, int num_procs, const sched_opts_t *base_opts) {
    /* Added in a later commit once all five baseline policies are implemented */
    (void)init_procs;
    (void)num_procs;
    (void)base_opts;
}
