// List all group member's name: Abhinay Reddy Vongur(av730), Anmol Sharma(as3593)
// username of iLab: ilabu3
// iLab Server: ilabu3.cs.rutgers.edu, kill.cs.rutgers.edu

#include "my_vm.h"

#define BITS 32
#define LEVELS 2

// global variables
static void* memory_start = NULL;
static char* phy_bitmap = NULL;
static int num_pages = 0;
static pde_t* page_dir = NULL;
static pte_t* page_tables = NULL;
static struct dt_map* dir_map = NULL;
static char* virt_bitmap = NULL;
struct tlb* tlb_store;
int tlb_hit = 0;
int tlb_miss = 0;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
struct partial_list* partial_pages;
struct alloc_list** alloted;


// constants --- REMOVE LATER
static int page_dir_bits;
static int page_table_bits;
static int offset_bits;
static int page_dir_entries;
static int page_table_entries;


// helper function declarations
static unsigned long extract_bits(unsigned long num, int start, int bits_len);
static void* form_virtual_address(int page_dir_index, int page_table_index);
static unsigned long* find_free_physical_page(int num_pages);
static void set_bit_at_index(char *bitmap, int index);
void* get_next_page(void* va);
static void unset_bit_at_index(char *bitmap, int index);
void arrange(tlb_node* node_to_arrange);
void delete_from_tlb(void* va);
void calculate_entries_bits();
void *check_partial_list(int size);
int free_part_page(unsigned long page_dir_index, unsigned long page_table_index, unsigned long offset, unsigned long size);
int is_part_page_valid(unsigned long page_num, unsigned long offset, unsigned long size);
void add_partial_page(void* va, int size);
void update_partial_list(void* va, int size);
int add_alloc_entry(void* va, int size);
void print_alloc_node(unsigned long page_dir_index, unsigned long page_table_index);
void delete_partial_node(void* va);


/*
Function responsible for allocating and setting your physical memory 
*/
void set_physical_mem() {

    //Allocate physical memory using mmap or malloc; this is the total size of
    //your memory you are simulating

    
    //HINT: Also calculate the number of physical and virtual pages and allocate
    //virtual and physical bitmaps and initialize them

    calculate_entries_bits();
    printf("Setting up memory\n");
    if (memory_start != NULL) {
        printf("Memory already set!\n");
    }

    memory_start = (void*) malloc(MEMSIZE);
    memset(memory_start, 0, MEMSIZE);
    num_pages = (int) MEMSIZE/PGSIZE;
    
    phy_bitmap = (char*) malloc(sizeof(char) * (int)ceil(num_pages/8.0));
    virt_bitmap = (char*) malloc(sizeof(char) * (page_dir_entries * page_table_entries) / 8);

    partial_pages = (struct partial_list*) malloc(sizeof(struct partial_list));
    partial_pages->head = NULL;
    partial_pages->tail = NULL;
    alloted = (struct alloc_list**) malloc(sizeof(struct alloc_list*) * num_pages);

    for(int i = 0; i < num_pages; i++) {
        alloted[i] = NULL;
    }

    for(int i = 0; i < (int)MEMSIZE/(8 * PGSIZE); i++) {
        phy_bitmap[i] = 0;
    }

    for(int i = 0; i < (page_dir_entries * page_table_entries)/8; i++){
        virt_bitmap[i] = 0;
    }

    page_dir = (pde_t*) find_free_physical_page(1)[0];
    set_bit_at_index(phy_bitmap, ((void*)page_dir - memory_start)/PGSIZE);
    // page_dir = (pde_t*) malloc(sizeof(pde_t) * PG_DIR_ENTRIES);

    for(int i = 0; i < page_dir_entries; i++){
        page_dir[i] = 0;
    }

    if (tlb_store == NULL){
        tlb_store = (struct tlb*) malloc(sizeof(struct tlb));
        tlb_store->head = NULL;
        tlb_store->tail = NULL;
        tlb_store->length = 0;
    }

}


/*
 * Part 2: Add a virtual to physical page translation to the TLB.
 * Feel free to extend the function arguments or return type.
 */
