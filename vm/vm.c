/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "hash.h"
#include "threads/vaddr.h"
#include "threads/mmu.h"
#include "threads/palloc.h"
#include "lib/string.h"
#include "devices/disk.h"
#define ONE_MB (1 << 20) // 1MB    

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
	lru_clock = list_head(&frame_table);
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
static struct list_elem *get_next_lru_clock();

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
		
		/* 부모의 페이지를 복사한 경우 */
		if (type & VM_MARKER_1) {
			struct page *parent_page = (struct page *)aux;
			/* 보라 : 부모 페이지가 uninit 일 경우 lazyload에서 부모의 aux가 free 될 수 있음. calloc?memcpy? */
			void *_aux = (parent_page->uninit).aux;
			uninit_new(p, pg_round_down(upage), init, type, _aux, anon_initializer);
			
			p->writable = parent_page->writable;
			p->is_loaded = parent_page->is_loaded;
			p->f = parent_page->f;
			p->offset = parent_page->offset;
			p->read_bytes = parent_page->read_bytes;
			p->zero_bytes = parent_page->zero_bytes;

	
			if(parent_page->frame != NULL) {
				struct frame *f = vm_get_frame();
				f->page = p;
				p->frame = f;
				memcpy(f->kva, parent_page->frame->kva, PGSIZE);
				pml4_set_page(thread_current()->pml4, p->va, f->kva, p->writable);
			}
			/* Add the page to the process's address space. */
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
		/* stack 초기화 page인 경우 */
		if (type & VM_MARKER_0) {
			p->writable = true;
			p->is_loaded = true;
			vm_do_claim_page(p);
			return true;
		}

		// printf("vm alloc page init done %d!!!!!\n", page_get_type(p));
		return true;
	} else {
		// printf("vm initialize spt table is not null!!!!!!!!!!!\n");
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
	struct thread *cur = thread_current();
	 /* TODO: The policy for eviction is up to you. */
	while(true) {
		struct list_elem *clock = get_next_lru_clock();
		struct page *cur_page = list_entry(clock, struct frame, frame_elem)->page;
		if(!pml4_is_accessed(cur->pml4, cur_page->va)) {
			victim = cur_page->frame;
			break;
		}
		pml4_set_accessed(cur->pml4, cur_page->va, 0);
	}
	puts("found victim!!!============");
	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	puts("let's start evict!!!!!");
	struct frame *victim = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */
	if(victim != NULL) {
		puts("lets swap out in evict()");
		struct thread *cur = thread_current();
		struct page *found_p = victim->page;
		switch(page_get_type(found_p)) {
			case VM_ANON:
				swap_out(found_p);
				break;
			case VM_FILE:
				puts("file!!!!");
				if(pml4_is_dirty(cur->pml4, found_p->va)) {
					file_write_at(found_p->f, found_p->va, found_p->read_bytes, found_p->offset);
				}
				swap_out(found_p);
				break;
		}
	}
	return victim;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;
	/* TODO: Fill this function. */
	void* kva = palloc_get_page(PAL_USER);
	if (kva == NULL) {
		puts("need vicitm!!!!!!");
		frame = vm_evict_frame();
		puts("did evcit!!!!!!===========");
		// PANIC("todo vm_get_frame");
	} else {
		frame = (struct frame *)calloc(1, sizeof(struct frame));
		frame->kva = kva;
	}
	frame->page = NULL;
	add_frame_to_frame_table(frame);
	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	// puts("done get frame+++++++++++");
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
  vm_alloc_page(VM_ANON | VM_MARKER_0, addr, true);
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
	struct thread *cur = thread_current();
	struct supplemental_page_table *spt UNUSED = &cur->spt;
	struct page *page = spt_find_page(spt, addr);
  
	if (page == NULL) {
		if(USER_STACK - (uint64_t)addr <= ONE_MB){
		  if(f->rsp - 8 == addr) {
        for (uint64_t i = cur->stack_bottom - PGSIZE; pg_round_down(addr) <= i; i -= PGSIZE) {
          vm_stack_growth(i);
          cur->stack_bottom = i;
        }
				return true;
			}
		}
		return false;
	}
	if(write && !page->writable) {
		// printf("vm hanele fault page write!!\n");
		return false;
	}
	page->va = pg_round_down(addr);
	// printf("vm hanele fault page done!!\n");
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
	// printf("vm claim page %p!!\n", page->va);
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
	// printf("vm do claim page %p writable %s kva %p!!\n", page->va, page->writable ? "true" : "false", frame->kva);
	pml4_set_page(cur->pml4, page->va, frame->kva, page->writable);
	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	// printf("vm do claim page insert!!!!!!!%d va %p\n", page_get_type(page), page->va);
	if(page_get_type(page) & VM_MARKER_0) {
		return true;
	}
	return swap_in (page, frame->kva);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	hash_init(&spt->hash_page_table, page_hash, page_less, NULL);
}

static void copy_elem(struct hash_elem *hash_elem, void* aux) {
	struct page *p = hash_entry (hash_elem, struct page, hash_elem);
	vm_alloc_page_with_initializer(page_get_type(p) | VM_MARKER_1, p->va, p->writable, NULL, p);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {

	hash_apply(&src->hash_page_table, copy_elem);
	return true;
}

static void delete_elem(struct hash_elem *hash_elem, void* aux) {
	struct page *p = hash_entry(hash_elem, struct page, hash_elem);
	delete_frame(p);
	vm_dealloc_page(p);
}

void free_frame(void *kva) {
	struct list_elem *cur_elem = list_begin(&frame_table);

		while (cur_elem != list_end(&frame_table)) {
			struct frame *found_frame = list_entry(cur_elem, struct frame, frame_elem);
			if (found_frame->kva == kva) {
				delete_frame(found_frame->page);
				break;
			}
			cur_elem = list_next(cur_elem);
		}
}

void delete_frame(struct page *p) {
	if(p->frame != NULL) {
		pml4_clear_page(thread_current()->pml4, p->va);
	 	palloc_free_page(p->frame->kva);
		free(p->frame);
	}
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	hash_destroy(&spt->hash_page_table, delete_elem);
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
  struct hash_elem *e;
  struct supplemental_page_table *spt = &thread_current()->spt;
  
  p.va = pg_round_down(address);
	// printf("page lookup %p!!!!%p\n", address, p.va);
  e = hash_find (&spt->hash_page_table, &p.hash_elem);
  return e != NULL ? hash_entry (e, struct page, hash_elem) : NULL;
}


void add_frame_to_frame_table(struct frame *frame) {
	list_push_back(&frame_table, &frame->frame_elem);
}

void del_frame_from_frame_table(struct frame *frame) {
	list_remove(&frame->frame_elem);
}

static struct list_elem *get_next_lru_clock() {
	struct list_elem *next_clock = list_next(lru_clock);
	if (list_empty(&frame_table)) {
		return NULL;
	}
	if (next_clock == list_tail(&frame_table)) {
		next_clock = list_begin(&frame_table);
	}
	lru_clock = next_clock;
	return next_clock;
}
