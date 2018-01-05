Implementacja zarządcy pamięci w oparciu o system bliźniaków.

Realizacja zadania przez implementację następujących funkcji:
* `void *malloc(size_t size);`
* `void free(void *ptr);`
* `void *calloc(size_t nmemb, size_t size);`
* `void *realloc(void *ptr, size_t size);`
* `void * reallocarray(void *ptr, size_t nmemb, size_t size);`

Praca będzie:
* zgodna ze specyfikacją [POSIX](http://pubs.opengroup.org/onlinepubs/9699919799/)
* implementować system bliźniaków
* wątkowo bezpieczna
* używać `sys/mman.h`, `unistd.h`

Pamięć będzie przydzielana w blokach długości `2^k` lub `PAGE_SIZE*k`, gdzie `k` będzie najmniejszą taką liczbą spełniającą żądanie oraz `PAGE_SIZE` to rozmiar strony pobrany z systemu.
Praca będzie bazować na zarządzaniu stronami. Dla żądania mniejszego niż długość strony przydzielony będzie wolny blok z zarządzanych ston, jeśli taki blok nie istnieje lub nie będzie można takiego utworzyć to blok zostanie przydzielony z nowej strony. Dla żądań niemniejszych niż długość strony przydzielona będzie minimalna ilość stron spełniająca żądanie.

