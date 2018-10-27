#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdbool.h>
#include "mtfio.h"
#include "mtfio_internal.h"

#define DEBUG 1

static struct mtfio_handle **mtfio_handle_db;
static int num_handle_alloc = 0;
static int num_handle_used = 0;
static pthread_mutex_t oc_mutex = PTHREAD_MUTEX_INITIALIZER;

mtfio_handle_p mtfio_open (char* path) {
        /* 
         * returns pointer to mtfio_handle for the file specified.
         */
        pthread_mutex_lock (&oc_mutex);

        if (!mtfio_handle_db) {
                if (DEBUG == 1)
                        printf ("allocating memory for mtfio_handle_db\n");
                mtfio_handle_db = malloc (5 * sizeof (void*));
                if (!mtfio_handle_db) {
                        fprintf (stderr, "could not allocate memory for mtfio_handle_db\n");
                        exit (1);
                }
                num_handle_alloc = 5;
        }
        if (num_handle_used == num_handle_alloc) {
                if (DEBUG == 1)
                        printf ("reallocating memory for mtfio_handle_db\n");
                mtfio_handle_db = realloc (mtfio_handle_db, (sizeof (void*) * (num_handle_alloc + 5)));
                if (!mtfio_handle_db) {
                        fprintf( stderr, "could not reallocate memory for mtfio_handle_db\n");
                        exit (1);
                }
                num_handle_alloc = num_handle_alloc + 5;
        }
        if (num_handle_used < (num_handle_alloc - 6)) {
                if (DEBUG == 1)
                        printf ("deallocating memory for mtfio_handle_db\n");
                mtfio_handle_db = realloc (mtfio_handle_db, (sizeof (void*) * (num_handle_alloc - 5)));
                if (!mtfio_handle_db) {
                        fprintf (stderr, "could not deallocate memory for mtfio_handle_db\n");
                }
                num_handle_alloc = num_handle_alloc - 5;
        }
        int current_handle = -1;
        for (int i = 0; i < num_handle_used; i++) {
                if (strcmp (mtfio_handle_db[i]->path, path) == 0) {
                        if (DEBUG == 1)
                                printf("current_handle is now %d\n", i);
                        current_handle = i;
                }
        }
        if (current_handle == -1) {
                if (DEBUG == 1) 
                        printf("current handle was not found\n");
                current_handle = num_handle_used;
                if (DEBUG == 1)
                        printf("current handle is now %d\n", current_handle);
                num_handle_used++;
                if (DEBUG == 1)
                        printf("memory is now being allocated for current handle\n");
                mtfio_handle_db[current_handle] = malloc (sizeof (struct mtfio_handle));
                if (DEBUG == 1)
                        printf("mutex is now being initialized\n");
                pthread_mutex_init (&mtfio_handle_db[current_handle]->mutex, NULL);
                strncpy (mtfio_handle_db[current_handle]->path, path, strlen (path) + 1);
        }
        mtfio_handle_db[current_handle]->num_threads++;
        mtfio_handle_db[current_handle]->locked = 0;
        struct mtfio_handle *ret_pointer = mtfio_handle_db[current_handle];
        pthread_mutex_unlock (&oc_mutex);
        return ((void*)ret_pointer);
}

void mtfio_close (mtfio_handle_p _handle) {
        /* 
         * decrements _handle->num_threads, and if it's under 1, it frees 
         * the memory previously occupied by the handle
         */
        struct mtfio_handle *handle = (struct mtfio_handle*)_handle;
        if (handle->locked == 1) {
                fprintf (stderr, "mtfio handle locked with mtflock()\n");
                return;
        }
        pthread_mutex_lock (&oc_mutex);

        int done = 0;
        // scan for matching pointer
        for (int i = 0; i < num_handle_used && done != 1; i++) {
                // if handle matches
                if (handle == mtfio_handle_db[i]) {
                        // lower thread count
                        mtfio_handle_db[i]->num_threads--;
                        // done searching
                        done == 1;
                        // if it's 0 or lower (meaning the program screwed up before)
                        if (mtfio_handle_db[i]->num_threads < 1) {
                                free (mtfio_handle_db[i]);
                                // delete the pointer from the array
                                for (int i2 = i; i2 < num_handle_used - 1; i2++) {
                                        mtfio_handle_db[i2] = mtfio_handle_db[i2 + 1];
                                }
                                num_handle_used--;
                        }
                }
        }
        pthread_mutex_unlock (&oc_mutex);
}

