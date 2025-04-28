// ui.h
#ifndef UI_H
#define UI_H

#include <ncurses.h>
#include <sys/types.h>
#include <stddef.h>
#include"block_io.h"
#include "variables.h"

void init_ui();
void display_sector(const unsigned char *buffer, size_t size, off_t sector_num, int cursor_x, int cursor_y);
void display_error(const char *msg);
void display_message(const char *msg);
void display_help();
void graceful_exit(sector_t *sectors);
void input_string(char *buffer, const char *prompt);
void destroy_sector(sector_t *sector);
sector_t* make_sector();
#endif