int
add_TLB(void *va, void *pa)
{

    /*Part 2 HINT: Add a virtual to physical page translation to the TLB */

    tlb_node* node=(tlb_node*) malloc(sizeof(tlb_node));
    if (node == NULL) {
        printf("Unable to allocate space for node in tlb\n");
        return -1;
    }

    node->pa=(pte_t)pa;
    node->va=va;
    node->next=NULL;

    if (tlb_store->head==NULL)
    {
        tlb_store->head=node;
        tlb_store->tail=node;
        tlb_store->length++;
        
    } else if (tlb_store->length < TLB_ENTRIES) {
        node->next=tlb_store->head;
        tlb_store->head=node;
        // tlb_store->tail->next=node;
        // tlb_store->tail=tlb_store->tail->next;
        tlb_store->length++;
    } else{
        // removing Least recently used node
        // tlb_node* prev=NULL;
        // tlb_node* present=NULL;
        // present=tlb_store->head;
        // while(present->next!=NULL){
        //     prev=present;
        //     present=present->next;
        // }
        // prev->next=node;
        // tlb_store->tail=prev->next;

        node->next = tlb_store->head->next;
        free(tlb_store->head);
        tlb_store->head = node;
    }

    return 0;
}


/*
 * Part 2: Check TLB for a valid translation.
 * Returns the physical page address.
 * Feel free to extend this function and change the return type.
 */
pte_t *
check_TLB(void *va) {

    /* Part 2: TLB lookup code here */
    if (tlb_store==NULL){
        printf("TLB not initialized\n");
        return NULL;
    }

    tlb_node* present=NULL;
    present=tlb_store->head;
    int flag=0;
    while (present!=NULL)
    {
       if(present->va==va){
           arrange(present);
           flag=1;
           break;
       }
       present=present->next;
    }
    if (flag==1){
        tlb_hit++;
        return &(present->pa);

    }
    else{
        tlb_miss++;
        return NULL;
    }

}


/*
 * Part 2: Print TLB miss rate.
 * Feel free to extend the function arguments or return type.
 */
void
print_TLB_missrate()
{
    double miss_rate = 0;	

    /*Part 2 Code here to calculate and print the TLB miss rate*/
    printf("TLB hit %d  TLB miss %d\n", tlb_hit, tlb_miss);

    miss_rate = (tlb_miss)/(double)(tlb_hit+tlb_miss);


    fprintf(stderr, "TLB miss rate %lf \n", miss_rate);
}



/*
The function takes a virtual address and page directories starting address and
performs translation to return the physical address
*/
pte_t *translate(pde_t *pgdir, void *va) {
    /* Part 1 HINT: Get the Page directory index (1st level) Then get the
    * 2nd-level-page table index using the virtual address.  Using the page
    * directory index and page table index get the physical address.
    *
    * Part 2 HINT: Check the TLB before performing the translation. If
    * translation exists, then you can return physical address from the TLB.
    */


    //If translation not successful, then return NULL
    

    unsigned long virtual_addr = (unsigned long)(va);

    unsigned long page_dir_index = extract_bits(virtual_addr, 1, page_dir_bits);
    unsigned long page_table_index = extract_bits(virtual_addr, page_dir_bits+1, page_table_bits);
    pte_t* physical_address = check_TLB(form_virtual_address(page_dir_index, page_table_index));

    if (physical_address != NULL){
        return physical_address;
    } 

    if (page_dir[page_dir_index] == NULL) {
        printf("Invalid virtual address\n");
        return NULL;
    }

    pte_t* page_table = (pte_t*) page_dir[page_dir_index];

    if (page_table[page_table_index] == NULL) {
        printf("Invalid virtual address\n");
        return NULL;
    }

    add_TLB(form_virtual_address(page_dir_index, page_table_index), (void*)page_table[page_table_index]);

    return &page_table[page_table_index];
}


/*
The function takes a page directory address, virtual address, physical address
as an argument, and sets a page table entry. This function will walk the page
directory to see if there is an existing mapping for a virtual address. If the
virtual address is not present, then a new entry will be added
*/
int page_map(pde_t *pgdir, void *va, void *pa)
{

    /*HINT: Similar to translate(), find the page directory (1st level)
    and page table (2nd-level) indices. If no mapping exists, set the
    virtual to physical mapping */

    unsigned long virtual_addr = (unsigned long)(va);
    unsigned long physical_addr = (unsigned long)(pa);

    unsigned long page_dir_index = extract_bits(virtual_addr, 1, page_dir_bits);
    unsigned long page_table_index = extract_bits(virtual_addr, page_dir_bits+1, page_table_bits);

    if (page_dir[page_dir_index] == NULL) {
        page_dir[page_dir_index] = (pde_t) find_free_physical_page(1)[0];
        set_bit_at_index(phy_bitmap, ((void*)page_dir[page_dir_index] - memory_start)/PGSIZE);
        // page_dir[page_dir_index] = (pde_t) malloc(sizeof(pte_t) * PG_TABLE_ENTRIES);
        pte_t* new_page_table = (pte_t*) page_dir[page_dir_index];
        for(int i = 0; i < page_table_entries; i++){
            new_page_table[i] = 0;
        }
    }

    pte_t* page_table = (pte_t*) page_dir[page_dir_index];


    if (page_table[page_table_index] != NULL) {
        printf("Page table entry not empty\n");
        return -1;
    }
    page_table[page_table_index] = physical_addr;

    return 0;
}