int mtfread (void *dest, size_t size_memb, size_t nmemb, long int offset, mtfio_handle_p _handle) {
        struct mtfio_handle *handle = (struct mtfio_handle*)_handle;
        if (handle->locked == 1) {
                fprintf (stderr, "mtfio handle locked with mtflock()\n");
                return (1);
        }
        pthread_mutex_lock (&(handle->mutex));
        FILE *file = fopen (handle->path, "r");
        if (!file) {
                fprintf (stderr, "could not open file at %s\n", handle->path);
                pthread_mutex_unlock(&handle->mutex);
                return (1);
        }

        if (fseek (file, offset, SEEK_SET) != 0) {
                fprintf (stderr, "could not seek in file at %s\n", handle->path);
                pthread_mutex_unlock(&handle->mutex);
                return (1);
        }

        size_t read_quant = fread (dest, size_memb, nmemb, file);
        if (read_quant != nmemb) {
                fprintf (stderr, "could not read %zu members from file %s, only %zu were read\n", nmemb, handle->path, read_quant); 
                pthread_mutex_unlock(&handle->mutex);
                return (1);
        }

        if (fclose (file) != 0) {
                fprintf (stderr, "could not close file %s\n", handle->path);
                pthread_mutex_unlock(&handle->mutex);
                return (1);
        }

        pthread_mutex_unlock(&handle->mutex);
        return(0);
}

int mtfwrite (void *src, size_t size_memb, size_t nmemb, long int offset, mtfio_handle_p _handle) {
        struct mtfio_handle *handle = (struct mtfio_handle*)_handle;
        if (handle->locked == 1) {
                fprintf (stderr, "mtfio handle locked with mtflock()\n");
                return (1);
        }
                
        pthread_mutex_lock(&handle->mutex);
        FILE *file = fopen (handle->path, "w");
        if (!file) {
                fprintf (stderr, "could not open file at %s\n", handle->path);
                pthread_mutex_unlock (&handle->mutex);
                return (1);
        }

        if (fseek (file, offset, SEEK_SET) != 0) {
                fprintf (stderr, "could not seek in file at %s\n", handle->path);
                pthread_mutex_unlock(&handle->mutex);
                return (1);
        }
        
        size_t write_quant = fwrite (src, size_memb, nmemb, file);
        if (write_quant != nmemb) {
                fprintf (stderr, "could not write %zu elements to %s file, only wrote %zu elements\n", nmemb, handle->path, write_quant);
                pthread_mutex_unlock(&handle->mutex);
                return (1);
        }

        if (fclose (file) != 0) {
                fprintf (stderr, "Could not close file\n");
                return (1);
        }


        pthread_mutex_unlock(&handle->mutex);
        return(0);
}

int mtfappend (void *src, size_t size_memb, size_t nmemb, mtfio_handle_p _handle) {
        struct mtfio_handle *handle = (struct mtfio_handle*)_handle;
        if (handle->locked == 1) {
                fprintf (stderr, "mtfio handle locked with mtflock()\n");
                return (1);
        }

        pthread_mutex_lock(&(handle->mutex));
        FILE *file = fopen (handle->path, "a");
        if (!file) {
                fprintf (stderr,"could not open file at %s\n", handle->path);
                pthread_mutex_unlock (&handle->mutex);
                return (1);
        }

        size_t write_quant = fwrite (src, size_memb, nmemb, file);
        if (write_quant != nmemb) {
                fprintf (stderr, "could not write %zu elements to %s file, only wrote %zu elements\n", nmemb, handle->path, write_quant);
                pthread_mutex_unlock (&handle->mutex);
                return (1);
        }
        if (fclose (file) != 0) {
                fprintf (stderr, "Could not close file\n");
                return (1);
        }
        pthread_mutex_unlock(&handle->mutex);
        return (0);
}

int mtflock (mtfio_handle_p _handle) {
        /*
         * mtflock will lock the file pointed to by the handle, thus supporting multiple
         * read and writes without the danger of being preempted by another thread writing 
         * to the same file. Cannot use mtfread, mtfwrite, mtfappend or mtfio_close while locked.
         */
        struct mtfio_handle *handle = (struct mtfio_handle*)_handle;
        pthread_mutex_lock (&oc_mutex);
        if (DEBUG == 1) 
                printf ("about to lock mutex\n");
        pthread_mutex_lock (&(handle->mutex));

        handle->locked = 1;
        pthread_mutex_unlock (&oc_mutex);
        return 0;
}

