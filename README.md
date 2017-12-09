Implementacja systemu zarządzania pamięcią w oparciu o algorytm bliźniaków.

Realizacja zadania przez implementację następujących funkcji:
* `void *malloc(size_t size);`
* `void free(void *ptr);`
* `void *calloc(size_t nmemb, size_t size);`
* `void *realloc(void *ptr, size_t size);`
* `void * reallocarray(void *ptr, size_t nmemb, size_t size);`

Praca będzie:
* zgodna ze specyfikacją [POSIX](http://pubs.opengroup.org/onlinepubs/9699919799/)
* implementować algorytm Buddy
* używać `sys/mman.h`