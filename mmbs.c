#include "mmbs.h"
#include <unistd.h>
#include <sys/mman.h>
#include <stdbool.h>
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

void mm_startup();
void *find_slot_page(void *tree, void *page, size_t k);
void *find_slot(size_t k);
void merge_leaves(void *tree, size_t i, size_t wentleft, size_t cur_len);
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
      bool merge = true;
      //size_t cur_len = child_len << 1;
      uint32_t wentleft = 0;
      bool cpy = offset != 0;
      new_offset -= offset;

      // go down - find leaf - try merge to node
      while( 1 ){

         if( offset < child_len ){ // go left
            // is R m_free?
            if( (child_len == 1 && getbit( tree, i+2 )) || (child_len > 1 && (!getbit( tree, i+(1<<child_len)) || getbit( tree, i+(1<<child_len)+1 )) ))
               { merge = false; break; } // (L=1 ^ Fword) v ( L>1 ^ ( not M v ( M ^ not F )  ) )
            
            wentleft |= 1;
            ++i;
         }
         else{ // go right
            if( (child_len == 1 && getbit( tree, i+1 )) || (child_len > 1 && (!getbit(tree, i+1) || getbit(tree, i+2)) )) // L is not m_free
               { merge = false; break; }

            offset -= child_len;
            i += 1 << child_len;
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
         
         merge_leaves(tree, i, wentleft, child_len);

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
   ///////////printf("free starts\noffset = %u\n",offset);

   // go down - find leaf
   while( 1 ){
      ///////////printf("+offset = %u\tlen = %lu\n",offset,child_len);
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

      ///////////printf("<offset = %u\tlen = %lu\n",offset,child_len);
      if( offset < child_len ){ // go left
         // offset = offset
         wentleft |= 1;
         ++i;
      }
      else{ // go right
         // wentleft &= ~1
         offset -= child_len;
         i += (child_len) ? (child_len << 1) : (1);
      }
      ///////printf("-offset = %u\tlen = %lu\n",offset,child_len);

   }

   //size_t cur_len = (child_len) ? (child_len << 1) : (1);
      /////printf("offset = %u\tlen = %lu\n",offset,cur_len);

   merge_leaves(tree, i, wentleft, child_len << 1);
/*
   // go up - merge m_free leaves
   do{
      // go up
      i -=  ( wentleft & 2 ) ? (1) : (1 << cur_len);
      wentleft >>= 1;
      cur_len <<= 1;

      // if both children merged and m_free - merge
      // else - return

      if( cur_len > 2){
         if( !getbit( tree, i+1 ) || !getbit( tree, i+(1 << (cur_len>>1)) ) ) return; // one is split
         if( getbit( tree, i+2 ) || getbit( tree, i+1+(1 << (cur_len>>1)) )) return; // one is not m_free

         // clear children
         clearbit( tree, i+1 );
         clearbit( tree, i+(1 << (cur_len>>1)) );
      }
      else{ // children are single words
         if( getbit( tree, i+1 ) || getbit( tree, i+2 ) ) return; // one is not m_free
      }

      setbit( tree, i ); // merge children
   }while( i > 0 );
*/
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

void merge_leaves(void *tree, size_t i, size_t wentleft, size_t cur_len){
   /*
      Starts at m_free leaf and goes up merging free children
      args
         tree     - tree pointer 
         i        - starting positon
         wentleft - 
         cur_len  -
   */
   int j;
   printf("\nmerge:\n");
   do{
   
   for(j=0;j<PAGE_SIZE>>1;++j)
      putchar('0'+getbit(page_adrs[0],j));
   putchar('\n');
   print_tree(0);
      // go up
      wentleft >>= 1;
      cur_len <<= 1;
      i -=  ( wentleft & 1 ) ? 1 : cur_len;

      // if both children merged and m_free - merge
      // else - return

         printf("\n0 len=%lu\t i=%lu\n",cur_len,i);
         for(j=i;j<PAGE_SIZE>>1;++j)
            putchar('0'+getbit(page_adrs[0],j));
         putchar('\n');
      if( cur_len > 2){
         printf("3\n");
         if( !getbit( tree, i+1 ) || !getbit( tree, i+cur_len ) ) break; // one is split
         printf("4\n");
         if( getbit( tree, i+2 ) || getbit( tree, i+1+cur_len ) ) break; // one is not m_free
         printf("5\n");

         // clear children
         clearbit( tree, i+1 );
         clearbit( tree, i+cur_len );
      }
      else{ // children are single words
         printf("1\n");
         if( getbit( tree, i+1 ) || getbit( tree, i+2 ) ) break; // one is not m_free
         printf("2\n");
      }

      setbit( tree, i ); // merge children
   }while( i > 0 );
   
   for(j=0;j<PAGE_SIZE>>1;++j)
      putchar('0'+getbit(page_adrs[0],j));
   putchar('\n');
   printf("^^^^^^^^^^^^\n");
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
      ptr = find_slot_page( ( (uint32_t*)page_tree + (PAGE_SIZE >> 4)*i ), page_adrs[i], k );
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

void print_tree(size_t i){
   if( i >= num_pages){ printf("Tree #%llu not found!\n", (unsigned long long)i); return; }
   /*
      wypisuje i-ta trone jako ciag slow oznaczonych:
         '#' - zajete
         '-' - wolne
   */
   _print_tree( page_tree[i]);
}


int main(){
   printf("Program testujacy:\n");
   mm_startup();
   printf("PAGE_SIZE = %lu bytes = %lu words\nsize of page tree: %lu words\n",PAGE_SIZE,PAGE_SIZE>>2,PAGE_SIZE>>6);
   void *page = page_adrs[0];
   _print_tree(page);
   putchar('\n');
   
   void *p = find_slot_page(page,page,8);

   _print_tree(page);
   putchar('\n');
   putchar('\n');

   
   int j;
   for(j=0;j<PAGE_SIZE>>1;++j)
      putchar('0'+getbit(page,j));
   putchar('\n');

   m_free(p);
   _print_tree(page);
   putchar('\n');
   
   
   p = find_slot_page(page,page,1);
   void *p2 = find_slot_page(page,page,1);
   void *p3 = find_slot_page(page,page,1);

   //m_free(p);
   _print_tree(page);
   // putchar('\n');



   printf("\npsuje sie:\n");
    m_free(p3);
    _print_tree(page);
    putchar('\n');

   m_free(p2);
   _print_tree(page);
   putchar('\n');

   for(j=0;j<PAGE_SIZE>>1;++j)
      putchar('0'+getbit(page,j));
   putchar('\n');

   
   // m_free();
   // setbit(page,4);
   // clearbit(page,5);
   // merge_leaves();

   // for(j=0;j<PAGE_SIZE>>1;++j)
   //    putchar('0'+getbit(page,j));
   // putchar('\n');

   return 0;
}
