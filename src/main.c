#include <stdio.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <command> [options] <input_file>\n", argv[0]);
        return 1;
    }
    printf("MiniOS-Sim initialized. Command: %s\n", argv[1]);
    return 0;
}
