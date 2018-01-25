#include "mmbs.h"
#include <stdio.h>

int main(){
   printf("Prosty interaktywny program testujacy:\n");
   printf("1 - malloc\n2 - free\n3 - realloc\n4 - wypisz strony\n5 - wypisz drzewa stron\n0 - wyjdz\n\n");
   printf("jest 100 wskaznikow (0-99)\n");
   printf("polecenia {3,4,0} nalezy podac bez argumentow\n");
   printf("komendy nalezy podac w kolejnosci:\n-polecenie (1-3)\n-wskaznik ktorego to ma dotyczyc (0-99)\n-argumenty funkcji gdzie zamiast wskaznika nalezy podac jego numer\ndla free nalezy nie podawac argumentow\n");
   printf("\nnp. \tptr#3 = malloc( 10 ) to 1 3 10\n\tfree( ptr#0 ) to 2 0\n\tptr#99 = realloc( ptr#1, 64 ) to 3 99 1 64\n");
   
   void* pointer[100];
   mm_startup();
   putchar('\n');

   int command=1, ptr, arg0, arg1, j;
   while(command!=0){
      scanf("%d",&command);
      switch(command){
         case 0:
            break;
         case 1:
            scanf("%d %d",&ptr,&arg0);
            pointer[ptr] = m_malloc(arg0);
            break;
         case 2:
            scanf("%d",&ptr);
            m_free(pointer[ptr]);
            break;
         case 3:
            scanf("%d %d %d",&ptr,&arg0,&arg1);
            pointer[ptr] = m_realloc(pointer[arg0],arg1);
            break;
         case 4:
            j=0;
            while(mm_print_tree(j++)) putchar('\n');
            break;
         case 5:
            j=0;
            while(mm_print_tree_bin(j++)) putchar('\n');
            break;
      }
   }

   putchar('\n');
   return 0;
}
