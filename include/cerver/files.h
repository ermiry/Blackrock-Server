#ifndef _CERVER_FILES_H_
#define _CERVER_FILES_H_

#include <stdio.h>

#include <sys/stat.h>

// opens a file and returns it as a FILE
extern FILE *file_open_as_file (const char *filename, 
    const char *modes, struct stat *filestatus);

// opens and reads a file into a buffer
// sets file size to the amount of bytes read
extern char *file_read (const char *filename, int *file_size);

// opens a file
// returns fd on success, -1 on error
extern int file_open_as_fd (const char *filename, struct stat *filestatus);

// sends a file to the sock fd
// returns 0 on success, 1 on error
extern int file_send (const char *filename, int sock_fd);

#endif