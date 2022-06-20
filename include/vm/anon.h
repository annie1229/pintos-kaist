#ifndef VM_ANON_H
#define VM_ANON_H
#include "vm/vm.h"
#include "devices/disk.h"
struct page;
enum vm_type;

struct anon_page {
};

void vm_anon_init (void);
bool anon_initializer (struct page *page, enum vm_type type, void *kva);
bool anon_child_swap_in (struct page *parent_page, void *kva);
void delete_swap_slot (disk_sector_t swap_slot);
#endif