int mtfunlock (mtfio_handle_p _handle) {
        /*
         * mtfunlock will unlock file at handle to allow typical mtfio 
         * operations to resume. Must be done before closing file.
         */
        struct mtfio_handle *handle = (struct mtfio_handle*)_handle;
        if (handle->locked != 1) {
                fprintf (stderr, "mtfio handle not locked with mtflock(), cannot unlock it\n");
                return 1;
        }
        handle->locked = 0;
        pthread_mutex_unlock (&(handle->mutex));
        return 0;
}

int mtflockread (void *dest, size_t size_memb, size_t nmemb, long int offset, mtfio_handle_p _handle) {
        /*
         * Handle must be locked with mtflock before using!
         * mtflockread will read nmemb elements of size size_memb from offset in file
         * and place them into dest. Dest must be large enough to fit elements
         * (obviously). Returns 1 on error and prints message to stderr.
         */
        struct mtfio_handle *handle = (struct mtfio_handle*)_handle;
        if (handle->locked != 1) {
                fprintf (stderr, "mtfio handle not locked with mtflock()\n");
                return (1);
        }
        FILE *file = fopen (handle->path, "r");
        if (!file) {
                fprintf (stderr, "could not open file at %s\n", handle->path);
                return (1);
        }

        if (fseek (file, offset, SEEK_SET) != 0) {
                fprintf (stderr, "could not seek in file at %s\n", handle->path);
                return (1);
        }

        size_t read_quant = fread (dest, size_memb, nmemb, file);
        if (read_quant != nmemb) {
                fprintf (stderr, "could not read %zu members from file %s, only %zu were read\n", nmemb, handle->path, read_quant); 
                return (1);
        }

        if (fclose (file) != 0) {
                fprintf (stderr, "could not close file %s\n", handle->path);
                return (1);
        }

        return(0);
}

int mtflockwrite (void *src, size_t size_memb, size_t nmemb, long int offset, mtfio_handle_p _handle) {
        /*
         * Handle must be locked with mtflock before using!
         * mtflockwrite will write nmemb members of size size_memb to offset in file
         * from src pointer. Returns 1 on error and prints message to stderr.
         */
        struct mtfio_handle *handle = (struct mtfio_handle*)_handle;
        if (handle->locked != 1) {
                fprintf (stderr, "mtfio handle not locked with mtflock()\n");
                return (1);
        }
                
        FILE *file = fopen (handle->path, "w");
        if (!file) {
                fprintf (stderr, "could not open file at %s\n", handle->path);
                return (1);
        }

        if (fseek (file, offset, SEEK_SET) != 0) {
                fprintf (stderr, "could not seek in file at %s\n", handle->path);
                return (1);
        }

        size_t write_quant = fwrite (src, size_memb, nmemb, file);
        if (write_quant != nmemb) {
                fprintf (stderr, "could not write %zu elements to %s file, only wrote %zu elements\n", nmemb, handle->path, write_quant);
                return (1);
        }

        if (fclose (file) != 0) {
                fprintf (stderr, "Could not close file\n");
                return (1);
        }
        return(0);
}
int mtflockappend (void *src, size_t size_memb, size_t nmemb, mtfio_handle_p _handle) {
        /*
         * Handle must be locked with mtflock before using!
         * mtflockappend will nmemb members of size size_memb to file
         * from src pointer. Returns 1 on error, and prints message to stderr.
         */
        struct mtfio_handle *handle = (struct mtfio_handle*)_handle;
        if (handle->locked != 1) {
                fprintf (stderr, "mtfio handle not locked with mtflock()\n");
                return (1);
        }

        FILE *file = fopen (handle->path, "a");
        if (DEBUG == 1)
                printf ("Opened file with mtflockappend\n");
        if (!file) {
                fprintf (stderr,"could not open file at %s\n", handle->path);
                return (1);
        }
        if (DEBUG == 1)
                printf ("Supposedly wrote to file\n");

        size_t write_quant = fwrite (src, size_memb, nmemb, file);
        if (write_quant != nmemb) {
                fprintf (stderr, "could not write %zu elements to %s file, only wrote %zu elements\n", nmemb, handle->path, write_quant);
                return (1);
        }
        if (fclose (file) != 0) {
                fprintf (stderr, "Could not close file\n");
                return (1);
        }
        return (0);
}
