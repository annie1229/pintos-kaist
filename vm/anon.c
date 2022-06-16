/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"
#include "lib/kernel/bitmap.h"
#include "threads/mmu.h"
/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

static struct swap_table {
	size_t size;
	struct bitmap *used;
};

static struct swap_table *swap_table;

/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	// puts("anon int!!");
	swap_disk = disk_get(1, 1);
	swap_table = calloc(1, sizeof(struct swap_table));
	swap_table->size = disk_size(swap_disk);
	printf("size!! : %d\n", swap_table->size);
	swap_table->used = bitmap_create(swap_table->size);
	printf("bits size!!!! %d\n", bitmap_size (swap_table->used));
	// printf("bits not used start idx !!!! %d\n", bitmap_scan_and_flip(swap_table->used, 0, 8, false));
	// puts("init done!!");
}

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	// printf("anon initialize!!!!!!!!111\n");
	page->operations = &anon_ops;
	// printf("anon initialize!!!!!!!!222\n");

	struct anon_page *anon_page = &page->anon;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	// puts("anon_swap_in!!!!!!!!!");
	struct anon_page *anon_page = &page->anon;
	size_t idx = anon_page->swap_slot;
	void *addr = kva;
	for(int i = idx; i < idx + 8; i++) {
		disk_read(swap_disk, i, addr);
		addr += DISK_SECTOR_SIZE;
	}
	bitmap_set_multiple(swap_table->used, idx, 8, false);
	return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	// printf("anon swap out!!! page->va %p\n", page->va);
	struct anon_page *anon_page = &page->anon;
	size_t idx = bitmap_scan_and_flip(swap_table->used, 0, 8, false);

	if (idx == BITMAP_ERROR) {
		// puts("BITMAP_ERROR!!!!!!!!");
		return false;
	}
	anon_page->swap_slot = idx;

	// printf("anon swap slot!!! idx %u\n", idx);
	void *addr = page->va;
	for(int i = idx; i < idx + 8; i++) {
		disk_write(swap_disk, i, addr);
		addr += DISK_SECTOR_SIZE;
	}
	del_frame_from_frame_table(page->frame);
	page->frame = NULL;
	// printf("anon swap del_frame_from_frame_table!!!\n");
	// delete_frame(page);
	return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
}
