#ifndef BLOCK_IO_H
#define BLOCK_IO_H

#include <unistd.h>
#include "variables.h"

int open_device(const char *path);
int read_sector(sector_t *s);
int write_sector(sector_t *s); 
void close_device(int fd);

#endif
