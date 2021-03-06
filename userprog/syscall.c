#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

// * USERPROG 추가
#include "threads/palloc.h"
#include "filesys/filesys.h"
#include "filesys/file.h"

#include "vm/vm.h"
#include "threads/vaddr.h"
#include "userprog/process.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {

  lock_init(&filesys_lock);

	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.
  switch (f->R.rax) {
    case SYS_HALT:
      halt();
      break;
    case SYS_EXIT:
      exit(f->R.rdi);
      break;
    case SYS_FORK:
      memcpy(&thread_current()->ptf, f, sizeof(struct intr_frame));
      f->R.rax = (uint64_t)fork(f->R.rdi);
      break;
    case SYS_EXEC:
      exec(f->R.rdi);
      break;
    case SYS_WAIT:
      f->R.rax = (uint64_t)wait(f->R.rdi);
      break; 
    case SYS_CREATE:
      f->R.rax = (uint64_t)create(f->R.rdi, f->R.rsi);
      break;
    case SYS_REMOVE:
      f->R.rax = (uint64_t)remove(f->R.rdi);
      break;
    case SYS_OPEN:
      f->R.rax = (uint64_t)open(f->R.rdi);
      break;
    case SYS_FILESIZE:
      f->R.rax = (uint64_t)filesize(f->R.rdi);
      break;
    case SYS_READ:
      f->R.rax = (uint64_t)read(f->R.rdi, f->R.rsi, f->R.rdx);
      break;  
    case SYS_WRITE:     
      f->R.rax = (uint64_t)write(f->R.rdi, f->R.rsi, f->R.rdx);
      break;
    case SYS_SEEK:
      seek(f->R.rdi, f->R.rsi);
      break;
    case SYS_TELL:
      f->R.rax = (uint64_t)tell(f->R.rdi);
      break;
    case SYS_CLOSE:
      close(f->R.rdi);
      break;
    case SYS_MMAP:
      f->R.rax = mmap(f->R.rdi, f->R.rsi, f->R.rdx, f->R.r10, f->R.r8);
      break;
    case SYS_MUNMAP:
      munmap(f->R.rdi);
      break;
    default:
      exit(-1);
      break;
  }
}

void halt(void) {
  // * power_off()를 사용하여 pintos 종료
  power_off();
}

void exit(int status) {
  // puts("exit!!");
  /*
   * 실행중인 스레드 구조체를 가져옴
   * 프로세스 종료 메시지 출력
   * 출력 양식: "프로세스 이름: exit(종료상태)"
   * thread 종료
   */ 
  struct thread *cur = thread_current();
  cur->exit_status = status;
  printf("%s: exit(%d)\n", cur->name, status);
  thread_exit();
}

int fork (const char *thread_name) {
  // puts("fork!!");
  check_address(thread_name);
  int ret = process_fork(thread_name, &thread_current()->ptf);
  return ret;
}

int exec (const char *file_name) {
  // puts("exec!!");
  check_address(file_name);

  int file_size = strlen(file_name) + 1;
  char *fn_copy = palloc_get_page(PAL_ZERO);
  if (!fn_copy) {
    exit(-1);
    return -1;
  }
  strlcpy(fn_copy, file_name, file_size);
  if (process_exec(fn_copy) == -1) {
    exit(-1);
    return -1;
  }
}

int wait (tid_t pid) {
  // puts("wait!!");
  return process_wait(pid);
}

bool create (const char *file, unsigned initial_size) {

  // puts("create!!");
  /* 
   * 파일 이름과 크기에 해당하는 파일 생성
   * 파일 생성 성공 시 true 반환, 실패 시 false 반환
   */
  check_address(file);
  lock_acquire(&filesys_lock);
  bool result = filesys_create(file, initial_size);
  lock_release(&filesys_lock);
  return result;
}

bool remove (const char *file) {
  // puts("remove!!");
  /* 
   * 파일 이름에 해당하는 파일을 제거
   * 파일 제거 성공 시 true 반환, 실패 시 false 반환
   */
  check_address(file);
  lock_acquire(&filesys_lock);
  bool result = filesys_remove(file);
  lock_release(&filesys_lock);
  return result;
}

int open (const char *file) {
  check_address(file);
  struct thread *cur = thread_current();
  lock_acquire(&filesys_lock);
  struct file *fd = filesys_open(file);
  lock_release(&filesys_lock);
  if (fd) {
    for (int i = 2; i < FD_MAX; i++) {
      if (!cur->fdt[i]) {
        cur->fdt[i] = fd;
        cur->next_fd = i + 1;
        return i;
      }
    }
    lock_acquire(&filesys_lock);
    file_close(fd);
    lock_release(&filesys_lock);
  }
  return -1;
}