/*Function that gets the next available page
*/
void *get_next_avail(int num_pages) {
 
    //Use virtual address bitmap to find the next free page
    int page_count = 0;
    int no_chars = (page_dir_entries * page_table_entries) / 8;
    int i = 0;
    int j = 0;
    char num;
    int start_page = 0;
    while (i < no_chars){
        j = 0;
        num = virt_bitmap[i];
        while (j < 8){
            int bit = (num >> (8 - j - 1)) & 1;
            if (!bit) {
                if (page_count == 0) {
                    start_page = 8*i+j;
                } else if (page_count == num_pages-1 ||  page_count == num_pages){
                    return form_virtual_address(start_page/page_table_entries, start_page%page_table_entries);
                } 
                page_count++;
            } else {
                page_count = 0;
            }
            j++; 
        }
        i++;
    }
    return NULL;


}


/* Function responsible for allocating pages
and used by the benchmark
*/
void *t_malloc(unsigned int num_bytes) {

    /* 
     * HINT: If the physical memory is not yet initialized, then allocate and initialize.
     */

   /* 
    * HINT: If the page directory is not initialized, then initialize the
    * page directory. Next, using get_next_avail(), check if there are free pages. If
    * free pages are available, set the bitmaps and map a new page. Note, you will 
    * have to mark which physical pages are used. 
    */
    pthread_mutex_lock(&mutex);
    int pages_required = ceil(num_bytes / (float)PGSIZE);

    if (memory_start == NULL) {
        set_physical_mem();
        if (memory_start == NULL) {
            printf("Memory still not initialised\n");
            pthread_mutex_unlock(&mutex);
            return NULL;
        }
    }

    if (num_bytes < PGSIZE){
        void* partial_page = check_partial_list(num_bytes);
        if (partial_page != NULL){
            add_alloc_entry(partial_page, num_bytes);
            update_partial_list(partial_page, num_bytes);
            pthread_mutex_unlock(&mutex);
            return partial_page;
        }
    }
    void* va = get_next_avail(pages_required);
    unsigned long* phy_pages = find_free_physical_page(pages_required);
    if (phy_pages == NULL){
        printf("No physical memory available\n");
        pthread_mutex_unlock(&mutex);
        return NULL;
    }

    int page_map_status = 0;
    int page_dir_index;
    int page_table_index;
    void* va_copy = va;
    void* phy_page = (void*) find_free_physical_page(1)[0];
    for(int i = 0; i < pages_required; i++){
        page_dir_index = extract_bits((unsigned long)va, 1, page_dir_bits);
        page_table_index = extract_bits((unsigned long)va, page_dir_bits+1, page_table_bits);
        set_bit_at_index(virt_bitmap, page_dir_index*page_table_entries+page_table_index);
        set_bit_at_index(phy_bitmap, (phy_page - memory_start)/PGSIZE);
        page_map_status = page_map(page_dir, va, phy_page);
        if (num_bytes < PGSIZE){
            add_alloc_entry(va, num_bytes);
            add_partial_page(va, num_bytes);
        }
        phy_page = (void*) find_free_physical_page(1)[0];
        va = get_next_page(va);
        num_bytes -= PGSIZE;
    }

    pthread_mutex_unlock(&mutex);
    return va_copy;
}

