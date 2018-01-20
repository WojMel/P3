#include <unistd.h>
#include <sys/mman.h>
#include <stdbool.h>
#include <stdint.h>

#define _nbit(i)             (i & 0x1f)
#define _ibits(ptr,i)        (((uint32_t*)ptr)[ i >> 5 ])
#define getbit(ptr, i)       ((_ibits(ptr,i) >> _nbit(i)) & 1)
#define changebit(ptr, i, x) _ibits(ptr,i) ^= (-x ^ _ibits(ptr,i)) & (1 << _nbit(i))
#define setbit(ptr, i)       _ibits(ptr,i) |= 1 << _nbit(i)
#define clearbit(ptr, i)     _ibits(ptr,i) &= ~(1 << _nbit(i))
#define togglebit(ptr, i)    _ibits(ptr,i) ^= 1 << _nbit(i)

bool MM_RUNNING = false;
size_t PAGE_SIZE;
size_t num_pages = 0;
void **page_tree = NULL;
size_t page_adrs_cap = 0;
void **page_adrs = NULL;

void mm_startup();
void *find_slot_page(void *tree, void *page, size_t k);
void *find_slot(size_t k);
static inline void to_slot_size(size_t *size);

void *malloc(size_t size);
void *realloc(void *ptr, size_t size);
void free(void *ptr);
void *calloc(size_t nmemb, size_t size);

/*
   // strata 6.25% (1slowo per 16slow) dla PAGE_SIZE ktore sa wielokrotnoscia 16 slow
   drzewo strony:
      * wymaga 1 slowo na 16 slow obslugiwanych
      * ma strukture:
         MLP
         M - bit mowiacy czy L i P sa polaczone
         L i P - lewe i prawe dziecko
            jezeli M jest 1 to pierwszy bit po nim mowi czy slot uzywany
            wpp dzieci sa drzewami
      * drzewa-liscie skladaja sie z 3 bitow
         podobnie MLP ale
         L i P to bity mowiace o zajeciu danego slowa gdy M jest 0 gdy 1 jak wyzej
*/

void *malloc(size_t size){
   if( size == 0 ) return NULL;

   if( !MM_RUNNING ){
      mm_startup();
   }

   if( size > (PAGE_SIZE >> 1) ){
      // to do - give full pages
   }


   to_slot_size(&size);

   void *ptr = find_slot( size >> 2 );

   if( ptr == NULL ){
      // to-do
      // dodanie strony
      // find_slot_page dla nowej stony   
   }

   return ptr;
}

void *realloc(void *ptr, size_t size){
   if( ptr == NULL || (size_t)ptr&0x3 ) return NULL;
   if( size == 0 ) { free(ptr); return NULL; }
   if( size > (PAGE_SIZE >> 1) ){
      // to-do
      return NULL;
   }
   if( !MM_RUNNING ){
      mm_startup();
   }


   to_slot_size(&size);
   size >>= 2;

   // find page
   size_t i = 0;
   while( (ptr < page_adrs[i] || ptr >= (void*)((uint8_t*)(page_adrs[i]) + PAGE_SIZE)) && i < num_pages ) ++i;
   if( i >= num_pages ) return NULL;

   void *tree = page_tree[i];
   void *page = page_adrs[i];
   size_t child_len = PAGE_SIZE >> 2;
   uint32_t offset = (ptr - page) >> 2; 
   uint32_t new_offset = offset;
   i = 0;
   bool split = true;

   // go down - find node or leaf
   while( 1 ){
      child_len >>= 1;

      // leaf?
      if( getbit( tree, i ) ){
         if( ( child_len != 0 && !getbit( tree, i+1 ) ) || offset != 0 ) return NULL; // sth went wrong
         if( size >> 1 == child_len ) return ptr; // leaf ?= node
         break;
      }
      if( child_len == 0 ) return NULL;

      if( child_len == size >> 1 ){ // node?
         split = false;
         break;         
      }


      if( offset < child_len ){ // go left
         // offset = offset
         ++i;
      }
      else{ // go right
         offset -= child_len;
         i += 1 << child_len;
      }

   }


   if( split ){
      // go down - split leaf
      do{
         clearbit( tree, i );
         setbit( tree, i+1 );
         if( !child_len&1 ) setbit( tree, i+(1 << child_len) );

         ++i;
         child_len >>= 1;
      }while( child_len > size );
      if( !child_len ) setbit( tree, i+1 );
      return ptr;
   }
   else{
      uint32_t wentleft = 0;
      bool cpy = offset != 0;
      new_offset -= offset;
      // go down - find leaf - try merge to node
      while(1){

      }
      size_t old_size = 0;

      // go up - clean tree
      while(1){

      }

      if( cpy ) ; // cpymem 

   }



}

