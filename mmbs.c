#include "mmbs.h"
#include <unistd.h>
#include <sys/mman.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#define _nbit(i)             ((i) & 0x1f)
#define _ibits(ptr,i)        (((uint32_t*)(ptr))[ (i) >> 5 ])
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

void *find_slot_page(void *tree, void *page, size_t k);
void *find_slot(size_t k);
size_t merge_leaves(void *tree, size_t i, size_t wentleft, size_t cur_len, uint32_t steps);
static inline void to_slot_size(size_t *size);

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

void *m_malloc(size_t size){
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

void *m_realloc(void *ptr, size_t size){
   if( ptr == NULL || (size_t)ptr&0x3 ) return NULL;
   if( size == 0 ) { m_free(ptr); return NULL; }
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
   size_t cur_len = PAGE_SIZE >> 2;
   uint32_t offset = (ptr - page) >> 2; 
   uint32_t new_offset = offset;
   i = 0;
   bool split = true;

   // go down - find node or leaf
   while( 1 ){

      // leaf?
      if( getbit( tree, i ) ){
         if( ( cur_len != 1 && !getbit( tree, i+1 ) ) || offset != 0 ) return NULL; // sth went wrong
         if( size == cur_len ) return ptr; // leaf ?= node
         break;
      }
      if( cur_len == 1 ) return NULL;

      if( cur_len == size ){ // node?
         split = false;
         break;         
      }


      if( offset < (cur_len>>1)){ // go left
         ++i;
      }
      else{ // go right
         offset -= (cur_len>>1);
         i += cur_len;
      }

      cur_len >>= 1;
   }


   size_t child_len = cur_len >> 1;
   if( split ){
      // go down - split leaf
      do{
         clearbit( tree, i );
         setbit( tree, i+1 );
         if(cur_len>2) setbit( tree, i+cur_len );

         cur_len >>= 1;
         ++i;
      }while( cur_len > size );
      
      if( cur_len>1 ) setbit( tree, i+1 );
      return ptr;
   }
   else{

      bool merge = true;
      //size_t cur_len = child_len << 1;
      uint32_t wentleft = 0;
      bool cpy = offset != 0;
      new_offset -= offset;
      uint32_t steps = 0;

      // go down - find leaf - try merge to node
      while( 1 ){
         steps ++;

         if( offset < child_len ){ // go left
            // is R m_free?
            if( (child_len == 1 && getbit( tree, i+2 )) || (child_len > 1 && (!getbit( tree, i+(child_len<<1) ) || getbit( tree, i+(child_len<<1)+1 )) ))
               { merge = false; break; } // (L=1 ^ Fword) v ( L>1 ^ ( not M v ( M ^ not F )  ) )
            
            wentleft |= 1;
            ++i;
         }
         else{ // go right
            if( (child_len == 1 && getbit( tree, i+1 )) || (child_len > 1 && (!getbit(tree, i+1) || getbit(tree, i+2)) )) // L is not m_free
               { merge = false; break; }

            offset -= child_len;
            i += child_len;
         }

         
         // child leaf?
         if( getbit( tree, i ) ){
            if( ( child_len != 1 && !getbit( tree, i+1 ) ) || offset != 0 ) return NULL; // sth went wrong
            clearbit( tree, i+1 );
            break;
         }
         if( child_len == 1 ) return NULL;
         
         child_len >>= 1;
         wentleft <<= 1;
      }
      void *new_ptr;
      size_t old_size = child_len;

      if( merge ){
         
         i = merge_leaves(tree, i, wentleft, child_len, steps);
         setbit( tree, i+1 );

         new_ptr = (uint32_t*)page + new_offset;

         if( cpy ) return memmove(new_ptr, ptr, old_size<<2);
         return new_ptr;

      }


      m_free(ptr);
      new_ptr = m_malloc(size<<2);
      if( new_ptr == NULL ) return NULL;
      return memmove(new_ptr, ptr, old_size<<2);
   }

}

void m_free(void *ptr){
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

      if( getbit( tree, i ) ){ // merged / not free word
         if( ( child_len != 0 && !getbit( tree, i+1 ) ) || offset != 0 ) return;
         if( child_len ) clearbit( tree, i+1 ); // if not word free
         else{ // word
            clearbit( tree, i );
            if( (wentleft & 2) && !getbit( tree, i+1 ) ){ --i; setbit( tree, i ); wentleft >>= 1; }
            else if( !(wentleft & 2) && !getbit( tree, i-1 ) ){ i-=2; setbit( tree, i); wentleft >>= 1; }
            child_len = 1;
         }
         break;
      }
      if( child_len == 0 ) return;

      if( offset < child_len ){ // go left
         wentleft |= 1;
         ++i;
      }
      else{ // go right
         // wentleft &= ~1
         offset -= child_len;
         i += (child_len) ? (child_len << 1) : (1);
      }

   }

   //size_t cur_len = (child_len) ? (child_len << 1) : (1);
   merge_leaves(tree, i, wentleft, child_len << 1, 0);
}