/* Responsible for releasing one or more memory pages using virtual address (va)
*/
void t_free(void *va, int size) {

    /* Part 1: Free the page table entries starting from this virtual address
     * (va). Also mark the pages free in the bitmap. Perform free only if the 
     * memory from "va" to va+size is valid.
     *
     * Part 2: Also, remove the translation from the TLB
     */
    pthread_mutex_lock(&mutex);
    if (memory_start == NULL) {
        printf("Memory not set!\n");
        pthread_mutex_unlock(&mutex);
        return;
    }

    int pages_free = ceil(size / (float)PGSIZE);
    unsigned long virtual_addr = (unsigned long)(va);

    unsigned long page_dir_index = extract_bits(virtual_addr, 1, page_dir_bits);
    unsigned long page_table_index = extract_bits(virtual_addr, page_dir_bits+1, page_table_bits);
    unsigned long offset = extract_bits(virtual_addr, page_dir_bits+page_table_bits+1, offset_bits);

    if (page_dir[page_dir_index] == NULL) {
        printf("Invalid address to free\n");
        pthread_mutex_unlock(&mutex);
        return;
    }

    pte_t* page_table = (pte_t*) page_dir[page_dir_index];
    int size_copy = size;
    unsigned long offset_copy = offset;
    int i = 0;
    while(size_copy > 0) {
        if ((page_table[page_table_index+i] == NULL) || !is_part_page_valid(page_dir_index*page_table_entries+page_table_index+i, offset_copy, size_copy)) {
            printf("Invalid address to free\n");
            pthread_mutex_unlock(&mutex);
            return;
        }
        size_copy -= (PGSIZE - offset_copy);
        offset_copy = 0;
        i++;
    }

    void* physical_addr = (void*) page_table[page_table_index];
    int start_page_physical = (physical_addr - memory_start) / PGSIZE;
    int free_page = 1;
    i = 0;
    while (size > 0){
        free_page = free_part_page(page_dir_index, page_table_index+i, offset, size);
        size -= (PGSIZE - offset);
        offset = 0;
        if (!free_page){
            continue;
        }
        memset((void*)page_table[page_table_index+i], 0, PGSIZE);
        delete_from_tlb(form_virtual_address(page_dir_index, page_table_index+i));
        page_table[page_table_index+i] = 0;
        unset_bit_at_index(phy_bitmap, start_page_physical+i);
        unset_bit_at_index(virt_bitmap, page_dir_index*page_table_entries+page_table_index+i);
        i++;
    }
    pthread_mutex_unlock(&mutex);

}


/* The function copies data pointed by "val" to physical
 * memory pages using virtual address (va)
 * The function returns 0 if the put is successfull and -1 otherwise.
*/
void put_value(void *va, void *val, int size) {

    /* HINT: Using the virtual address and translate(), find the physical page. Copy
     * the contents of "val" to a physical page. NOTE: The "size" value can be larger 
     * than one page. Therefore, you may have to find multiple pages using translate()
     * function.
     */
    pthread_mutex_lock(&mutex);

    if (memory_start == NULL) {
        printf("Memory not set!\n");
        pthread_mutex_unlock(&mutex);
        return;
    }

    unsigned long virtual_addr = (unsigned long)(va);
    unsigned long page_dir_index = extract_bits(virtual_addr, 1, page_dir_bits);
    unsigned long page_table_index = extract_bits(virtual_addr, page_dir_bits+1, page_table_bits);
    int offset = (int) extract_bits(virtual_addr, (page_table_bits*(LEVELS-1))+page_dir_bits+1, offset_bits);
    unsigned long physical_address;
    
    if (PGSIZE - offset > size) {
        pte_t* pt_entry = translate(page_dir, va);
        if (!is_part_page_valid(page_dir_index*page_table_entries+page_table_index, offset, size)){
            int a = is_part_page_valid(page_dir_index*page_table_entries+page_table_index, offset, size);
            pthread_mutex_unlock(&mutex);
            printf("Invalid size/address for put_value\n");
            return;
        }
        physical_address = *pt_entry;
        physical_address += offset;
        memcpy((void*)physical_address, val, size);

    } else {
        int size_copy = size;
        int offset_copy = offset;
        void* va_copy = va;
        int i = 0;
        while (size_copy > 0) {
            pte_t* pt_entry = translate(page_dir, va_copy);
            if ((pt_entry == NULL)||(!is_part_page_valid(page_dir_index*page_table_entries+page_table_index+i, offset_copy, size_copy))){
                pthread_mutex_unlock(&mutex);
                printf("Invalid size/address for put_value\n");
                return;
            }
            physical_address = *pt_entry;
            size_copy -= (PGSIZE - offset_copy);
            offset_copy = 0;
            i++;
            va_copy = get_next_page(va_copy);
        }

        size_copy = size;
        while (size_copy > 0) {
            pte_t* pt_entry = translate(page_dir, va);
            if (pt_entry == NULL){
                printf("Something went wrong\n");
                pthread_mutex_unlock(&mutex);
                return;
            }
            physical_address = *pt_entry;
            physical_address += offset;
            memcpy((void*)physical_address, val, size_copy > PGSIZE - offset ? PGSIZE - offset : size_copy);
            val += size_copy > PGSIZE - offset ? PGSIZE-offset: size_copy;
            size_copy -= (PGSIZE - offset);
            offset = 0;
            va = get_next_page(va);
        }
    }
    pthread_mutex_unlock(&mutex);

}


