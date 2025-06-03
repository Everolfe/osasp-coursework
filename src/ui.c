#include "ui.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
SCREEN* main_screen = NULL;
WINDOW *main_win = NULL;
WINDOW *help_win = NULL;
bool help_visible = false;
void init_ui() {
    main_screen = newterm(NULL, stdout, stdin);
    if (!main_screen) {
        fprintf(stderr, "Failed to initialize curses screen.\n");
        exit(1);
    }
    set_term(main_screen);

    start_color();
    init_pair(1, COLOR_GREEN, COLOR_BLACK);  // match highlight
    init_pair(2, COLOR_YELLOW, COLOR_BLACK); // status/help/errors

    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);  // для работы стрелок

    main_win = stdscr; // alias
}


const char *detect_filesystem(const unsigned char *boot_sector, const unsigned char *ext4_sector) {
    // FAT32 — в boot sector по смещению 0x52 строка "FAT32   "
    if (memcmp(boot_sector + 0x52, "FAT32   ", 8) == 0) {
        return "FAT32";
    }

    // NTFS — в boot sector по смещению 3 строка "NTFS    "
    if (memcmp(boot_sector + 3, "NTFS    ", 8) == 0) {
        return "NTFS";
    }

    // EXT4 — в superblock по смещению 56 два байта: 0x53 0xEF
    if (ext4_sector) {
        uint16_t magic = *(const uint16_t *)(ext4_sector + 56);
        if (magic == 0xEF53) {
            return "EXT4";
        }
    }

    return "Unknown FS";
}



void display_error(const char *msg) {
    if (!main_win) return;
    wmove(main_win, 0, 0);
    wclrtoeol(main_win);
    wattron(main_win, COLOR_PAIR(2));
    waddstr(main_win, "ERROR: ");
    waddstr(main_win, msg);
    wattroff(main_win, COLOR_PAIR(2));
    wrefresh(main_win);
    getch();
}

void display_sector(const unsigned char *buffer, size_t size, off_t sector_num, int cursor_x, int cursor_y) {
    const char *mode_names[] = {"HEX", "ASCII", "DEC", "BIN"};
    int cell_width;

    switch (display_mode) {
        case 0: cell_width = 3; break;
        case 1: cell_width = 3; break;
        case 2: cell_width = 4; break;
        case 3: cell_width = 9; break;
        default: cell_width = 3; break;
    }

    // Строка заголовка
    char header[256];
    snprintf(header, sizeof(header), "File System: %s | Sector: %lld  |  Size: %lu bytes  |  Cursor: [%02d:%02d]  |  Mode: %s ",
             fs_name, (long long)sector_num, size, cursor_y, cursor_x, mode_names[display_mode]);

    wattron(main_win, A_BOLD | COLOR_PAIR(2));
    wmove(main_win, 0, 0);
    waddstr(main_win, header);
    wattroff(main_win, A_BOLD | COLOR_PAIR(2));

    // Граница
    for (int i = 0; i < COLS; i++) {
        wmove(main_win, 1, i);
        waddch(main_win, ACS_HLINE);
    }

    // Вывод сектора
    for (int row = 0; row < 32; row++) {
        for (int col = 0; col < 16; col++) {
            int index = row * 16 + col;
            if (index >= size) continue;

            int y = 2 + row;
            int x = col * cell_width;
            wmove(main_win, y, x);
            waddstr(main_win, "   ");  // очистка

            wmove(main_win, y, x);

            bool highlight = (row == cursor_y && col == cursor_x);
            if (highlight) wattron(main_win, A_REVERSE);
            else {
                for (int m = 0; m < match_count; m++) {
                    if (matches[m] == index) {
                        wattron(main_win, COLOR_PAIR(1));
                        break;
                    }
                }
            }

            char cell[16];
            switch (display_mode) {
                case 0:  // HEX
                    snprintf(cell, sizeof(cell), "%02X", buffer[index]);
                    break;
                case 1:  // ASCII
                    if (buffer[index] >= 32 && buffer[index] <= 126)
                        snprintf(cell, sizeof(cell), " %c", buffer[index]);
                    else
                        snprintf(cell, sizeof(cell), " .");
                    break;
                case 2:  // DEC
                    snprintf(cell, sizeof(cell), "%03d", buffer[index]);
                    break;
                case 3:  // BIN
                    for (int b = 7; b >= 0; b--)
                        cell[7 - b] = (buffer[index] & (1 << b)) ? '1' : '0';
                    cell[8] = '\0';
                    break;
            }

            waddstr(main_win, cell);

            if (highlight) wattroff(main_win, A_REVERSE);
            else wattroff(main_win, COLOR_PAIR(1));

            // ASCII-комментарий справа
            if (col == 15 && display_mode != 1) {
                int ascii_offset = (display_mode == 3) ? 148 : 70;
                wmove(main_win, y, ascii_offset);
                waddstr(main_win, "| ");
                for (int ascii_col = 0; ascii_col < 16; ascii_col++) {
                    int ascii_idx = row * 16 + ascii_col;
                    if (ascii_idx >= size) break;
                    unsigned char c = buffer[ascii_idx];
                    waddch(main_win, (c >= 32 && c <= 126) ? c : '.');
                }
            }
        }
    }

    wrefresh(main_win);
}



void display_help() {
    if (help_visible && help_win) {
        werase(help_win);
        wrefresh(help_win);
        delwin(help_win);
        help_win = NULL;
        help_visible = false;
        return;
    }

    int width = 80;
    int height = 7;
    int starty = LINES - height;
    int startx = 0;

    help_win = newwin(height, width, starty, startx);
    box(help_win, 0, 0);
    wattron(help_win, A_BOLD | COLOR_PAIR(2));

    const char *help_lines[] = {
        " [Q] Quit  [/] Search  [X] Delete Word  [Ctrl+U] Redo  [U] Undo",
        " [N] Next Match  [P] Previous Match  [G] Go to Sector",
        " [D] Next Sector  [A] Previous Sector [F5] Write Sector to Device",
        " [I] Insert String  [E] Edit Byte  [L] Load  [S] Save  [R] Replace",
        " [Tab] Toggle Mode  [Arrows] Move Cursor"
    };

    for (int i = 0; i < 5; i++) {
        wmove(help_win, i + 1, 2);
        waddstr(help_win, help_lines[i]);
    }

    wattroff(help_win, A_BOLD | COLOR_PAIR(2));
    wrefresh(help_win);

    help_visible = true;
}



void display_message(const char *msg) {
    if (!main_win) return;
    wmove(main_win, LINES - 1, 0);       // Установка курсора в последнюю строку
    wclrtoeol(main_win);                // Очистить до конца строки
    wattron(main_win, A_BOLD);         // Включить жирный текст
    waddstr(main_win, msg);            // Вывести строку
    wattroff(main_win, A_BOLD);        // Выключить жирный
    wrefresh(main_win);                // Обновить окно
}


void graceful_exit(sector_t *sectors) {
    close_device(&sectors->fd);

    if (help_win) {
        delwin(help_win);
        help_win = NULL;
    }

    if (main_win && main_win != stdscr) {
        delwin(main_win);
        main_win = NULL;
    }

    clear();
    refresh();
    endwin();

    if (main_screen) {
        delscreen(main_screen);
        main_screen = NULL;
    }
}

void input_string(char *buffer, const char *prompt) {
    display_message(prompt);
    echo();
    curs_set(1);
    mvgetstr(LINES - 1, 20, buffer);
    noecho();
    curs_set(0);
}