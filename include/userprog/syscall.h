// * USERPROG 추가
#include <stdbool.h>
#include "threads/thread.h"

#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

struct lock filesys_lock;

void syscall_init (void);

// * 추가
void check_address(void *addr);
void check_valid_buffer(void *buffer, unsigned size, bool writable);
void check_valid_string(const void *str, unsigned size);

// * syscall 추가
void halt(void);
void exit(int status);
int fork (const char *thread_name);
int exec (const char *file_name);
int wait (tid_t pid);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned size);
int write(int fd, const void *buffer, unsigned size);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);
void *mmap (void *addr, size_t length, int writable, int fd, off_t offset);
void munmap (void *addr);
off_t file_write_with_lock (struct file *file, void *buffer, off_t size, off_t file_ofs);
off_t file_read_with_lock (struct file *file, void *buffer, off_t size, off_t file_ofs);



#endif /* userprog/syscall.h */
