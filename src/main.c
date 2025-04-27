#include "block_io.h"
#include "ui.h"
#include"variables.h"
#include"stack.h"


void edit_byte(sector_t* sectors);
void handle_input(int ch, sector_t* sectors);
void save_buffer_to_file(sector_t *sectors);
void load_buffer_from_file(sector_t *sectors);
void create_sectors_directory();
void go_to_sector(off_t *sector);
void replace_string_in_sector(unsigned char *buffer, const char *search_str, const char *replace_str);
int search_string_in_sector(unsigned char *buffer, const char *search_str, int *matches) ;
int search_bytes_in_sector(unsigned char *buffer, unsigned char *search_bytes, int search_len);
void replace_string_at_cursor(sector_t *sectors, const char *input_str);
void delete_bytes_from_cursor(sector_t *sectors, int count);
void delete_string_at_cursor(sector_t *sectors, const char *search_str);
bool display_help_flag = false;
bool file_loaded = false;
int matches[MAX_MATCHES];  
int match_count = 0;
int display_mode = 0;
int main(int argc, char *argv[]) {
    
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <device/file>\n", argv[0]);
        return 1;
    }

    sector_t sectors = {0};
    create_sectors_directory();
    sectors.fd = open_device(argv[1]);
    if (sectors.fd < 0) {
        perror("Failed to open device");
        return 1;
    }

    init_ui();

    int ch;

    while (1) {
        if (!file_loaded) {
            if (read_sector(&sectors) != SECTOR_SIZE) {
                display_error("Failed to read sector");
                break;
            }
        } else {
            file_loaded = false; // Сброс после одной итерации
        }
        display_sector(sectors.buffer, SECTOR_SIZE, sectors.sector, sectors.cursor_x, sectors.cursor_y);

        ch = getch();
        handle_input(ch, &sectors);
        
    }

    return 0;
}

