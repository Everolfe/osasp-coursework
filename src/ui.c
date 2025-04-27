#include "ui.h"
#include <stdio.h>

void init_ui() {
    initscr();             // Инициализация ncurses
    start_color();         // Включаем поддержку цветов
    init_pair(1, COLOR_GREEN, COLOR_BLACK);  // Определяем пару цветов: красный текст на черном фоне
    init_pair(2, COLOR_YELLOW, COLOR_BLACK); // Жёлтый для обычного текста
    cbreak();              // Отключаем буферизацию ввода
    noecho();              // Отключаем вывод вводимых символов
    keypad(stdscr, TRUE);  // Включаем поддержку функциональных клавиш (стрелок)
}

void close_ui() {
    endwin();
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
    move(0, 0);
    clrtoeol();  // Очищаем строку для обновления информации о секторе
    printw("Sector: %lld (Size: %lu bytes)   Cursor: [%d:%d]   Byte: 0x%02X ('%c')   Mode: %s",
            (long long)sector_num, size,
            cursor_y, cursor_x,
            buffer[cursor_y * 16 + cursor_x],
            (buffer[cursor_y * 16 + cursor_x] >= 32 && buffer[cursor_y * 16 + cursor_x] <= 126) ?
                buffer[cursor_y * 16 + cursor_x] : '.',
            (display_mode == 0) ? "HEX" : "ASCII");

            for (int i = 0; i < 32; i++) {
                for (int j = 0; j < 16; j++) {
                    int index = i * 16 + j;
                    if (index >= size) continue;
        
                    move(2 + i, j * 3);  // Перемещаем курсор на нужную позицию
                    // Подсветка текущей позиции курсора
                    if (i == cursor_y && j == cursor_x) {
                        attron(A_REVERSE);
                    } else {
                        // Проверка на совпадение
                        for (int k = 0; k < match_count; k++) {
                            if (matches[k] == index) {
                                attron(COLOR_PAIR(1));
                                break;
                            }
                        }
                    }
        
                    // Печать байта в зависимости от режима
                    if (display_mode == 0) {
                        printw("%02X", buffer[index]);  // HEX
                    } else {
                        unsigned char c = buffer[index];
                        if (c >= 32 && c <= 126) {
                            printw(" %c", c); // Печатаемый ASCII
                        } else {
                            printw(" .");    // Непечатаемый символ
                        }
                    }
        
                    attroff(A_REVERSE);
                    attroff(COLOR_PAIR(1));
                }
            }
        
            refresh();
}
void display_help() {
    // Очищаем нижнюю строку и выводим подсказку
    move(LINES - 6, 0);  
    clrtoeol();  // Очищаем строку
    attron(A_BOLD | COLOR_PAIR(2)); // Выделим текст для подсказки (жёлтым)

    // Печатаем подсказку
    printw("Help: [q] Exit  [/] Search  [x] Delete Word  [X] Delete Bytes\n");
    printw("[N/n] Next Match  [P/p] Previous Match  [d/D] Next Sector\n");
    printw("[i/I] Insert String  [a/A] Previous Sector  [e/E] Edit Byte\n");
    printw("[l/L] Load Sector  [s/S] Save Sector  [r/R] Replace String\n");
    printw("[g] Go to Sector  [Arrow keys] Move Cursor  [Tab] Toggle Mode");

    attroff(A_BOLD | COLOR_PAIR(2));  // Снимаем выделение
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
    close_ui();
    close_device(sectors->fd);
    endwin();
    exit(0);
}

void input_string(char *buffer, const char *prompt) {
    display_message(prompt);
    echo();
    curs_set(1);
    mvgetstr(LINES - 1, MAX_ROW+10, buffer);
    noecho();
    curs_set(0);
}