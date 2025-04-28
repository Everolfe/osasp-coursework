#include "block_io.h"
#include "ui.h"
#include"variables.h"
#include"stack.h"

void edit_byte(sector_t* sectors);
void replace_string_in_sector(unsigned char *buffer, const char *search_str, const char *replace_str, bool is_hex);
int search_string_in_sector(unsigned char *buffer, const char *search_str, int *matches, bool is_hex);
int search_bytes_in_sector(unsigned char *buffer, unsigned char *search_bytes, int search_len);
void replace_string_at_cursor(sector_t *sectors, const char *input_str, bool is_hex);
void delete_bytes_from_cursor(sector_t *sectors, int count);
void delete_string_at_cursor(sector_t *sectors, const char *search_str, bool is_hex);
int parse_hex_string(const char *hex_str, unsigned char *output);