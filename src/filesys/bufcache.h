#ifndef FILESYS_BUFCACHE_H
#define FILESYS_BUFCACHE_H

#include "devices/block.h"

#define MAX_BUFCACHE_SIZE 64

void init_bufcache();

void read_sector(void* dest, block_sector_t sector);
void write_sector(void* src, block_sector_t sector);

#endif