void handle_input(int ch, sector_t* sectors) {
    static char search_string[MAX_INPUT_LEN] = {0};
    static int search_in_progress = 0;      
    static int current_match_index = 0;
    bool redo_flag = false;
    bool undo_flag = false;
    switch (ch) {
        case 'a':          case 'A':// Переход к предыдущему сектору
            if (sectors->sector > 0) {

                sectors->sector--;
                sectors->cursor_x = 0;
                sectors->cursor_y = 0;
            }
            memset(matches, 0, sizeof(matches));
            match_count = 0;
            clear();
            refresh();
            break;
        case 'd':         case 'D': // Переход к следующему сектору
            sectors->sector++;
            sectors->cursor_x = 0;
            sectors->cursor_y = 0;
            memset(matches, 0, sizeof(matches));
            match_count = 0;
            clear();
            refresh();
            break;
        case 'E':         case 'e': // Режим редактирования
            edit_byte(sectors); // Редактируем байт
            if (write_sector(sectors) != SECTOR_SIZE) {
                display_error("Failed to write sector");
            } else {
                clear();
                refresh();
            }
            break;
        case 'g':  // Переход к определенному сектору
            go_to_sector(&(sectors->sector));  // Вызов новой функции для перехода
            break;
            
        case 'H':
        case 'h':  // Отображение подсказки
            if (display_help_flag) {
                // Если подсказка уже показана, скрываем ее
                display_help_flag = false;
                clear();  // Очищаем экран, чтобы скрыть подсказку
                refresh();  // Обновляем экран
            } else {
                // Если подсказка не показана, показываем ее
                display_help_flag = true;
                display_help();  // Показать подсказку
            }
            break;  
        
        case 'I':    case 'i':  // Вставка строки по текущему положению курсора
            {
                char input_str[MAX_INPUT_LEN] = {0};
            
                input_string(input_str, "Enter string to insert: ");
                

                
                // Заменяем байты в буфере
                replace_string_at_cursor(sectors, input_str);
            
                // Записываем изменения в файл
                if (write_sector(sectors) != SECTOR_SIZE) {
                    display_error("Failed to write sector after insert");
                }
                else {
                    clear();
                    refresh();
                }
                break;
            }
        case 'l': case 'L':
            load_buffer_from_file(sectors);
            if (write_sector(sectors) != SECTOR_SIZE) {  // Добавлено!
                display_error("Failed to write sector after load");
            }
            file_loaded = true;
            break;
        case 'N':
        case 'n':  // Следующее совпадение
                if (search_in_progress && match_count > 0) {
                    current_match_index = (current_match_index + 1) % match_count; // Переходим к следующему совпадению
                    sectors->cursor_x = matches[current_match_index] % 16;
                    sectors->cursor_y = matches[current_match_index] / 16;
                    char msg[256];
                    snprintf(msg, sizeof(msg), "Found at byte %d", matches[current_match_index]);
                    display_message(msg);
                }
                break;

        case 'P':    
        case 'p':  // Предыдущее совпадение
            if (search_in_progress && match_count > 0) {
                current_match_index = (current_match_index - 1 + match_count) % match_count; // Переходим к предыдущему совпадению
                sectors->cursor_x = matches[current_match_index] % 16;
                sectors->cursor_y = matches[current_match_index] / 16;
                char msg[256];
                snprintf(msg, sizeof(msg), "Found at byte %d", matches[current_match_index]);
                display_message(msg);
            }
            break;
        case 'q':  
            graceful_exit(sectors);
            break;

            case '/':  
            input_string(search_string, "Enter search string: ");
            // Начинаем поиск
            match_count = search_string_in_sector(sectors->buffer, search_string, matches);

            if (match_count > 0) {
                current_match_index = 0;  // Начинаем с первого совпадения
                char msg[256];
                snprintf(msg, sizeof(msg), "Found %d matches", match_count);
                display_message(msg);
                sectors->cursor_x = matches[current_match_index] % 16;
                sectors->cursor_y = matches[current_match_index] / 16;
            } else {
                display_message("No matches found");
            }

            clear();
            refresh();
            search_in_progress = 1;
            break;
        case 'r': case 'R':
            {
                char search_string[MAX_INPUT_LEN] = {0};
                char replace_string[MAX_INPUT_LEN] = {0};
            
                display_message("Enter search string: ");
                echo(); curs_set(1);
                mvgetstr(LINES-1, MAX_ROW, search_string);
            
                display_message("Enter replacement string: ");
                mvgetstr(LINES-1, MAX_ROW, replace_string);
                noecho(); curs_set(0);
            
                replace_string_in_sector(sectors->buffer, search_string, replace_string);
            
                if (write_sector(sectors) != SECTOR_SIZE) {
                    display_error("Failed to write sector after replace");
                }
                break;
            }
        case 's': case 'S':
            save_buffer_to_file(sectors);
            break; 
        case 'x':  // Удаление найденного слова
            {
                input_string(search_string, "Enter string to delete: ");

                // Удаляем найденное слово
                delete_string_at_cursor(sectors, search_string);

                // Записываем изменения в файл
                if (write_sector(sectors) != SECTOR_SIZE) {
                    display_error("Failed to write sector after deletion");
                }
                clear();
                refresh();
                break;
            }
        case 'X':  // Удаление символов с позиции курсора
            {
                char delete_count_str[10];
                input_string(delete_count_str, "Enter number of bytes to delete: ");

                // Преобразуем введенную строку в число
                int delete_count = atoi(delete_count_str);

                // Удаляем байты (заменяем их на нули)
                delete_bytes_from_cursor(sectors, delete_count);

                // Записываем изменения в файл
                if (write_sector(sectors) != SECTOR_SIZE) {
                    display_error("Failed to write sector after deletion");
                }
                clear();
                refresh();
                break;
            }
        case KEY_RIGHT:  // Переход вправо
            if (sectors->cursor_x < 15) {
                sectors->cursor_x++;
            } else if (sectors->cursor_y < 31) {
                sectors->cursor_y++;
                sectors->cursor_x = 0;
            } else {
                sectors->sector++;
                sectors->cursor_x = 0;
                sectors->cursor_y = 0;
            }
            break;

        case KEY_LEFT:  // Переход влево
            if (sectors->cursor_x > 0) {
                sectors->cursor_x--;
            } else if (sectors->cursor_y > 0) {
                sectors->cursor_y--;
                sectors->cursor_x = 15;
            } else {
                if (sectors->sector > 0) {
                    sectors->sector--;
                    sectors->cursor_x = 15;
                    sectors->cursor_y = 31;
                }
            }
            break;

        case KEY_UP:  // Переход вверх
            if (sectors->cursor_y > 0) {
                sectors->cursor_y--;
            }
            break;

        case KEY_DOWN:  // Переход вниз
            if (sectors->cursor_y < 31) {
                sectors->cursor_y++;
            }
            break;
        case '\t':  // TAB для переключения режима отображения
            display_mode = (display_mode + 1) % 2;  // Между 0 (HEX) и 1 (ASCII)
            break;
        case 'u': case 'U':
            undo_flag = undo(sectors);
            if(undo_flag){
                if (write_sector(sectors) != SECTOR_SIZE) {
                    display_error("Failed to write sector after deletion");
                }
                clear();
                refresh();
            }
            break;
        
        case 21:  // CTRL+U — Redo
            redo_flag = redo(sectors);
            if(redo_flag){
                if (write_sector(sectors) != SECTOR_SIZE) {
                    display_error("Failed to write sector after deletion");
                }
                clear();
                refresh();
            }
            break;
        default:
            break;
    }

    
}

