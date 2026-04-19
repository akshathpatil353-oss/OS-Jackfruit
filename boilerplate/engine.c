#define _GNU_SOURCE
#include <sched.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STACK_SIZE (1024 * 1024)
#define META_FILE "/tmp/container_meta.txt"

static char child_stack[STACK_SIZE];

/* ---------------- CONTAINER FUNCTION ---------------- */
int container_main(void *arg) {
    char **args = (char **)arg;
    char *cmd = args[0];
    char *rootfs = args[1];

    // isolate filesystem
    if (chroot(rootfs) != 0) {
        perror("chroot");
        exit(1);
    }

    chdir("/");

    // mount proc
    if (mount("proc", "/proc", "proc", 0, NULL) != 0) {
        perror("mount proc");
        exit(1);
    }

    // execute command
    execl(cmd, cmd, NULL);

    perror("exec failed");
    return 1;
}

/* ---------------- MAIN ---------------- */
int main(int argc, char *argv[]) {

    /* -------- RUN (foreground) -------- */
    if (argc >= 5 && strcmp(argv[1], "run") == 0) {
        char *id = argv[2];
        char *rootfs = argv[3];
        char *cmd = argv[4];

        char *args[2] = {cmd, rootfs};

        int flags = CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWNS | SIGCHLD;

        pid_t pid = clone(container_main, child_stack + STACK_SIZE, flags, args);

        if (pid < 0) {
            perror("clone");
            exit(1);
        }

        waitpid(pid, NULL, 0);
        printf("Container %s exited\n", id);
        return 0;
    }

    /* -------- START (background) -------- */
    if (argc >= 5 && strcmp(argv[1], "start") == 0) {
        char *id = argv[2];
        char *rootfs = argv[3];
        char *cmd = argv[4];

        char *args[2] = {cmd, rootfs};

        int flags = CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWNS | SIGCHLD;

        pid_t pid = clone(container_main, child_stack + STACK_SIZE, flags, args);

        if (pid < 0) {
            perror("clone");
            exit(1);
        }

        // store metadata in file
        FILE *f = fopen(META_FILE, "a");
        if (f) {
            fprintf(f, "%s %d RUNNING\n", id, pid);
            fclose(f);
        }

        printf("Container %s started (PID %d)\n", id, pid);
        return 0;
    }

    /* -------- PS -------- */
    if (argc == 2 && strcmp(argv[1], "ps") == 0) {
        FILE *f = fopen(META_FILE, "r");

        if (!f) {
            printf("No containers\n");
            return 0;
        }

        char id[32], state[32];
        int pid;

        printf("ID\tPID\tSTATE\n");

        while (fscanf(f, "%s %d %s", id, &pid, state) != EOF) {
            printf("%s\t%d\t%s\n", id, pid, state);
        }

        fclose(f);
        return 0;
    }

    /* -------- USAGE -------- */
    printf("Usage:\n");
    printf("  ./engine run <id> <rootfs> <command>\n");
    printf("  ./engine start <id> <rootfs> <command>\n");
    printf("  ./engine ps\n");

    return 1;
}
