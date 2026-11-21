# MiniOS-Sim

CEN 302 Operating Systems Term Project.

## Student & Variant Information
- **Student Number:** 230704002
- **Variant (v = 230704002 mod 36):** 30
  - **Extra Scheduler:** HRRN
  - **Extra Synchronization Problem:** Sleeping Barber
  - **Extra Allocator:** Buddy
  - **Extra Page Replacement:** LFU

## Build
    make

## Run
    ./minios sched -a fcfs tests/sched-io.txt