void edit_byte(sector_t* sectors) {
    int index = sectors->cursor_y * 16 + sectors->cursor_x;
    int ch;
    char input[3] = {0};

    unsigned char old_value = sectors->buffer[index]; // <-- сохраняем старое значение

    if (display_mode == 0) {
        input_string(input, "Enter HEX value (2 characters, 0-9, A-F): ");
        if (strlen(input) == 2 && isxdigit(input[0]) && isxdigit(input[1])) {
            unsigned char new_value = (unsigned char)strtol(input, NULL, 16);
            sectors->buffer[index] = new_value;

            // сохраняем операцию в undo стек
            save_undo_state(sectors, OP_BYTE_CHANGE, index, old_value, new_value, NULL, NULL,0);

            // Очищаем стек redo после нового действия
            redo_top = -1;
        } else {
            display_error("Invalid HEX input. Please enter two hexadecimal characters.");
        }
    } else {
        display_message("Enter ASCII value: ");
        echo();
        curs_set(1);
        ch = getch();
        if (ch >= 0x20 && ch <= 0x7E) {
            unsigned char new_value = (unsigned char)ch;
            sectors->buffer[index] = new_value;

            save_undo_state(sectors, OP_BYTE_CHANGE, index, old_value, new_value, NULL, NULL,0);

            redo_top = -1;
        } else {
            display_error("Invalid ASCII input. Only printable characters allowed.");
        }
        noecho();
        curs_set(0);
    }
}


void go_to_sector(off_t *sector) {

    // Выводим сообщение в нижней строке
    display_message("Enter sector number (max 2048): ");

    // Включаем отображение ввода
    echo();
    curs_set(1);  // Показываем курсор для ввода

    // Место для ввода данных
    char input[MAX_COL];
    
    // Вводим строку в том же месте, где было сообщение
    mvgetstr(LINES - 1, MAX_ROW, input);  // Чтение ввода с позиции (LINES-1, 30)

    // Отключаем отображение символов и убираем курсор
    noecho();
    curs_set(0);

    // Преобразуем строку в число
    off_t new_sector = atol(input);
    if (new_sector >= 0 && new_sector <= MAX_SECTOR) {
        *sector = new_sector;
        // После ввода очищаем строку и обновляем экран
        clear();
        refresh();
    } else {
        display_error("Invalid sector number");
    }
}




void save_buffer_to_file(sector_t *sectors) {
    char filename[64];
    snprintf(filename, sizeof(filename), "./sectors/sector_%lld.bin", (long long)sectors->sector);

    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        display_error("Failed to open file for writing");
        return;
    }

    size_t written = fwrite(sectors->buffer, 1, SECTOR_SIZE, fp);
    fclose(fp);

    if (written != SECTOR_SIZE) {
        display_error("Failed to write full sector to file");
    } else {
        char msg[64];
        snprintf(msg, sizeof(msg), "Sector saved to %s", filename);
        display_message(msg);
    }
}


void load_buffer_from_file(sector_t *sectors) {
    char filename[64];
    snprintf(filename, sizeof(filename), "./sectors/sector_%lld.bin", (long long)sectors->sector);

    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        display_error("Failed to open file for reading");
        return;
    }

    size_t read = fread(sectors->buffer, 1, SECTOR_SIZE, fp);
    fclose(fp);

    if (read != SECTOR_SIZE) {
        display_error("Failed to read full sector from file");
    } else {
        char msg[64];
        snprintf(msg, sizeof(msg), "Sector loaded from %s", filename);
        display_message(msg);
    }
}


