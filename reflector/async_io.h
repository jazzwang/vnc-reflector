/* VNC Reflector Lib
 * Copyright (C) 2001 Const Kaplinsky
 *
 * $Id: async_io.h,v 1.4 2001/08/02 19:50:23 const Exp $
 * Asynchronous file/socket I/O
 */

#ifndef _REFLIB_ASYNC_IO_H
#define _REFLIB_ASYNC_IO_H

/*
 * Data types
 */

/* Just a pointer to function returning void */
typedef void (*AIO_FUNCPTR)();

/* This structure is used as a part of output queue */
typedef struct _AIO_BLOCK {
  size_t data_size;             /* Data size in this block                 */
  struct _AIO_BLOCK *next;      /* Next block or NULL for the last block   */
  unsigned char data[1];        /* Beginning of the data buffer            */
} AIO_BLOCK;

/* This structure holds the data associated with a file/socket */
typedef struct _AIO_SLOT {
  int type;                     /* To be used by the application to mark   */
                                /*   certain type of file/socket           */
  int fd;                       /* File/socket descriptor                  */

  AIO_FUNCPTR readfunc;         /* Function to process data read,          */
                                /*   to be set with aio_setread()          */
  unsigned char *readbuf;       /* Pointer to an input buffer,             */
                                /*   to be set with aio_setread()          */
  size_t bytes_to_read;         /* Bytes to read into the input buffer     */
                                /*   to be set with aio_setread()          */
  size_t bytes_ready;           /* Bytes ready in the input buffer         */
  unsigned char buf256[256];    /* Built-in input buffer                   */

  AIO_FUNCPTR writefunc;        /* Function called after data is written,  */
                                /*   to be set with aio_write()            */
  AIO_BLOCK *outqueue;          /* First block of the output queue or NULL */
  AIO_BLOCK *outqueue_last;     /* Last block of the output queue or NULL  */
  size_t bytes_written;         /* Number of bytes written from that block */

  AIO_FUNCPTR closefunc;        /* To be called before close, may be NULL  */

  unsigned alloc_f    :1;       /* 1 if buffer has to be freed with free() */
  unsigned close_f    :1;       /* 1 if the slot is about to be closed     */
  unsigned errread_f  :1;       /* 1 if there was a problem reading data   */
  unsigned errwrite_f :1;       /* 1 if there was a problem writing data   */

  struct _AIO_SLOT *next;       /* To make a list of AIO_SLOT structures   */
  struct _AIO_SLOT *prev;       /* To make a list of AIO_SLOT structures   */

} AIO_SLOT;

/*
 * Global variables
 */

extern AIO_SLOT *cur_slot;

/*
 * Public functions
 */

void aio_init(void);
void aio_add_slot(int fd, AIO_FUNCPTR initfunc, int type, size_t slot_size);
int aio_listen(int port, AIO_FUNCPTR initfunc, int type, size_t slot_size);
void aio_close(int fatal);
void aio_mainloop(void);
void aio_setread(AIO_FUNCPTR fn, void *inbuf, int bytes_to_read);
void aio_write(AIO_FUNCPTR fn, void *outbuf, int bytes_to_write);
void aio_setclose(AIO_FUNCPTR closefunc);

#endif /* _REFLIB_ASYNC_IO_H */
