#ifndef VM_FILE_H
#define VM_FILE_H
#include "filesys/file.h"
#include "vm/vm.h"

struct page;
enum vm_type;

struct file_page {
};

void vm_file_init (void);
bool file_backed_initializer (struct page *page, enum vm_type type, void *kva);
void *do_mmap(void *addr, size_t length, int writable,
		struct file *file, off_t offset);
bool do_munmap (void *va);
void mmap_hash_init (struct hash *m_hash);
bool mmap_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux);
unsigned mmap_hash (const struct hash_elem *mf_, void *aux);
#endif