void create_sectors_directory() {
    struct stat st = {0};

    if (stat("./sectors", &st) == -1) {
        if (mkdir("./sectors", 0755) == -1) {
            perror("Unable to create sectors directory");
        } else {
            printf("Directory 'sectors' created successfully\n");
        }
    } else {
        printf("Directory 'sectors' already exists\n");
    }
}

int search_string_in_sector(unsigned char *buffer, const char *search_str, int *matches) {
    int str_len = strlen(search_str);
    int match_count = 0;

    for (int i = 0; i < SECTOR_SIZE - str_len; i++) {
        if (memcmp(buffer + i, search_str, str_len) == 0) {
            if (match_count < MAX_MATCHES) {
                matches[match_count++] = i;  // Сохраняем индекс совпадения
            }
        }
    }

    return match_count;  // Возвращаем количество найденных совпадений
}

int search_bytes_in_sector(unsigned char *buffer, unsigned char *search_bytes, int search_len) {
    for (int i = 0; i < SECTOR_SIZE - search_len; i++) {
        if (memcmp(buffer + i, search_bytes, search_len) == 0) {
            return i; // Возвращаем индекс первого вхождения
        }
    }
    return -1; // Если не найдено
}

void replace_string_in_sector(unsigned char *buffer, const char *search_str, const char *replace_str) {
    int search_len = strlen(search_str);
    int replace_len = strlen(replace_str);

    if (replace_len > search_len) {
        display_error("Replacement string is longer than search string!");
        return;
    }

    for (int i = 0; i <= SECTOR_SIZE - search_len; i++) {
        if (memcmp(buffer + i, search_str, search_len) == 0) {
            memcpy(buffer + i, replace_str, replace_len);
            // Если замена короче, заполняем оставшиеся байты нулями
            for (int j = replace_len; j < search_len; j++) {
                buffer[i + j] = 0x00;
            }
        }
    }

    display_message("Replacement complete.");
}

void replace_string_at_cursor(sector_t *sectors, const char *input_str) {
    int index = sectors->cursor_y * 16 + sectors->cursor_x;  // Вычисляем индекс байта
    int input_len = strlen(input_str);

    // Проверка выхода за пределы буфера
    if (index + input_len > SECTOR_SIZE) {
        display_error("Insert string exceeds sector boundaries!");
        return;
    }

    char old_line[17] = {0}; // до 16 символов + нуль-терминатор
    for (int i = 0; i < input_len; i++) {
        old_line[i] = sectors->buffer[index + i];
    }

    // Сохраняем в undo стек перед изменением
    save_undo_state(sectors, OP_LINE_REPLACE, index, 0, 0, old_line, input_str, input_len);

    // Заменяем байты из строки
    for (int i = 0; i < input_len; i++) {
        sectors->buffer[index + i] = input_str[i];
    }

    // После любого изменения нужно сбросить redo стек
    redo_top = -1;
}



void delete_string_at_cursor(sector_t *sectors, const char *search_str) {
    int search_len = strlen(search_str);
    unsigned char *buffer = sectors->buffer;

    for (int i = 0; i <= SECTOR_SIZE - search_len; i++) {
        if (memcmp(buffer + i, search_str, search_len) == 0) {
            char old_line[17] = {0};
            memcpy(old_line, buffer + i, search_len);

            memset(buffer + i, 0x00, search_len);

            save_undo_state(sectors, OP_LINE_REMOVE, i, 0, 0, old_line, NULL, search_len);

            redo_top = -1;

            display_message("String deleted.");
            return;
        }
    }

    display_message("String not found.");
}



void delete_bytes_from_cursor(sector_t *sectors, int count) {
    int index = sectors->cursor_y * 16 + sectors->cursor_x;

    if (index + count > SECTOR_SIZE) {
        display_error("Delete range exceeds sector boundaries!");
        return;
    }

    // Сохраняем удаляемые байты
    char old_data[17] = {0};  // Максимум 16 байт + \0
    memcpy(old_data, &sectors->buffer[index], count);

    // Удаляем байты
    for (int i = 0; i < count; i++) {
        sectors->buffer[index + i] = 0x00;
    }

    // Сохраняем в undo
    save_undo_state(sectors, OP_LINE_REMOVE, index, 0, 0, old_data, NULL, count);

    // Сброс redo
    redo_top = -1;

    display_message("Bytes deleted.");
}





