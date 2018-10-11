#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdbool.h>
#include "mtfio.h"
#include "mtfio_internal.h"

#define DEBUG 0

static struct mtfio_handle **mtfio_handle_db;
static int num_handle_alloc = 0;
static int num_handle_used = 0;
static pthread_mutex_t oc_mutex = PTHREAD_MUTEX_INITIALIZER;

mtfio_handle_p mtfio_open (char* path) {
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
    struct mtfio_handle *ret_pointer = mtfio_handle_db[current_handle];
    pthread_mutex_unlock (&oc_mutex);
    return (ret_pointer);
}

void mtfio_close (mtfio_handle_p _handle) {
    pthread_mutex_lock (&oc_mutex);

    struct mtfio_handle *handle = (struct mtfio_handle*)_handle;
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

    pthread_mutex_unlock(&handle->mutex);
    return(0);
}

int mtfappend (void *src, size_t size_memb, size_t nmemb, mtfio_handle_p _handle) {
    struct mtfio_handle *handle = (struct mtfio_handle*)_handle;
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

    pthread_mutex_unlock(&handle->mutex);
    return (0);
}
