/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "hash.h"
#include "threads/vaddr.h"

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
	list_init(&frame_table);
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);
static struct frame *vm_get_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {
	// printf("vm alloc page init!!!!!%d, %d\n", type, VM_TYPE(type));
	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		struct page *p = (struct page *)calloc(1, sizeof(struct page));
		p->va = pg_round_down(upage);
		// printf("type : %d, marker %d, & %d\n", type, VM_MARKER_0, type & VM_MARKER_0);
		if (type & VM_MARKER_0) {
			struct frame *f = (struct frame *)vm_get_frame();
			f->page = p;
			p->frame = f;
			p->writable = true;
			p->is_loaded = true;
			/* Add the page to the process's address space. */
			pml4_set_page(thread_current()->pml4, p->va, f->kva, p->writable);
			// hash_insert(&spt->hash_page_table, &p->hash_elem);
			spt_insert_page(spt, p);
			return true;
		}
		switch (VM_TYPE(type))
		{
			case VM_ANON:
				uninit_new(p, pg_round_down(upage), init, type, aux, anon_initializer);
				break;
			case VM_FILE:
				uninit_new(p, pg_round_down(upage), init, type, aux, file_backed_initializer);
				break;
			default:
				goto err;
		}
		/* TODO: Insert the page into the spt. */
		p->writable = writable;
		spt_insert_page(spt, p);

		// printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>initialize insert upage %u\n", upage);
		// printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>initialize insert page %u\n", p->va);
		// printf("vm alloc page init done!!!!!\n");
		return true;
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function. */
	page = page_lookup(va);
	// printf("spt find!!!!!!!%p\n", page);
	return page;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	int succ = false;
	/* TODO: Fill this function. */
	page->va = pg_round_down(page->va);
	if(hash_insert(&spt->hash_page_table, &page->hash_elem) == NULL) {
		succ = true;
		// printf("spt insert succ!!!!!\n");
		return succ;
	};
	// printf("spt insert fail!!!!!\n");
	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = (struct frame *)calloc(1, sizeof(struct frame));
	/* TODO: Fill this function. */
	void* kva = palloc_get_page(PAL_USER);
	if (kva == NULL) {
		frame = NULL;
		PANIC("todo vm_get_frame");
	}
	frame->kva = kva;
	frame->page = NULL;
	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	// printf("======call vm try handle fault=====\n");
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	/* TODO: Validate the fault */
	struct page *page = spt_find_page(spt, addr);
	// struct page *page = (struct page *)malloc(sizeof(struct page));

	// if(page != NULL && page->is_loaded) {
	// 	printf("vm hanele fault page trueeeeeee!!\n");
	// 	return true;
	// }

	// if(page != NULL && not_present) {
	// 	printf("vm hanele fault page trueeeeeee!!\n");
	// 	return anon_initializer(page, VM_ANON, page->frame->kva);
	// 	// return true;
	// }
	if (page == NULL) {
		// printf("vm hanele fault page nulllllllll!! %p\n", addr);
		return false;
	}
	if(write && !page->writable) {
		// printf("vm hanele fault page write!!\n");
		return false;
	}
	// if(not_present) {
	// 	printf("vm hanele fault page not present!!\n");
	// 	return false;
	// }
	page->va = pg_round_down(addr);
	// printf("vm hanele fault page done!!\n");
	struct file_info *aux2 = (struct file_info *)page->uninit.aux;

	// printf("do claim pg read %d, zero %d\n", aux2->read_bytes, aux2->zero_bytes);
	return vm_do_claim_page (page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = spt_find_page(&thread_current()->spt, va);
	/* TODO: Fill this function */
	printf("vm claim page %p!!\n", page->va);
	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct thread *cur = thread_current();
	struct frame *frame = vm_get_frame ();
	struct supplemental_page_table *spt = &cur->spt;

	/* Set links */
	frame->page = page;
	page->frame = frame;
	// printf("vm do claim page %p writable %s!!\n", page->va, page->writable ? "true" : "false");
	pml4_set_page(cur->pml4, page->va, frame->kva, page->writable);
	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	// printf("vm do claim page insert!!!!!!!%d\n", VM_TYPE(page->operations->type));
	struct file_info *aux2 = (struct file_info *)(page->uninit.aux);

	// printf("do claim pg read %d, zero %d\n", aux2->read_bytes, aux2->zero_bytes);
	return swap_in (page, frame->kva);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	hash_init(&spt->hash_page_table, page_hash, page_less, NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
	// struct hash_iterator i;

  // hash_first (&i, src);
  // while (hash_next (&i))
  // {
  // 	struct page *p = hash_entry (hash_cur (&i), struct page, page_table_elem);
	// 	hash_insert(dst, &p->page_table_elem);
	// }
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
}

/* Returns a hash value for page p. */
unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED) {
  const struct page *p = hash_entry (p_, struct page, hash_elem);
  return hash_bytes (&p->va, sizeof p->va);
}

/* Returns true if page a precedes page b. */
bool page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED) {
  const struct page *a = hash_entry (a_, struct page, hash_elem);
  const struct page *b = hash_entry (b_, struct page, hash_elem);
	// printf("hash>>>>>page_less %p &&&& %p\n", a->va, b->va);

  return a->va < b->va;
}

/* Returns the page containing the given virtual address, or a null pointer if no such page exists. */
struct page *
page_lookup (const void *address) {
  struct page p;
  // struct page *p;
  struct hash_elem *e;
  struct supplemental_page_table *spt = &thread_current()->spt;
  
  p.va = pg_round_down(address);
	// printf("page lookup %p!!!!%p\n", address, p.va);
  e = hash_find (&spt->hash_page_table, &p.hash_elem);
	
  // (*p).va = pg_round_down(address);
	// printf("page lookup %p!!!!%p\n", address, (*p).va);
  // e = hash_find (&spt->hash_page_table, &p->hash_elem);
  return e != NULL ? hash_entry (e, struct page, hash_elem) : NULL;
}