void free(void *ptr){
   // find page
   if( ptr == NULL || !MM_RUNNING || (size_t)ptr&0x3 ) return;
   size_t i = 0;
   while( (ptr < page_adrs[i] || ptr >= (void*)((uint8_t*)(page_adrs[i]) + PAGE_SIZE)) && i < num_pages ) ++i;
   if( i >= num_pages ) return;

   void *tree = page_tree[i];
   void *page = page_adrs[i];
   size_t child_len = PAGE_SIZE >> 2;
   uint32_t offset = (ptr - page) >> 2; 
   uint32_t wentleft = 0;
   i = 0;

   // go down - find leaf
   while( 1 ){
      child_len >>= 1;
      wentleft <<= 1;

      if( getbit( tree, i ) ){
         if( ( child_len != 0 && !getbit( tree, i+1 ) ) || offset != 0 ) return;
         clearbit( tree, i+1 );
         break;
      }
      if( child_len == 0 ) return;

      if( offset < child_len ){ // go left
         // offset = offset
         wentleft |= 1;
         ++i;
      }
      else{ // go right
         // wentleft &= ~1
         offset -= child_len;
         i += 1 << child_len;
      }

   }

   size_t cur_len = (child_len) ? (child_len << 1) : (1);

   // go up - merge free leaves
   do{
      // go up
      i -=  ( wentleft & 2 ) ? (1) : (1 << cur_len);
      wentleft >>= 1;
      cur_len <<= 1;

      // if both children merged and free - merge
      // else - return

      if( cur_len > 2){
         if( !getbit( tree, i+1 ) || !getbit( tree, i+(1 << (cur_len>>1)) ) ) return; // one is split
         if( getbit( tree, i+2 ) || getbit( tree, i+1+(1 << (cur_len>>1)) )) return; // one is not free

         // clear children
         clearbit( tree, i+1 );
         clearbit( tree, i+(1 << (cur_len>>1)) );
      }
      else{ // children are single words
         if( getbit( tree, i+1 ) || getbit( tree, i+2 ) ) return; // one is not free
      }

      setbit( tree, i ); // merge children
   }while( i > 0 );
   
}

