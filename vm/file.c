/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/mmu.h"
#define PGBITS  12                         /* Number of offset bits. */
#define PGSIZE  (1 << PGBITS)              /* Bytes in a page. */

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);
static bool lazy_load_segment (struct page *page, void *aux);
/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;
	struct file_page *file_page = &page->file;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
	// printf("do_mmap start==============\n");
	struct thread *cur = thread_current();
  struct mmap_file *mf = calloc(1, sizeof(struct mmap_file));
	list_init(&mf->vme_list);
	mf->file = file;
	mf->va = addr;

	size_t read_bytes = file_length(file);
	size_t zero_bytes = length - read_bytes;

	while (0 < read_bytes) {
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;
		// printf("do_mmap while length %d, read_bytes %d, offset %d\n", length, page_read_bytes, offset);

		/* TODO: Set up aux to pass information to the lazy_load_segment. */
		struct file_info *aux = calloc(1, sizeof(struct file_info));
		aux->file = file;
		aux->offset = offset;
		aux->read_bytes = page_read_bytes;
		aux->zero_bytes = page_zero_bytes;
		aux->writable = writable;
		aux->is_loaded = false;
		// printf("do mmap >>>>> addr %p read %d zero %d writable %s\n", addr, aux->read_bytes, aux->zero_bytes, writable ? "true" : "false");
		if (!vm_alloc_page_with_initializer (VM_FILE, addr, writable, lazy_load_segment, aux)) {
			// printf("do mmap vm alloc page fail!!!\n");
			return NULL;
		}
		
		struct page *p = spt_find_page(&cur->spt, addr);
		list_push_back(&mf->vme_list, &p->mmap_elem);

		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		offset += page_read_bytes;
		addr += PGSIZE;
	}
	
	// printf("to_insert_va %p\n", mf->va);
	// printf("to_insert_va %p\n", cur->mmap_hash);
	if (hash_insert(&cur->mmap_hash, &mf->elem) != NULL) {
		// printf("hash insert fail!!!!!! addr %p\n\n", addr);
		// return mmap_f->va;
		return NULL;
	}
	// printf("do_mmap done==============\n");
	return mf->va;
}


void
mmap_hash_init (struct hash *m_hash UNUSED) {
	hash_init(m_hash, mmap_hash, mmap_less, NULL);
}


/* Returns true if page a precedes page b. */
bool mmap_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED) {
  const struct mmap_file *a = hash_entry (a_, struct mmap_file, elem);
  const struct mmap_file *b = hash_entry (b_, struct mmap_file, elem);
	// printf("hash>>>>>page_less %p &&&& %p\n", a->va, b->va);
  return a->va < b->va;
}

/* Returns a hash value for page p. */
unsigned
mmap_hash (const struct hash_elem *mf_, void *aux UNUSED) {
  const struct mmap_file *mf = hash_entry (mf_, struct mmap_file, elem);
  return hash_bytes (&mf->va, sizeof mf->va);
}

void copy_elem(struct hash_elem *hash_elem, void* aux) {
	struct page *parent_p = hash_entry (hash_elem, struct page, hash_elem);
	struct page *child_p = spt_find_page(&thread_current()->spt, parent_p->va);
	hash_insert(&thread_current()->mmap_hash, &child_p->mmap_elem);
}

/* Copy supplemental page table from src to dst */
bool
mmap_hash_table_copy (struct hash *dst UNUSED, struct hash *src UNUSED) {
	hash_apply(src, copy_elem);
	return true;
}

static bool
lazy_load_segment (struct page *page, void *aux) {
	/* TODO: Load the segment from the file */
	/* TODO: This called when the first page fault occurs on address VA. */
	/* TODO: VA is available when calling this function. */
	// printf("-----------------lazy load seg mmap!!!!!!!!!!!!!!!\n");
	// printf("va : %p, kva: %p\n", page->va, page->frame->kva);
	struct file_info *f_info = (struct file_info *)aux;
	
	page->f = f_info->file;
	page->offset = f_info->offset;
	page->read_bytes = f_info->read_bytes;
	page->zero_bytes = f_info->zero_bytes;
	page->writable = f_info->writable;
	page->is_loaded = f_info->is_loaded;
	// printf("lazy load file_read start!!!!!!!file %p, kva %p\n", f_info->file, page->frame->kva);
	// printf("read bytes %d, zero bytes %d\n", page->read_bytes, page->zero_bytes);
	if (file_read_at(f_info->file, page->frame->kva, f_info->read_bytes, f_info->offset) != (int) f_info->read_bytes) {
		// printf("lazy load file_read mmap fail!!!!!!!!!!!\n");
		free(aux);
		vm_dealloc_page(page);
		return false;
	}
	// printf("lazy load file_read succ!!!!!!!!!!!\n");
	memset (page->frame->kva + f_info->read_bytes, 0, f_info->zero_bytes);
	page->is_loaded = true;
	free(aux);
	// printf("-----------------lazy load seg mmap done!!!!!!!!!!!!!!!\n");
	return true;
}


/* Do the munmap */
bool
do_munmap (void *addr) {
	// printf("do_munmap start==============\n");
	struct thread *cur = thread_current();
	struct mmap_file mf;
	mf.va = addr;

	struct hash_elem *e;
	e = hash_find(&cur->mmap_hash, &mf.elem);
	if(e == NULL) {
		// printf("do_munmap fail\n");
		return false;
	}
	
	struct mmap_file *found_mf = hash_entry (e, struct mmap_file, elem);
	while (!list_empty (&found_mf->vme_list)) {
		// puts("circle!!!!!!!!!!!!!!");
		struct list_elem *list_elem = list_pop_front (&found_mf->vme_list);
		struct page *p = list_entry(list_elem, struct page, mmap_elem);
		if(pml4_is_dirty (thread_current()->pml4, p->va)) {
			// puts("dirrrrrrrrrrrrrrrrrrrty");
			file_write_at(p->f, p->va, p->read_bytes, p->offset);
		}
		delete_frame(p);
		hash_delete(&cur->spt.hash_page_table, &p->hash_elem);
		vm_dealloc_page(p);
	}
	/*파일삭제?! */ 
	free(found_mf);
	// printf("do_munmap done==============\n");
	return true;
}

static void delete_elem(struct hash_elem *hash_elem, void* aux) {
	struct page *p = hash_entry(hash_elem, struct page, hash_elem);
	delete_frame(p);
	vm_dealloc_page(p);
	/*file_close*/
}

static void 
delete_mmap (struct hash_elem *elem, void *aux) {
	struct mmap_file *found_mf = hash_entry (elem, struct mmap_file, elem);
	do_munmap(found_mf->va);
}

void
mmap_hash_kill (struct hash *hash) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	hash_destroy(hash, delete_mmap);
}

