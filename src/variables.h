#ifndef VAR_H
#define VAR_H
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <ncurses.h>
#include <stdbool.h> 
#include <string.h> 
#include <sys/stat.h>
#include <ctype.h>
#define SECTOR_SIZE 512
#define MAX_ROW 32
#define MAX_COL 16
#define MAX_INPUT_LEN 128
#define MAX_SECTOR 2048
#define MAX_MATCHES 100
extern int matches[MAX_MATCHES];       // Массив для хранения индексов совпадений
extern int match_count;
extern int display_mode;
typedef struct {
    int fd;
    unsigned char *buffer; // теперь указатель на динамический массив
    size_t buffer_size;    // чтобы знать размер буфера
    off_t sector;
    int cursor_x;
    int cursor_y;
} sector_t;
#endif