void *calloc(size_t nmemb, size_t size){
   size *= nmemb;
   void *ptr = malloc( size );

   if(size&0x3) size = size&~0x3 + 4;
   size >>= 2;
   size_t i = 0;
   while( i < size){
      ((uint32_t*)ptr)[i] ^= ((uint32_t*)ptr)[i];
      ++i; 
   }
   
   return ptr;
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

/*
uint8_t MSBidx(uint64_t n){
   uint8_t idx = 0;

   if( n & 0xffffffff00000000 ){ n >>= 32; idx = 32; }
   if( n & 0xffff0000 ){ n >>= 16; idx |= 16; }
   if( n & 0xff00 ){ n >>= 8; idx |= 8; }
   if( n & 0xf0 ){ n >>= 4; idx |= 4; }
   if( n & 0xc ){ n >>= 2; idx |= 2; }
   //if( n & 0x2 ){ n >>= 1; idx |= 1; }
   //idx |= n>>1;
   return idx | n>>1;
}*/

void mm_startup(){
   /*
      tworzy pierwsza strone
      na jej poczatku inicjuje liste drzew stron
      tworzy liste stron 
   */
   PAGE_SIZE = sysconf(_SC_PAGESIZE);
   void *page = mmap(0, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);

   // temporary space for page_tree's and page_adres's
   void *temp_page_tree[1];
   void *temp_page_adrs[1];
   page_tree = temp_page_tree;
   page_adrs = temp_page_adrs;

   page_adrs[0] = page;
   page_tree[0] = page;
   setbit(page_tree[0], 0);
   // creating space for page_tree[0]
   find_slot_page(page_tree[0], page_adrs[0], PAGE_SIZE >> 6); // == page_tree[0]

   // creating space for page_tree
   page_tree = find_slot_page(page_tree[0], page_adrs[0], sizeof(void*));
   page_tree[0] = temp_page_tree[0];

   // creating space for page_adrs
   page_adrs = find_slot_page(page_tree[0], page_adrs[0], sizeof(void*));
   page_adrs[0] = temp_page_adrs[0];

   page_adrs_cap = 1;
   page_adrs[0] = page;
   ++num_pages;

   MM_RUNNING = true;
}


void *find_slot_page(void *tree, void *page, size_t k){
   /*
         Znajduje slot dlugosci k-slow w danej stronie
         aktualizuje drzewo strony
      args
         tree - drzewo danej strony
         page - dana strona
         k    - dlugosc szukanego slotu w slowach, potega 2
      return
         ptr - adress do wolnego slotu o dlugosci k-slow, NULL gdy taki slot nie istnieje
   */
   size_t cur_len = PAGE_SIZE >> 2; // ilosc slow w aktualnym slotcie
   size_t offset = 0; // offset danego slotu
   size_t i = 0; // wskazuje na bit M aktualnego slotu


   // 32bit MAX_PAGE_SIZE = 256 GiB // 64bit MAX_PAGE_SIZE = 1 ZiB = 1024^7 B
   uint32_t wentleft = 0;
   bool wentup = false;


   while( 1 ){
      if( getbit( tree, i )  ){
         if( !getbit( tree, i+1 ) ){
            if( cur_len == k ){
               setbit( tree, i+1 );
               //SUCCESS
               break;
            }
            else { //if( cur_len > k ){
               // go left
               clearbit( tree, i );
               setbit( tree, i );
               setbit( tree, i+(1 << (cur_len>>1)) );

               wentleft |= 1;
               wentleft <<= 1;
               ++i;
               cur_len >>= 1;
               continue;
            }
         }
         else { // go up

            if( wentleft & 2 ){
               --i;
            }
            else {
               i -= (1 << cur_len);
               offset -= cur_len;
            }

            wentleft >>= 1;
            cur_len <<= 1;
            wentup = true;
            continue;
         }
      }
      else {
         if( cur_len == k ){ // go up
            
            if( wentleft & 2 ){
               --i;
            }
            else {
               i -= (1 << cur_len);
               offset -= cur_len;
            }

            wentleft >>= 1;
            cur_len <<= 1;
            wentup = true;
            continue;
         }
         else { //if( cur_len > k ){
            if( !wentup ){ // go left

               ++i;
               wentleft |= 1;
               wentleft <<= 1;
               cur_len >>= 1;
               continue;
            }
            else {
               if( wentleft & 1 ){ // go right

                  cur_len >>= 1;
                  i += 1 << (cur_len/*>>1*/);
                  offset += cur_len/* >> 1*/;
                  wentleft &= ~1;
                  wentleft <<= 1;
                  wentup = false;
                  continue;
               }
               else { // go up

                  if( wentleft & 2 ){
                     --i;
                  }
                  else {
                     i -= (1 << cur_len);
                     offset -= cur_len;
                  }

                  wentleft >>= 1;
                  cur_len <<= 1;
                  // wentup = true;
                  continue;
               }
            }

         }
      }

   }

   /*
      if merged & free
         if len == k
            success
         if len > k
            split
            wentleft true
            go left
      if merged & not free
         wentup true
         go up
      if not merged
         if len == k
            wentup true
            go up
         if len > k
            if !wentup
               wentleft true
               go left
            if wentup and wentleft
               wentleft fasle
               go right
            if wentup and !wentleft
               go up
   */
   return (uint32_t*)page + offset;
}

void *find_slot(size_t k){
/*
      Znajduje slot dlugosci k-slow w zarzadzanych stronach
   args
      k - dlugosc szukanego slotu w slowach, potega 2
   return
      ptr - adress do wolnego slotu o dlugosci k-slow, NULL gdy taki slot nie istnieje
*/
   void *ptr = NULL;
   size_t i = 0;
   do{
      ptr = find_slot_page( ( (uint32_t*)page_tree + (PAGE_SIZE >> 4)*i ), page_adrs[i], k );
      ++i;
   }while( i < num_pages && ptr != NULL );
   return ptr;
}

void add_page(){
   // to-do
}