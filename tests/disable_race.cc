/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id: open_write_close.cc 54729 2013-03-26 16:24:56Z bkuszmaul $"

#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "backup.h"
#include "backup_internal.h"
#include "backup_test_helpers.h"
#include "backup_debug.h"

const int SIZE = 100;
const char ONE = 'a';
const char ZERO = 'b';
const unsigned int ASCII_OFFSET = 48;
const unsigned int N = 4;
static int FD_0 = 0;
static int FD_N = 0;
static int fd_array[N] = {0};

static void* write_ones(void *p) {
    const int * const nth_ptr = (const int * const)p;
    const int nth_fd = *nth_ptr;
    char data[SIZE] = {ONE};
    int pwrite_r = pwrite(fd_array[nth_fd], data, SIZE, 0);
    assert(pwrite_r == SIZE);
    return NULL;
}

static int verify(void)
{
    int result = 0;
    char *source_scratch = get_src();
    char *destination_scratch = get_dst();
    for (int i = 0; i < N; ++i) {
        char c = (char) i + ASCII_OFFSET;
        char source_file[SIZE];
        snprintf(source_file, SIZE, "%s/%c%c%c%c", source_scratch, c, c, c, c);
        char destination_file[SIZE];
        snprintf(destination_file, SIZE, "%s/%c%c%c%c", destination_scratch, c, c, c, c);
        int src_fd = open(source_file, O_RDWR);
        int dst_fd = open(destination_file, O_RDWR);

        char src_buf[SIZE];
        char dst_buf[SIZE];
        int r = pread(src_fd, src_buf, SIZE, 0);
        assert(r == SIZE);
        r = pread(dst_fd, dst_buf, SIZE, 0);
        assert(r == SIZE);

        if (src_buf[0] != dst_buf[0]) {
            printf("\nCharacters in Buffers should NOT  match: %c vs %c\n",
                   src_buf[0],
                   dst_buf[0]);
            result = -1;
        }

        close(dst_fd);
        close(src_fd);
    }
    
    free((void*)destination_scratch);
    free((void*)source_scratch);
    return result;
}

static void create_n_files(void)
{
    char file[SIZE];
    char *source_scratch = get_src();
    for (int i = 0; i < N; ++i) {
        char c = (char) i + ASCII_OFFSET;
        snprintf(file, SIZE, "%s/%c%c%c%c", source_scratch, c, c, c, c);
        printf("%s\n", file);
        fd_array[i] = open(file, O_CREAT | O_RDWR, 0777);
        assert(fd_array[i] >= 0);
        char data[SIZE] = {ZERO};
        int r = write(fd_array[i], data, SIZE);
        assert(r == SIZE);
    }

    free((void *) source_scratch);
}

static int disable_race(void) {
    int result = 0;

    // 1. Create N files with some data.
    create_n_files();

    // 2. Set Pause Point to mid point of disabling descriptions.
    HotBackup::toggle_pause_point(HotBackup::MANAGER_IN_DISABLE);
    backup_pause_disable(true);
    backup_set_keep_capturing(true);

    // 3. Start backup
    pthread_t backup_thread;
    start_backup_thread(&backup_thread);
    sleep(1); // Give the copier a change to grab the lock.

    // 6.  Allow disabling of descriptions to begin.
    printf("Allowing capturing to finish.\n");
    backup_set_keep_capturing(false);

    // 7. Create two threads, writing to the 0th and Nth files respectively. 
    pthread_t write_zero_thread;
    int first = 0;
    int r = pthread_create(&write_zero_thread, NULL, write_ones, &first); 
    assert(r == 0);
    pthread_t write_n_thread;
    int second = N - 1;
    r = pthread_create(&write_n_thread, NULL, write_ones, &second);
    assert(r == 0);

    // 8. Disable Pause Point.
    sleep(1);
    printf("Allowing disabling to continue.\n");
    HotBackup::toggle_pause_point(HotBackup::MANAGER_IN_DISABLE);
    backup_pause_disable(false);

    r = pthread_join(write_n_thread, NULL);
    assert(r == 0);
    r = pthread_join(write_zero_thread, NULL);
    assert(r == 0);
    sleep(4);
    printf("waiting for backup thread to finish...\n");
    finish_backup_thread(backup_thread);

    // 9. Verify that neither write made it to the backup file.
    result = verify();
    if (result != 0) {
        fail();
    } else {
        pass();
    }

    // Close all the open files.
    for (int i = 0; i < N; ++i) {
        int r = close(fd_array[i]);
        assert(r == 0);
    }

    return result;
}

int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    int result = 0;
    setup_source();
    setup_destination();
    result = disable_race();
    cleanup_dirs();
    return result;
}