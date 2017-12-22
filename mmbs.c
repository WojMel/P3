

#define _nbit(i)             (i & 0x1f)
#define _ibits(ptr,i)        (((uint32_t*)ptr)[ i >> 5 ])
#define getbit(ptr, i)       ((_ibits(ptr,i) >> _nbit(i)) & 1)
#define changebit(ptr, i, x) _ibits(ptr,i) ^= (-x ^ _ibits(ptr,i)) & (1 << _nbit(i))
#define setbit(ptr, i)       _ibits(ptr,i) |= 1 << _nbit(i)
#define clearbit(ptr, i)     _ibits(ptr,i) &= ~(1 << _nbit(i))
#define togglebit(ptr, i)    _ibits(ptr,i) ^= 1 << _nbit(i)

bool MM_INIT = false;
size_t PAGE_SIZE = sysconf(_SC_PAGESIZE);
size_t num_pages = 0;
void *page_tree = NULL;
size_t page_adrs_size = 0;
size_t page_adrs_cap = 0;
void **page_adrs = NULL;
size_t page_adrs_start_size = sizeof(void*)

void *malloc(size_t size){
   if( size == 0 ) return NULL;

   if( !MM_INIT ){
      mm_startup();
   }

   if( size > (PAGE_SIZE >> 1) ){
      // to do - give full pages
   }


   to_slot_size(&size);
   // find free slot
   // if failed create new page
}

static inline void to_slot_size(size_t *size){
   if( ! (*size >> 2) ){ *size = 4; return; }
   // https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
   --*size;
   *size |= *size >> 1;
   *size |= *size >> 2;
   *size |= *size >> 4;
   *size |= *size >> 8;
   *size |= *size >> 16;
   *size |= *size >> 32;
   ++*size;
}

void mm_startup(){
   void *page = mmap(0, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
   ++num_pages;
   init_page_tree(page, PAGE_SIZE >> 2);
}

void mm_cleanup(){

}

void init_page_tree(void *ptr, size_t size){
   // assert size > 2 and Ek 2^k = size
   page_tree = ptr;
   setbit(page_tree, 0);
   find_slot(page_tree, ptr, size >> 4);
}

void *find_slot(void *tree, void *page, size_t k){

   size_t offset = 0;


   return page + offset;
}