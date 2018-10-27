#ifndef MTFIO
#define MTFIO

#include <pthread.h>
#include <limits.h>
#include <linux/limits.h>

typedef void *mtfio_handle_p;

/* functions will return 0 on success, 1 on failure, and error will be output to stderr */
mtfio_handle_p mtfio_open (char* path);
void mtfio_close (mtfio_handle_p handle);
int mtfread (void *dest, size_t size_memb, size_t nmemb, long int offset, mtfio_handle_p handle);
int mtfwrite (void *src, size_t size_memb, size_t nmemb, long int offset, mtfio_handle_p handle); 
int mtfappend (void *src, size_t size_memb, size_t nmemb, mtfio_handle_p handle); 
/* 
 * mtflock and mtfunlock can only be used with mtflockread, mtflockwrite, and mtflockappend
 * otherwise, the program will never write, or undefined behaviour may occur.
 */
int mtflock (mtfio_handle_p handle);
int mtfunlock (mtfio_handle_p handle);
int mtflockread (void *dest, size_t size_memb, size_t nmemb, long int ofset, mtfio_handle_p handle);
int mtflockwrite (void *src, size_t size_memb, size_t nmemb, long int ofset, mtfio_handle_p handle);
int mtflockappend (void *src, size_t size_memb, size_t nmemb, mtfio_handle_p handle);



#endif
