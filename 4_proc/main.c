#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <stdint-gcc.h>
#include <fcntl.h>
#include <stdlib.h>

#define BUF_SIZE 512

#define PID "13661"

void write_from_file_to_file(FILE *from, FILE *to) {
    int len, i;
    char buf[BUF_SIZE];
    while ((len = fread(buf, 1, BUF_SIZE, from)) > 0) {
        for (i = 0; i < len; i++)
            if (buf[i] == 0)
                buf[i] = 10;
        buf[len] = 0;
        fprintf(to, "%s", buf);
    }
}

void print_page(uint64_t address, uint64_t data, FILE *out) {
    fprintf(out, "0x%-16lx : %-16lx %-10ld %-10ld %-10ld %-10ld\n", address,
            data & (((uint64_t) 1 << 55) -1), (data >> 55) &1,
            (data >> 61) &1, (data >> 62) &1, (data >> 63) &1);
}

void get_pagemap_info(const char *proc, FILE *out) {
    fprintf(out, "\n---------------------\nPAGEMAP\n---------------------\n");

    fprintf(out, " addr : pfn soft-dirty file/shared swapped present\n");

    char path[PATH_MAX];
    snprintf(path, PATH_MAX, "/proc/%s/maps", proc);
    FILE *maps = fopen(path, "r");

    snprintf(path, PATH_MAX, "/proc/%s/pagemap", proc);
    int pm_fd = open(path, O_RDONLY);

    char buf[BUF_SIZE + 1] = "\0";
    int len;

    // чтение maps
    while ((len = fread(buf, 1, BUF_SIZE, maps)) > 0) {
        for (int i = 0; i < len; ++i)
            if (buf[i] == 0) buf[i] = '\n';
        buf[len] = '\0';

        // проход по строкам из maps
        // используем strtok_r вместо strtok для возможности разбиения на лексемы разных буферов
        char *save_row;
        char *row = strtok_r(buf, "\n", &save_row);

        while (row) {
            // получение столбца участка адресного пространства
            char *addresses = strtok(row, " ");
            char *start_str, *end_str;

            // получение начала и конца участка адресного пространства
            if ((start_str = strtok(addresses, "-")) && (end_str = strtok(NULL, "-"))) {
                uint64_t start = strtoul(start_str, NULL, 16);
                uint64_t end = strtoul(end_str, NULL, 16);

                for (uint64_t i = start; i < end; i += sysconf(_SC_PAGE_SIZE)) {
                    uint64_t offset;
                    // поиск смещения, по которому в pagemap находится информация о текущей странице
                    uint64_t index = i / sysconf(_SC_PAGE_SIZE) * sizeof(offset);

                    pread(pm_fd, &offset, sizeof(offset), index);
                    print_page(i, offset, out);
                }
            }
            row = strtok_r(NULL, "\n", &save_row);
        }
    }

    fclose(maps);
    close(pm_fd);
}

void write_stat(FILE *from, FILE *to) {
    char *names[] = {
            "pid",
            "comm",
            "state",
            "ppid",
            "pgrp",
            "session",
            "tty_nr",
            "tpgid",
            "flags",
            "minflt",
            "cminflt",
            "majflt",
            "cmajflt",
            "utime",
            "stime",
            "cutime",
            "cstime",
            "priority",
            "nice",
            "num_threads",
            "itrealvalue",
            "starttime",
            "vsize",
            "rss",
            "rsslim",
            "startcode",
            "endcode",
            "startstack",
            "kstkesp",
            "kstkeip",
            "signal",
            "blocked",
            "sigignore",
            "sigcatch",
            "wchan",
            "nswap",
            "сnswap",
            "exit_signal",
            "processor",
            "rt_priority",
            "policy",
            "delayacct_blkio_ticks",
            "guest_time",
            "cguest_time",
            "start_data",
            "end_data",
            "start_brk",
            "arg_start",
            "arg_end",
            "env_start",
            "env_end",
            "exit_code"
    };

    int i = 0;
    char buf[BUF_SIZE];
    fread(buf, 1, BUF_SIZE, from);
    char *pch = strtok(buf, " ");

    while (pch != NULL) {
        fprintf(to, "%s: %s\n", names[i], pch);
        pch = strtok(NULL, " ");
        i++;
    }
}

void write_comm(FILE *from, FILE *to) {
    char buf[BUF_SIZE];
    fscanf(from, "%s", buf);
    fprintf(to, "%s", buf);
}

void null_buf(char *buf) {
    for (int i = 0; i < BUF_SIZE; i++)
        buf[i] = '\0';

}

void write_dir(char *dirpath, FILE *to) {
    struct dirent *dirp;
    DIR *dp;
    char buf[BUF_SIZE];
    char path[BUF_SIZE];
    dp = opendir(dirpath);
    while ((dirp = readdir(dp)) != NULL) {
        if ((strcmp(dirp->d_name, ".") != 0) && (strcmp(dirp->d_name, "..") != 0)) {
            sprintf(path, "%s%s", "/proc/" PID "/fd/", dirp->d_name);
            readlink(path, buf, BUF_SIZE);
            fprintf(to, "%s -> %s\n", dirp->d_name, buf);
        }
        null_buf(buf);
    }
    closedir(dp);
}

int main(int argc, char *argv[]) {
    FILE *s = fopen("res1.txt", "w");
    FILE *f;

    fprintf(s, "\n---------------------\nCMDLINE\n---------------------\n");
    f = fopen("/proc/" PID "/cmdline", "r");
    write_from_file_to_file(f, s);
    fclose(f);

    f = fopen("/proc/" PID "/environ", "r");
    fprintf(s,
            "\n---------------------\nENVIRON\n---------------------\n");
    write_from_file_to_file(f, s);
    fclose(f);

    fprintf(s, "\n---------------------\nFD\n---------------------\n");
    write_dir("/proc/" PID "/fd", s);

    fprintf(s, "\n---------------------\nROOT\n---------------------\n");
    char buf[BUF_SIZE];
    readlink("/proc/" PID "/root", buf, BUF_SIZE);
    fprintf(s, "%s -> %s\n", "/proc/" PID "/root", buf);

    f = fopen("/proc/" PID "/stat", "r");
    fprintf(s, "\n---------------------\nSTAT\n---------------------\n");
    write_stat(f, s);
    fclose(f);

    f = fopen("/proc/" PID "/maps", "r");
    fprintf(s, "\n---------------------\nMAPS\n---------------------\n");
    write_from_file_to_file(f, s);
    fclose(f);

    f = fopen("/proc/" PID "/comm", "r");
    fprintf(s, "\n---------------------\nCOMM\n---------------------\n");
    write_comm(f, s);
    fclose(f);

    f = fopen("/proc/" PID "/io", "r");
    fprintf(s, "\n---------------------\nIO\n---------------------\n");
    write_from_file_to_file(f, s);
    fclose(f);

    f = fopen("/proc/" PID "/wchan", "r");
    fprintf(s, "\n---------------------\nWCHAN\n---------------------\n");
    write_from_file_to_file(f, s);
    fclose(f);

    fprintf(s, "\n---------------------\nTASK\n---------------------\n");
    write_dir("/proc/" PID "/task", s);

    get_pagemap_info(PID, s);

    fclose(s);
    return 0;
}