/*Given a virtual address, this function copies the contents of the page to val*/
void get_value(void *va, void *val, int size) {

    /* HINT: put the values pointed to by "va" inside the physical memory at given
    * "val" address. Assume you can access "val" directly by derefencing them.
    */
    pthread_mutex_lock(&mutex);

    if (memory_start == NULL) {
        printf("Memory not set!\n");
        pthread_mutex_unlock(&mutex);
        return;
    }

    unsigned long virtual_addr = (unsigned long)(va);
    unsigned long page_dir_index = extract_bits(virtual_addr, 1, page_dir_bits);
    unsigned long page_table_index = extract_bits(virtual_addr, page_dir_bits+1, page_table_bits);
    int offset = (int) extract_bits(virtual_addr, page_table_bits*(LEVELS-1)+page_dir_bits+1, offset_bits);
    unsigned long physical_address;
    
    if (PGSIZE - offset > size) {
        pte_t* pt_entry = translate(page_dir, va);
        if ((pt_entry == NULL)|| (!is_part_page_valid(page_dir_index*page_table_entries+page_table_index, offset, size))){
            pthread_mutex_unlock(&mutex);
            printf("Invalid size/address for get_value\n");
            return;
        }
        physical_address = *pt_entry;
        physical_address += offset;
        memcpy(val, (void*)physical_address, size);

    } else {
        int size_copy = size;
        int offset_copy = offset;
        void* va_copy = va;
        int i = 0;
        while (size_copy > 0) {
            pte_t* pt_entry = translate(page_dir, va_copy);
            if (pt_entry == NULL){
                printf("Invalid size/address\n");
                pthread_mutex_unlock(&mutex);
                return;
            }
            if (!is_part_page_valid(page_dir_index*page_table_entries+page_table_index+i, offset, size)){
            pthread_mutex_unlock(&mutex);
            printf("Invalid size/address for get_value\n");
            return;
        }
            physical_address = *pt_entry;
            size_copy -= (PGSIZE - offset_copy);
            offset_copy = 0;
            i++;
            va_copy = get_next_page(va_copy);
        }

        size_copy = size;
        while (size_copy > 0) {
            pte_t* pt_entry = translate(page_dir, va);
            if (pt_entry == NULL){
                printf("Something went wrong\n");
                pthread_mutex_unlock(&mutex);
                return;
            }
            physical_address = *pt_entry;
            physical_address += offset;
            memcpy(val, (void*)physical_address, size_copy > PGSIZE - offset ? PGSIZE-offset: size_copy);
            val += PGSIZE - offset ? PGSIZE-offset: size_copy;
            size_copy -= (PGSIZE - offset);
            offset = 0;
            va = get_next_page(va);
        }
    }
    pthread_mutex_unlock(&mutex);


}



/*
This function receives two matrices mat1 and mat2 as an argument with size
argument representing the number of rows and columns. After performing matrix
multiplication, copy the result to answer.
*/
void mat_mult(void *mat1, void *mat2, int size, void *answer) {

    /* Hint: You will index as [i * size + j] where  "i, j" are the indices of the
     * matrix accessed. Similar to the code in test.c, you will use get_value() to
     * load each element and perform multiplication. Take a look at test.c! In addition to 
     * getting the values from two matrices, you will perform multiplication and 
     * store the result to the "answer array"
     */
    int x, y, val_size = sizeof(int);
    int i, j, k;
    for (i = 0; i < size; i++) {
        for(j = 0; j < size; j++) {
            unsigned int a, b, c = 0;
            for (k = 0; k < size; k++) {
                int address_a = (unsigned int)mat1 + ((i * size * sizeof(int))) + (k * sizeof(int));
                int address_b = (unsigned int)mat2 + ((k * size * sizeof(int))) + (j * sizeof(int));
                get_value( (void *)address_a, &a, sizeof(int));
                get_value( (void *)address_b, &b, sizeof(int));
                // printf("Values at the index: %d, %d, %d, %d, %d\n", 
                //     a, b, size, (i * size + k), (k * size + j));
                c += (a * b);
            }
            int address_c = (unsigned int)answer + ((i * size * sizeof(int))) + (j * sizeof(int));
            // printf("This is the c: %d, address: %x!\n", c, address_c);
            put_value((void *)address_c, (void *)&c, sizeof(int));
        }
    }
}


