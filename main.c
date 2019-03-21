#define _GNU_SOURCE
#include <dirent.h>
#include <linux/sched.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define DEBUG 0

int configure_uid_map();

int main(int argc, const char** argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <root_directory>\n", argv[0]);
        return 1;
    }

    if (access(argv[1], F_OK) != 0) {
        if (mkdir(argv[1], 0755) != 0) {
            perror("mkdir");
            return 1;
        }
    }

    if (chdir(argv[1]) != 0) {
        perror("chdir");
        return 1;
    }

    int old_uid = getuid();
    int old_gid = getgid();
    if (DEBUG) {
        fprintf(stderr, "setting up user namespace ...\n");
    }
    if (unshare(CLONE_NEWUSER) != 0) {
        perror("unshare");
        return 1;
    }

    if (DEBUG) {
        fprintf(stderr, "configuring UID map ...\n");
    }
    if (configure_uid_map(old_uid, old_gid)) {
        return 1;
    }

    if (DEBUG) {
        fprintf(stderr, "creating other namespaces ...\n");
    }
    if (unshare(CLONE_NEWNS | CLONE_NEWPID) != 0) {
        perror("unshare");
        return 1;
    }

    pid_t pid = fork();
    if (pid != 0) {
        int status = 0;
        waitpid(pid, &status, 0);
        return status;
    }

    char working_dir[512];
    getcwd(working_dir, sizeof(working_dir));

    const char* mkdirs[] = {
        "bin",
        "lib",
        "lib64",
        "proc",
        "sbin",
        "usr",
        "usr/bin",
        "usr/lib",
        "usr/sbin",
    };
    for (int i = 0; i < sizeof(mkdirs) / sizeof(mkdirs[0]); ++i) {
        mkdir(mkdirs[i], 0755);
    }

    const char* paths[] = {
        "/bin",
        "/lib",
        "/lib64",
        "/sbin",
        "/usr/bin",
        "/usr/lib",
        "/usr/sbin",
    };
    for (int i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
        const char* path = paths[i];
        size_t path_size = strlen(path) + strlen(working_dir) + 1;
        char abs_path[512];
        snprintf(abs_path, sizeof(abs_path), "%s%s", working_dir, path);
        if (DEBUG) {
            fprintf(stderr, "bind mounting %s to %s ...\n", path, abs_path);
        }
        if (mount(path, abs_path, NULL, MS_BIND, NULL) != 0) {
            perror("mount");
            return 1;
        }
    }

    char proc_path[512];
    snprintf(proc_path, sizeof(proc_path), "%s/proc", working_dir);
    if (DEBUG) {
        fprintf(stderr, "mounting proc to %s ...\n", proc_path);
    }
    if (mount("proc", proc_path, "proc", 0, NULL) != 0) {
        perror("mount");
        return 1;
    }

    if (DEBUG) {
        fprintf(stderr, "changing root to %s ...\n", working_dir);
    }
    if (chroot(working_dir) != 0) {
        perror("chroot");
        return 1;
    }
    if (chdir("/") != 0) {
        perror("chdir");
        return 1;
    }

    if (DEBUG) {
        fprintf(stderr, "executing shell ...\n");
    }
    if (execl("/bin/bash", "/bin/bash", NULL) != 0) {
        perror("execl");
        return 1;
    }
}

int configure_uid_map(int old_uid, int old_gid) {
    FILE* fp = fopen("/proc/self/setgroups", "w");
    if (fp == NULL) {
        perror("fopen");
        return 1;
    }
    if (fwrite("deny\n", 1, 5, fp) != 5) {
        fclose(fp);
        perror("fwrite");
        return 1;
    }
    fclose(fp);

    for (int i = 0; i < 2; ++i) {
        if (i == 0) {
            fp = fopen("/proc/self/uid_map", "w");
        } else {
            fp = fopen("/proc/self/gid_map", "w");
        }
        if (fp == NULL) {
            perror("fopen");
            return 1;
        }
        char data[512];
        if (i == 0) {
            sprintf(data, "0 %d 1\n", old_uid);
        } else {
            sprintf(data, "0 %d 1\n", old_gid);
        }
        if (fwrite(data, 1, strlen(data), fp) != strlen(data)) {
            fclose(fp);
            return 1;
        }
        fclose(fp);
    }
    return 0;
}
