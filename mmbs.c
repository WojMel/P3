#include <unistd.h>
#include <sys/mman.h>
#include <stdint.h>

#define _nbit(i)             (i & 0x1f)
#define _ibits(ptr,i)        (((uint32_t*)ptr)[ i >> 5 ])
#define getbit(ptr, i)       ((_ibits(ptr,i) >> _nbit(i)) & 1)
#define changebit(ptr, i, x) _ibits(ptr,i) ^= (-x ^ _ibits(ptr,i)) & (1 << _nbit(i))
#define setbit(ptr, i)       _ibits(ptr,i) |= 1 << _nbit(i)
#define clearbit(ptr, i)     _ibits(ptr,i) &= ~(1 << _nbit(i))
#define togglebit(ptr, i)    _ibits(ptr,i) ^= 1 << _nbit(i)

bool MM_RUNNING = false;
size_t PAGE_SIZE = sysconf(_SC_PAGESIZE);
size_t num_pages = 0;
void **page_tree = NULL;
size_t page_adrs_cap = 0;
void **page_adrs = NULL;

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
   if( ptr == NULL ) return NULL;
   if( size == 0 ) { free(ptr); return NULL: }
   if( size > (PAGE_SIZE >> 1) ){
      // free malloc cpmem
   }
   if( !MM_RUNNING ){
      mm_startup();
   }
   // usun z drzewa
   // find slot
   // jezeli inny adres cpymem
}

void free(void *ptr){
   // to-do

   size_t i = 0;
   while( (ptr < page_adrs[i] || ptr >= (uint8_t)(page_adrs[i]) + PAGE_SIZE) && i < num_pages ) ++i;
   if( i >= num_pages ) return;

   void *tree = 

   // free slot and upadte tree
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
   /*
      tworzy pierwsza strone
      na jej poczatku inicjuje liste drzew stron
      tworzy liste stron 
   */
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
               setbit( tree, i+(2 << (cur_len>>1)) );

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
               i -= (2 << cur_len);
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
               i -= (2 << cur_len);
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
                  i += 2 << (cur_len/*>>1*/);
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
                     i -= (2 << cur_len);
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
      if marged & free
         if len == k
            success
         if len > k
            split
            wentleft true
            go left
      if marged & not free
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