void *m_calloc(size_t nmemb, size_t size){
   size *= nmemb;
   void *ptr = m_malloc( size );

   if(size&0x3) size = (size&~0x3) + 4;
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

/*uint8_t MSBidx(uint64_t n){
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
   PAGE_SIZE = sysconf(_SC_PAGESIZE) >>5;
   void *page = mmap(0, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
   // temporary space for page_tree and page_adr
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
   page_tree = find_slot_page(page_tree[0], page_adrs[0], sizeof(void*)>>2);
   page_tree[0] = temp_page_tree[0];

   // creating space for page_adrs
   page_adrs = find_slot_page(page_tree[0], page_adrs[0], sizeof(void*)>>2);
   page_adrs[0] = temp_page_adrs[0];

   page_adrs_cap = 1;
   //page_adrs[0] = page;
   ++num_pages;

   MM_RUNNING = true;
}

size_t merge_leaves(void *tree, size_t i, size_t wentleft, size_t cur_len, uint32_t steps){
   /*
      Starts at m_free leaf and goes up merging free children
      args
         tree     - tree pointer 
         i        - starting positon
         wentleft - 
         cur_len  -
         steps    - 0==inf
   */
   bool inf = steps == 0;
   do{
      // go up
      wentleft >>= 1;
      cur_len <<= 1;
      i -=  ( wentleft & 1 ) ? 1 : cur_len;

      // if both children merged and m_free - merge
      // else - return

      if( cur_len > 2){
         if( !getbit( tree, i+1 ) || !getbit( tree, i+cur_len ) ) break; // one is split
         if( getbit( tree, i+2 ) || getbit( tree, i+1+cur_len ) ) break; // one is not m_free

         // clear children
         clearbit( tree, i+1 );
         clearbit( tree, i+cur_len );
      }
      else{ // children are single words
         if( getbit( tree, i+1 ) || getbit( tree, i+2 ) ) break; // one is not m_free
      }

      setbit( tree, i ); // merge children
      if(!inf) --steps;
   }while( i > 0 && (steps || inf ));
   return i;
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
   uint32_t wentleft = 0;
   bool wentup = false;

   while( 1 ){
      
      if( cur_len==1 || getbit( tree, i ) ){ //merged or word
         if( (cur_len==1 && !getbit( tree, i )) || (cur_len!=1 && !getbit( tree, i+1 )) ){ //free
            
            if( cur_len > k )
               clearbit( tree, i ); //begin split
            while( cur_len > k ){
               if(cur_len>2) setbit( tree, i+cur_len ); //set P merged

               // go left
               ++i;
               cur_len >>= 1;
            }

            // set not free
            if( cur_len!=1 ) setbit( tree, i+1 );
            setbit( tree, i );
            //SUCCESS
            
            break;
         }
         
         // not free -> go up
         wentleft >>= 1;
         cur_len <<= 1;
         wentup = true;
         if( wentleft & 1 ){
            --i;
         }
         else {
            i -= cur_len;
            offset -= cur_len>>1;
         }

         continue;
      }
      
      if( cur_len == k || (wentup && !(wentleft&1)) ){ // go up
         wentleft >>= 1;
         cur_len <<= 1;
         wentup = true;
         
         if( wentleft & 1 ){
            --i;
         }
         else {
            i -= cur_len;
            offset -= cur_len>>1;
         }

         continue;
      }

      if( !wentup ){ // go left
         ++i;
         wentleft |= 1;
         wentleft <<= 1;
         cur_len >>= 1;
         continue;
      }
      
      // go right
      i += cur_len;
      cur_len >>= 1;
      offset += cur_len;
      wentleft &= ~1;
      wentleft <<= 1;
      wentup = false;
      continue;
   }

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
      ptr = find_slot_page( *((void**)( (uint32_t*)page_tree + (PAGE_SIZE >> 6)*i )), page_adrs[i], k );
      ++i;
   }while( i < num_pages && ptr != NULL );
   return ptr;
}

const char *sym = "-#";
void _print_tree(void *tree){

   size_t cur_len = PAGE_SIZE >> 2; // ilosc slow w aktualnym slotcie
   size_t i = 0; // wskazuje na bit M aktualnego slotu
   uint32_t wentleft = 0;
   bool wentup = false;
   bool leaf = false;
   char c;

   while( cur_len <= PAGE_SIZE>>2 ){
      if( !wentup ){
         if( cur_len==1 ){ leaf = true; putchar(sym[getbit( tree, i )]); } // word
         else if( getbit( tree, i ) ){ // merged
            leaf = true;
            c = sym[getbit( tree, i+1 )]; 
            size_t j = cur_len;
            while( j ){
               putchar(c);
               --j;
            }
         }
      }

      if(leaf){
         //go up
         cur_len <<= 1;
         wentleft >>= 1;
         i -= ( wentleft & 1 ) ? 1 : cur_len;
         wentup = true;
         leaf = false;
      }
      else{
         if( wentup ){
            if( wentleft & 1 ){
               // go right
               wentleft &= ~1;
               wentleft <<= 1;
               i += cur_len;
               cur_len >>= 1;
               wentup = false;
            }
            else{
               // go up
               cur_len <<= 1;
               wentleft >>= 1;
               i -= ( wentleft & 1 ) ? 1 : cur_len;
            }
         }
         else{
            //go left
            wentleft |= 1;
            wentleft <<= 1;
            cur_len >>= 1;
            ++i;
         }
      }
   }
}

bool mm_print_tree_bin(size_t i){
   if( i >= num_pages) return false;//{ printf("Tree #%llu not found!\n", (unsigned long long)i); return; }
   printf("PAGE#%lu\n",i);
   void *tree = page_tree[i];
   int j;
   for(j=0;j<PAGE_SIZE>>1;++j)
      putchar('0'+getbit(tree,j));
   return true;
}

bool mm_print_tree(size_t i){
   if( i >= num_pages) return false;//{ printf("Tree #%llu not found!\n", (unsigned long long)i); return; }
   /*
      wypisuje i-ta trone jako ciag slow oznaczonych:
         '#' - zajete
         '-' - wolne
   */
   printf("PAGE#%lu\n",i);
   _print_tree( page_tree[i]);
   return true;
}
