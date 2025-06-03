#include "block_io.h"
#include "ui.h"
#include"variables.h"
#include"stack.h"
#include"utils.h"
#include <signal.h>
#define TOTAL_MODES 4
#define CTRL_U 21
char* fs_name;
void handle_input(int ch, sector_t* sectors);
void save_buffer_to_file(sector_t *sectors);
void load_buffer_from_file(sector_t *sectors);
void create_sectors_directory();
void go_to_sector(off_t *sector);
void signal_handler(int sig);
bool display_help_flag = false;
bool exit_flag = false;
bool file_loaded = false;
bool sector_modified = false;
bool new_sector = false;
int matches[MAX_MATCHES];  
int match_count = 0;
int display_mode = 0;
int main(int argc, char *argv[]) {
    signal(SIGINT, signal_handler);   // Ctrl+C
    signal(SIGTSTP, signal_handler); 
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
    unsigned char boot[SECTOR_SIZE];
    unsigned char ext[SECTOR_SIZE];
    
    // Считать сектор 0 (boot sector)
    if (lseek(sectors.fd, 0 * SECTOR_SIZE, SEEK_SET) == -1) {
        perror("lseek boot failed");
        exit(1);
    }
    if (read(sectors.fd, boot, SECTOR_SIZE) != SECTOR_SIZE) {
        perror("read boot failed");
        exit(1);
    }
    
    // Считать сектор 2 (для ext superblock начало в 1024 байта = сектор 2)
    if (lseek(sectors.fd, 2 * SECTOR_SIZE, SEEK_SET) == -1) {
        perror("lseek ext failed");
        exit(1);
    }
    if (read(sectors.fd, ext, SECTOR_SIZE) != SECTOR_SIZE) {
        perror("read ext failed");
        exit(1);
    }
    
    fs_name = detect_filesystem(boot, ext);
    while (1) {
        if (new_sector) {
            if (read_sector(&sectors) != SECTOR_SIZE) {
                display_error("Failed to read sector");
                break;
            }
            new_sector = false;  // Сбросим флаг до следующего перехода
        }
        display_sector(sectors.buffer, SECTOR_SIZE, sectors.sector, sectors.cursor_x, sectors.cursor_y);
        ch = getch();
        handle_input(ch, &sectors);
        if (exit_flag){
        break;
        }
    }

    close_device(&sectors.fd);

    if (sectors.buffer) {
        free(sectors.buffer);
    }
    clear_all_stacks();
    graceful_exit(&sectors);
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
            if (sector_modified) {
                if (write_sector(sectors) != SECTOR_SIZE) {
                    display_error("Failed to save modified sector");
                } else {
                    sector_modified = false;
                }
            }
            if (sectors->sector > 0) {
                new_sector = true;
                sectors->sector--;
                sectors->cursor_x = 0;
                sectors->cursor_y = 0;
            }
            memset(matches, 0, sizeof(matches));
            match_count = 0;
            clear();
            break;
        case 'd':         case 'D': // Переход к следующему сектору
            if (sector_modified) {
                if (write_sector(sectors) != SECTOR_SIZE) {
                    display_error("Failed to save modified sector");
                } else {
                    sector_modified = false;
                }
            }
            new_sector = true;
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
            sector_modified = true;
            clear();
            refresh();
            break;
        case 'g':  // Переход к определенному сектору
            if (sector_modified) {
                if (write_sector(sectors) != SECTOR_SIZE) {
                    display_error("Failed to save modified sector");
                } else {
                    sector_modified = false;
                }
            }
            new_sector = true;
            go_to_sector(&(sectors->sector));  // Вызов новой функции для перехода
            break;
        case 'H':
        case 'h':  // Отображение подсказки
            display_help();
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
                sector_modified = true;
                clear();
                refresh();
                break;
            }
            
        case 'l': case 'L':
            load_buffer_from_file(sectors);
            if (write_sector(sectors) != SECTOR_SIZE) {  // Добавлено!
                display_error("Failed to write sector after load");
            }
            file_loaded = true;
            sector_modified = true;
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
                sector_modified = true;
                clear();
                refresh();
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
                sector_modified = true;
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
                sector_modified = true;
                // Записываем изменения в файл
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
                if (sector_modified) {
                    if (write_sector(sectors) != SECTOR_SIZE) {
                        display_error("Failed to save modified sector");
                    } else {
                        sector_modified = false;
                    }
                }
                new_sector = true;
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
                    if (sector_modified) {
                        if (write_sector(sectors) != SECTOR_SIZE) {
                            display_error("Failed to save modified sector");
                        } else {
                            sector_modified = false;
                        }
                    }
                    new_sector = true;
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
            display_mode = (display_mode + 1) % TOTAL_MODES;  // Между 0 (HEX) и 1 (ASCII)
            clear();
            break;
        case 'u': case 'U':
            undo_flag = undo(sectors);
            if(undo_flag){
                clear();
                refresh();
            }
            break;
        
        case CTRL_U:  // CTRL+U — Redo
            redo_flag = redo(sectors);
            if(redo_flag){
                clear();
                refresh();
            }
            break;
        case KEY_F(5):  // F5
            if (write_sector(sectors) != SECTOR_SIZE) {
                display_error("Failed to write sector");
            } else {
                display_message("Sector written to device");
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

void signal_handler(int sig) {
    exit_flag = true;
    endwin();
    printf("\nCaught signal %d. Exiting...\n", sig);
}