// helper functions

void calculate_entries_bits(){
    offset_bits = (int)log2(PGSIZE);
    page_table_entries = PGSIZE/sizeof(pte_t);
    page_dir_entries = PGSIZE/sizeof(pde_t);
    int remaining_bits = BITS - offset_bits;
    page_table_bits = (int)log2(page_table_entries);
    if (page_table_bits > remaining_bits){
        int bits_per_level = (BITS - offset_bits) / LEVELS;
        page_table_bits = bits_per_level;
        remaining_bits -= bits_per_level;
    } else {
        remaining_bits -= page_table_bits;
    }
    page_dir_bits = remaining_bits;
}

void arrange(tlb_node* node_to_arrange){

    if (tlb_store->head == tlb_store->tail) {
        return;
    }
    tlb_node* curr= tlb_store->head;
    tlb_node* prev= NULL;
    while (curr!=node_to_arrange){
        prev=curr;
        curr=curr->next;
    }
    if (prev == NULL){
        return;
    }
    prev->next=curr->next;
    curr->next=tlb_store->head;
    tlb_store->head=curr;
}

static unsigned long extract_bits(unsigned long num, int start, int bits_len) {
    unsigned long mask = (1 << bits_len) - 1;
    num = num >> (32 - bits_len - start + 1);
    return num & mask;
}

static void* form_virtual_address(int page_dir_index, int page_table_index) {
    
    int i = 0;
    unsigned long virt_addr = 4294967295;
    unsigned long extr_bit = 0;
    while(i < offset_bits){
        virt_addr = virt_addr >> 1;
        i++;
    }
    i = 0;
    while(i < page_table_bits){
        extr_bit = page_table_index & 1;
        page_table_index = page_table_index >> 1;
        extr_bit = extr_bit << 31;
        virt_addr = virt_addr >> 1 | extr_bit; 
        i++;
    }
    extr_bit = 0;
    i = 0;
    while(i < page_dir_bits){
        extr_bit = page_dir_index & 1;
        page_dir_index = page_dir_index >> 1;
        extr_bit = extr_bit << 31;
        virt_addr = virt_addr >> 1 | extr_bit; 
        i++;
    }
    return (void*) virt_addr;
}

static unsigned long* find_free_physical_page(int num_pages) {
    unsigned long* page_pointers = (unsigned long*) malloc(sizeof(unsigned long) * num_pages);
    int no_chars = (int)MEMSIZE/(8 * PGSIZE);
    int i = 0;
    int j = 0;
    char num;
    int page_count = 0;
    while (i < no_chars){
        j = 0;
        num = phy_bitmap[i];
        while (j < 8){
            int bit = (num >> (8 - j - 1)) & 1;
            if (!bit) {
                page_pointers[page_count] = (unsigned long)(memory_start + (8*(i) + j) * PGSIZE);
                page_count++;
                if (page_count == num_pages){
                    return page_pointers;
                }
            }
            j++; 
        }
        i++;
    }
    return NULL;
}

static void set_bit_at_index(char *bitmap, int index)
{
    //Implement your code here
    // get the index of the byte where the bit is in
    int byte_pos = (int) index/8;
    // get the index of the bit in the particular byte
    int bit_pos = index%8;
    // move the bit 1 to required position using right shift
    // use bitwise or operation to set the bit
    bitmap[byte_pos] = bitmap[byte_pos] | (128 >> bit_pos);
    return;
}

static void unset_bit_at_index(char *bitmap, int index)
{
    //Implement your code here
    // get the index of the byte where the bit is in
    int byte_pos = (int) index/8;
    // get the index of the bit in the particular byte
    int bit_pos = index%8;
    // move the bit 1 to required position using right shift
    // use bitwise or operation to set the bit
    int mask = 0;
    int i = 0;
    while (i < 8)
    {
        if (i == index) {
            mask = mask << 1 | 0;
        } else {
            mask = mask << 1 | 1;
        }
        i += 1;
    }
    
    bitmap[byte_pos] = bitmap[byte_pos] & mask;
    return;
}