int filesize (int fd) {
  struct file *file = thread_current()->fdt[fd];
  if (file) {
    lock_acquire(&filesys_lock);
    int length = file_length(file);
    lock_release(&filesys_lock);
    return length;
  }
  return -1;
}

int read (int fd, void *buffer, unsigned size) {
  // puts("read!!");
  check_valid_buffer(buffer, size, true);
  if (fd == 1) {
    return -1;
  }

  if (fd == 0) {
    lock_acquire(&filesys_lock);
    int byte = input_getc();
    lock_release(&filesys_lock);
    return byte;
  }
  struct file *file = thread_current()->fdt[fd];
  if (file) {
    lock_acquire(&filesys_lock);
    int read_byte = file_read(file, buffer, size);
    lock_release(&filesys_lock);
    return read_byte;
  }
  return -1;
}

int write (int fd UNUSED, const void *str, unsigned size) {
  // puts("write!!");
  check_valid_string(str, size);

  if (fd == 0) // STDIN일때 -1
    return -1;

  if (fd == 1) {
    lock_acquire(&filesys_lock);
	  putbuf(str, size);
    lock_release(&filesys_lock);
    return size;
  }

  struct file *file = thread_current()->fdt[fd];
  if (file) {
    lock_acquire(&filesys_lock);
    int write_byte = file_write(file, str, size);
    lock_release(&filesys_lock);
    return write_byte;
  }
}

void seek (int fd, unsigned position) {
  struct file *curfile = thread_current()->fdt[fd];
  if (curfile) {
    lock_acquire(&filesys_lock);
    file_seek(curfile, position);
    lock_release(&filesys_lock);
  }
}

unsigned tell (int fd) {
  struct file *curfile = thread_current()->fdt[fd];
  if (curfile) {
    lock_acquire(&filesys_lock);
    unsigned result = file_tell(curfile);
    lock_release(&filesys_lock);
    return result;
  }
  return -1;
}

void close (int fd) {
  // puts("close!!");
  struct file * file = thread_current()->fdt[fd];
  if (file) {
    thread_current()->fdt[fd] = NULL;
    lock_acquire(&filesys_lock);
    file_close(file);
    lock_release(&filesys_lock);
  }
}

void check_address(void *addr) {
  struct thread *cur = thread_current();
#ifdef VM
  if (addr == NULL || is_kernel_vaddr(addr) || spt_find_page(&cur->spt, addr) == NULL) {
    exit(-1);
  }
#else
  if (addr == NULL || is_kernel_vaddr(addr) || pml4_get_page(cur->pml4, addr) == NULL){
    exit(-1);
  }
#endif
}

void check_valid_buffer(void *buffer, unsigned size, bool writable) {
  check_address(buffer);
  struct thread *cur = thread_current();

  for(int i=0; i < size; i += PGSIZE) {
    struct page *p = spt_find_page(&cur->spt, buffer + i);
    if(!p->writable) {
      exit(-1);
    }
  }
}

void check_valid_string(const void *str, unsigned size) {
  check_address(str);
  struct thread *cur = thread_current();

  for(int i=0; i < size; i += PGSIZE) {
    struct page *p = spt_find_page(&cur->spt, str + i);
    if(p == NULL) {
      exit(-1);
    }
  }
}

void *mmap (void *addr, size_t length, int writable, int fd, off_t offset) {
  if(addr == 0 || length == 0 || KERN_BASE - USER_STACK < length || fd == 0 || fd == 1|| pg_ofs (addr) != 0 || length < offset || is_kernel_vaddr(addr))
    return NULL;
  struct file *f = thread_current()->fdt[fd];
  lock_acquire(&filesys_lock);
  struct file *open_file = file_reopen(f);
  lock_release(&filesys_lock);
  return do_mmap(addr, length, writable, open_file, offset);
}

void munmap (void *addr) {
  if(!do_munmap(addr)) {
    exit(-1);
  };
}

off_t file_write_with_lock (struct file *file, void *buffer, off_t size, off_t file_ofs) {
    lock_acquire(&filesys_lock);
    off_t ret = file_write_at (file, buffer, size, file_ofs);
    lock_release(&filesys_lock);
    return ret;
}

off_t file_read_with_lock (struct file *file, void *buffer, off_t size, off_t file_ofs) {
    lock_acquire(&filesys_lock);
    off_t ret = file_read_at (file, buffer, size, file_ofs);
    lock_release(&filesys_lock);
    return ret;
}
