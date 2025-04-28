#include "block_io.h"
#include "ui.h"
#include"variables.h"
#include"stack.h"
#include"utils.h"

void handle_input(int ch, sector_t* sectors);
void save_buffer_to_file(sector_t *sectors);
void load_buffer_from_file(sector_t *sectors);
void create_sectors_directory();
void go_to_sector(off_t *sector);
bool display_help_flag = false;
bool exit_flag = false;
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

    sectors.buffer_size = SECTOR_SIZE;
    sectors.buffer = malloc(sectors.buffer_size);
    if (!sectors.buffer) {
        perror("Failed to allocate memory for sector buffer");
        close_device(&sectors.fd);
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
        if (exit_flag){
            break;
        }
    }

    endwin();
    close_device(&sectors.fd);

    if (sectors.buffer) {
        free(sectors.buffer);
    }
    
    clear_all_stacks();
    return 0;
}

void handle_input(int ch, sector_t* sectors) {
    static char search_string[MAX_INPUT_LEN] = {0};
    static int search_in_progress = 0;      
    static int current_match_index = 0;
    char mode;
    bool is_hex = false;
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
        
            case 'I': case 'i':
            {
                char input_str[MAX_INPUT_LEN] = {0};
            
                display_message("Mode (a=ASCII / h=HEX)? ");
                mode = getch();

                input_string(input_str, "Enter string to insert: ");
            
                if (mode == 'h' || mode == 'H') {
                    is_hex = true;
                }
            
                replace_string_at_cursor(sectors, input_str, is_hex);
            
                if (write_sector(sectors) != SECTOR_SIZE) {
                    display_error("Failed to write sector after insert");
                } else {
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
        case 'q': case 'Q': 
            exit_flag = true;
            graceful_exit(sectors);
            break;

            case '/':  
            {
                display_message("Search mode: (a) ASCII / (h) HEX? ");
                mode = getch();
                if (mode == 'h' || mode == 'H') {
                    is_hex = true;
                }
                input_string(search_string, "Enter search string: ");
            

            
                match_count = search_string_in_sector(sectors->buffer, search_string, matches, is_hex);
            
                if (match_count > 0) {
                    current_match_index = 0;
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
            }
            
            case 'r': case 'R':
            {
                display_message("Search mode: (a) ASCII / (h) HEX? ");
                mode = getch();
                if (mode == 'h' || mode == 'H') {
                    is_hex = true;
                }

                char search_string[MAX_INPUT_LEN] = {0};
                char replace_string[MAX_INPUT_LEN] = {0};
            
                display_message("Enter search string: ");
                echo(); curs_set(1);
                mvgetstr(LINES-1, MAX_ROW, search_string);
            
                display_message("Enter replacement string: ");
                mvgetstr(LINES-1, MAX_ROW, replace_string);
                noecho(); curs_set(0);
            
                replace_string_in_sector(sectors->buffer, search_string, replace_string, is_hex);
            
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
                display_message("Delete mode: (a) ASCII / (h) HEX? ");
                mode = getch();
                input_string(search_string, "Enter string to delete: ");
            
                if (mode == 'h' || mode == 'H') {
                    is_hex = true;
                }
            
                delete_string_at_cursor(sectors, search_string, is_hex);
            
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

void go_to_sector(off_t *sector) {
    display_message("Enter sector number (max 2048): ");

    echo();
    curs_set(1);  // Показываем курсор

    char *input = (char *)malloc(MAX_COL);
    if (!input) {
        display_error("Memory allocation failed");
        noecho();
        curs_set(0);
        return;
    }
    memset(input, 0, MAX_COL);

    mvgetstr(LINES - 1, MAX_ROW, input);

    noecho();
    curs_set(0);

    off_t new_sector = atol(input);
    if (new_sector >= 0 && new_sector <= MAX_SECTOR) {
        *sector = new_sector;
        clear();
        refresh();
    } else {
        display_error("Invalid sector number");
    }

    free(input);  // Освобождаем память!
}





void save_buffer_to_file(sector_t *sectors) {
    char *filename = (char *)malloc(128);  // Выделяем побольше места с запасом
    if (!filename) {
        display_error("Memory allocation failed for filename");
        return;
    }
    snprintf(filename, 128, "./sectors/sector_%lld.bin", (long long)sectors->sector);

    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        display_error("Failed to open file for writing");
        free(filename);
        return;
    }

    size_t written = fwrite(sectors->buffer, 1, SECTOR_SIZE, fp);
    fclose(fp);

    if (written != SECTOR_SIZE) {
        display_error("Failed to write full sector to file");
    } else {
        char *msg = (char *)malloc(128);
        if (!msg) {
            display_error("Memory allocation failed for message");
            free(filename);
            return;
        }
        snprintf(msg, 128, "Sector saved to %s", filename);
        display_message(msg);
        free(msg);
    }

    free(filename);
}



void load_buffer_from_file(sector_t *sectors) {
    char *filename = (char *)malloc(128);
    if (!filename) {
        display_error("Memory allocation failed for filename");
        return;
    }
    snprintf(filename, 128, "./sectors/sector_%lld.bin", (long long)sectors->sector);

    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        display_error("Failed to open file for reading");
        free(filename);
        return;
    }

    size_t read = fread(sectors->buffer, 1, SECTOR_SIZE, fp);
    fclose(fp);

    if (read != SECTOR_SIZE) {
        display_error("Failed to read full sector from file");
    } else {
        char *msg = (char *)malloc(128);
        if (!msg) {
            display_error("Memory allocation failed for message");
            free(filename);
            return;
        }
        snprintf(msg, 128, "Sector loaded from %s", filename);
        display_message(msg);
        free(msg);
    }

    free(filename);
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

