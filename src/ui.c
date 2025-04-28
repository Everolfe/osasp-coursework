#include "ui.h"
#include <stdio.h>

void init_ui() {
    initscr();
    start_color();
    init_pair(1, COLOR_GREEN, COLOR_BLACK);
    init_pair(2, COLOR_YELLOW, COLOR_BLACK);
    cbreak();
    noecho();
    curs_set(0);        
    keypad(stdscr, TRUE);
}



void display_error(const char *msg) {
    move(0, 0);
    clrtoeol();  // Очищаем текущую строку, чтобы заменить старое сообщение
    attron(COLOR_PAIR(2)); // Используем желтый цвет для ошибок
    printw("ERROR: %s", msg);
    attroff(COLOR_PAIR(2));  // Выключаем подсветку
    refresh();
    getch();
}

void display_sector(const unsigned char *buffer, size_t size, off_t sector_num, int cursor_x, int cursor_y) {

    // Заголовок
    attron(A_BOLD | COLOR_PAIR(2));
    mvprintw(0, 0, " Sector: %lld  |  Size: %lu bytes  |  Cursor: [%02d:%02d]  |  Mode: %s ",
            (long long)sector_num, size, cursor_y, cursor_x,
            (display_mode == 0) ? "HEX" : "ASCII");
    attroff(A_BOLD | COLOR_PAIR(2));

    // Разделение экрана на два блока: HEX и ASCII
    for (int row = 0; row < 32; row++) {
        for (int col = 0; col < 16; col++) {
            int index = row * 16 + col;
            if (index >= size) continue;

            int y = 2 + row;
            int x = 3 * col;

            // HEX вывод
            move(y, x);
            if (row == cursor_y && col == cursor_x) {
                attron(A_REVERSE);
            } else {
                for (int m = 0; m < match_count; m++) {
                    if (matches[m] == index) {
                        attron(COLOR_PAIR(1));
                        break;
                    }
                }
            }

            if (display_mode == 0) {
                printw("%02X", buffer[index]);
            } else {
                unsigned char c = buffer[index];
                if (c >= 32 && c <= 126) {
                    printw(" %c", c);
                } else {
                    printw(" .");
                }
            }

            attroff(A_REVERSE);
            attroff(COLOR_PAIR(1));

            // Разделение между HEX и ASCII выводом
            if (col == 15) {
                mvprintw(y, 50, "| ");

                // ASCII вывод
                for (int ascii_col = 0; ascii_col < 16; ascii_col++) {
                    int ascii_idx = row * 16 + ascii_col;
                    if (ascii_idx >= size) break;
                    unsigned char c = buffer[ascii_idx];
                    if (c >= 32 && c <= 126) {
                        printw("%c", c);
                    } else {
                        printw(".");
                    }
                }
            }
        }
    }

    // Нарисовать разделительную линию
    for (int i = 0; i < COLS; i++) {
        mvaddch(1, i, ACS_HLINE);
    }

    refresh();
}

void display_help() {
    move(LINES - 6, 0);
    clrtoeol();
    attron(A_BOLD | COLOR_PAIR(2));
    mvprintw(LINES - 6, 0, " [Q] Quit  [/] Search  [X] Delete Word  [Ctrl+U] Redo  [U] Undo");
    mvprintw(LINES - 5, 0, " [N] Next Match  [P] Previous Match  [G] Go to Sector");
    mvprintw(LINES - 4, 0, " [D] Next Sector  [A] Previous Sector");
    mvprintw(LINES - 3, 0, " [I] Insert String  [E] Edit Byte  [L] Load  [S] Save  [R] Replace");
    mvprintw(LINES - 2, 0, " [Tab] Toggle Mode  [Arrows] Move Cursor");
    attroff(A_BOLD | COLOR_PAIR(2));
    refresh();
}


void display_message(const char *msg) {
    move(LINES - 1, 0);  // Перемещаемся в нижнюю строку
    clrtoeol();  // Очищаем строку, чтобы заменить сообщение
    attron(A_BOLD);
    printw("%s", msg);  // Выводим сообщение
    attroff(A_BOLD);
    refresh();
}

void graceful_exit(sector_t *sectors) {
    close_device(&sectors->fd);
    clear();
    refresh();
    endwin();
}

void input_string(char *buffer, const char *prompt) {
    display_message(prompt);
    echo();
    curs_set(1);
    mvgetstr(LINES - 1, MAX_ROW+10, buffer);
    noecho();
    curs_set(0);
}