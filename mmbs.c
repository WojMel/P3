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

bool MM_INIT = false;
size_t PAGE_SIZE = sysconf(_SC_PAGESIZE);
size_t num_pages = 0;
void *page_tree = NULL;
size_t page_adrs_cap = 0;
void **page_adrs = NULL;

/*
   drzewo strony:
      * wymaga 1 slowo na 16 slow obslugiwanych
      * ma strukture:
         MLP
         M - bit mowiacy czy L i P sa polaczone
         L i P - lewe i prawe drzewo-dziecko
      * drzewa-liscie skladaja sie z 3 bitow
         podobnie MLP ale
         L i P to bity mowiace o zajeciu danego slowa
*/

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

void *realloc(void *ptr, size_t size){
   if( ptr == NULL ) return NULL;
   if( size == 0 ) { free(ptr); return NULL: }
   if( size > (PAGE_SIZE >> 1) ){
      // free malloc cpmem
   }
   // usun z drzewa
   // find slot
   // jezeli inny adres cpymem
}

void free(void *ptr){
   // usun z drzewa
   // jesli w pamieci tylko drzewo i tablica stron mm_cleanup
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

   page_tree = page;
   setbit(page_tree, 0);
   find_slot_page(page_tree, page, PAGE_SIZE >> 6);

   page_adrs = find_slot_page(page_tree, page, sizeof(void*));
   page_adrs_cap = 1;
   page_adrs[0] = page;
   ++num_pages;

   MM_INIT = true;
}

void mm_cleanup(){

   // to-do

   MM_INIT = false;
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
   size_t cur_len = PAGE_SIZE << 2;
   size_t offset = 0;
   size_t i = 0;
   
   // to-do
   // obliczenie offset i modyfikacja drzewa

   return page + offset;
}