void* get_next_page(void* va){
    unsigned long page_dir_index = extract_bits((unsigned long)va, 1, page_dir_bits);
    unsigned long page_table_index = extract_bits((unsigned long)va, page_dir_bits+1, page_table_bits);
    unsigned long offset = extract_bits((unsigned long)va, 21, 32);
    void* next_page;

    if (page_table_index == page_table_entries - 1) {
        if (page_dir_index == page_dir_entries - 1) {
            return NULL;
        }
        next_page = form_virtual_address(page_dir_index+1, 0);
        return next_page;
    }
     
    next_page = va + (PGSIZE - offset);
    return next_page;

}


void delete_from_tlb(void* va){

    if (tlb_store==NULL){
        printf("TLB not initialized\n");
    }

    if (tlb_store->head == NULL) {
        return;
    }

    if (tlb_store->head == tlb_store->tail && tlb_store->head->va == va){
        free(tlb_store->head);
        tlb_store->head = NULL;
        tlb_store->tail = NULL;
        tlb_store->length--;
        return;
    }

    tlb_node* present=NULL;
    tlb_node* prev=NULL;
    present=tlb_store->head;
    int flag=0;
    while (present!=NULL)
    {
       if(present->va==va){
           flag=1;
           break;
       }
       prev = present;
       present=present->next;
    }
    if (flag==1){
        if (tlb_store->head == present){
            tlb_store->head = present->next;
        } else if (tlb_store->tail == present) {
            prev->next = NULL;
            tlb_store->tail = prev;
        } else {
            prev->next = present->next;
        }
        free(present);
        tlb_store->length--;
        return;
    }
    else{
        return;
    }
}

void *check_partial_list(int size){
    if (partial_pages->head == NULL){
        return NULL;
    }
    partial_node* temp = partial_pages->head;
    while (temp != NULL)
    {
        if (PGSIZE - temp->space_taken >= size){
            return (void*)(temp->base_va + temp->space_taken + 1);
        }
        temp = temp->next;
    }

    return NULL;
}

int add_alloc_entry(void* va, int size){
    unsigned long page_dir_index = extract_bits((unsigned long)va, 1, page_dir_bits);
    unsigned long page_table_index = extract_bits((unsigned long)va, page_dir_bits+1, page_table_bits);

    struct alloc_list* page_alloc_list = alloted[page_dir_index*page_table_entries+page_table_index];
    alloc_node* new_node = (alloc_node*) malloc(sizeof(alloc_node));
    new_node->offset_start = extract_bits((unsigned long)va, page_dir_bits+page_table_bits+1, offset_bits);
    new_node->offset_end = new_node->offset_start + size - 1;
    new_node->free_status = 0;
    new_node->next = NULL;
    if (page_alloc_list == NULL){
        page_alloc_list = (struct alloc_list*) malloc(sizeof(struct alloc_list));
        alloted[page_dir_index*page_table_entries+page_table_index] = page_alloc_list;
        page_alloc_list->head = new_node;
        page_alloc_list->tail = new_node;
    } else {
    page_alloc_list->tail->next = new_node;
    page_alloc_list->tail = page_alloc_list->tail->next;
    }
}

void print_tlb(){
    printf("____________________________________\n");
    printf("Tlb length: %d\n", tlb_store->length);
    tlb_node* temp = tlb_store->head;
    while (temp != NULL)
    {
        printf("Virtual Address: %lu | Physical address: %lu\n", (unsigned long)temp->va, temp->pa);
        temp = temp->next;
    }
    printf("____________________________________\n");
}

void update_partial_list(void* va, int size) {
    unsigned long page_dir_index = extract_bits((unsigned long)va, 1, page_dir_bits);
    unsigned long page_table_index = extract_bits((unsigned long)va, page_dir_bits+1, page_table_bits);
    unsigned long offset = extract_bits((unsigned long)va, page_dir_bits+1, offset_bits);
    unsigned long va_no_off = (unsigned long)form_virtual_address(page_dir_index, page_table_index);

    partial_node* temp = partial_pages->head;
    partial_node* prev = NULL;
    while (temp != NULL)
    {
        if (temp->base_va == va_no_off){
            if (offset+size >= PGSIZE){
                if (prev == NULL) {
                    if(partial_pages->tail == partial_pages->head){
                        partial_pages->tail = partial_pages->tail->next;
                    }
                    partial_pages->head = partial_pages->head->next;
                } else {
                    if (partial_pages->tail == temp){
                        partial_pages->tail = prev;
                    }
                    prev->next = temp->next;
                }
            } else {
                temp->space_taken += size;
            }
            return;
        }
        prev = temp;
        temp = temp->next;
    }
    
}

void update_partial_list_free(void* va, int size) {
    unsigned long page_dir_index = extract_bits((unsigned long)va, 1, page_dir_bits);
    unsigned long page_table_index = extract_bits((unsigned long)va, page_dir_bits+1, page_table_bits);
    unsigned long offset = extract_bits((unsigned long)va, page_dir_bits+1, offset_bits);
    unsigned long va_no_off = (unsigned long)form_virtual_address(page_dir_index, page_table_index);

    partial_node* temp = partial_pages->head;
    partial_node* prev = NULL;
    while (temp != NULL)
    {
        if (temp->base_va == va_no_off){
            if (offset+size >= PGSIZE){
                if (prev == NULL) {
                    if(partial_pages->tail == partial_pages->head){
                        partial_pages->tail = partial_pages->tail->next;
                    }
                    partial_pages->head = partial_pages->head->next;
                } else {
                    if (partial_pages->tail == temp){
                        partial_pages->tail = prev;
                    }
                    prev->next = temp->next;
                }
            } else {
                temp->space_taken += size;
            }
            return;
        }
        prev = temp;
        temp = temp->next;
    }
    
}

void add_partial_page(void* va, int size){
    partial_node* new_node = (partial_node*) malloc(sizeof(partial_node));
    new_node->base_va = (unsigned long)va;
    new_node->space_taken = size;
    new_node->next = NULL;
    if (partial_pages->head == NULL){
        partial_pages->head = new_node;
        partial_pages->tail = new_node;
        return;
    }

    partial_pages->tail->next = new_node;
    partial_pages->tail = new_node;
}

int is_part_page_valid(unsigned long page_num, unsigned long offset, unsigned long size){
    struct alloc_list* page_alloc_list = alloted[page_num];
    if (page_alloc_list == NULL){
        return 1;
    }
    alloc_node* temp = page_alloc_list->head;
    if (temp == NULL) {
        return 1;
    }
    while (temp != NULL)
    {
        if (offset >= temp->offset_start && offset+size-1 <= temp->offset_end) {
            return 1;
        }
        temp = temp->next;
    }
    return 0;
}

int free_part_page(unsigned long page_dir_index, unsigned long page_table_index, unsigned long offset, unsigned long size){
    struct alloc_list* page_alloc_list = alloted[page_dir_index*page_table_entries+page_table_index];
    if (page_alloc_list == NULL){
        return 1;
    }
    alloc_node* temp = page_alloc_list->head;
    if (temp == NULL) {
        return 1;
    }
    int all_free = 1;
    while (temp != NULL)
    {
        if (offset >= temp->offset_start && offset+size-1 <= temp->offset_end) {
            temp->free_status = 1;
            temp->offset_start = 0;
            temp->offset_end = 0;
        }

        if (!temp->free_status) {
            all_free = 0;
        }
        temp = temp->next;
    }
    if (all_free){
        free(page_alloc_list);
        alloted[page_dir_index*page_table_entries+page_table_index] = NULL;
        delete_partial_node(form_virtual_address(page_dir_index, page_table_index));
        return 1;
    }
    return 0;
}

void delete_partial_node(void* va) {
    partial_node* temp = partial_pages->head;
    partial_node* prev = NULL;
    while (temp != NULL)
    {
        if (temp->base_va == (unsigned long)va){
            if (prev == NULL) {
                    if(partial_pages->tail == partial_pages->head){
                        partial_pages->tail = partial_pages->tail->next;
                    }
                    partial_pages->head = partial_pages->head->next;
                } else {
                    if (partial_pages->tail == temp){
                        partial_pages->tail = prev;
                    }
                    prev->next = temp->next;
                }
        }
        prev = temp;
        temp = temp->next;
    }
    
}

void print_alloc_node(unsigned long page_dir_index, unsigned long page_table_index){
    alloc_node* temp = alloted[page_dir_index*page_table_entries+page_table_index]->head;
    if (temp == NULL){
        printf("No allocations\n");
    }
    while (temp != NULL){
        printf("Offset start: %lu  Offset end: %lu  Isfree? %d\n", temp->offset_start, temp->offset_end, temp->free_status);
        temp = temp->next;
    }
}
