#include"utils.h"

void edit_byte(sector_t* sectors) {
    int index = sectors->cursor_y * 16 + sectors->cursor_x;
    int ch;
    char* input = (char*)malloc(3);  // Выделяем 3 байта (+1 для null-терминатора)

    if (!input) {
        display_error("Memory allocation failed");
        return;
    }
    memset(input, 0, 3);

    unsigned char old_value = sectors->buffer[index]; // сохраняем старое значение

    if (display_mode == 0) {  // HEX режим
        input_string(input, "Enter HEX value (2 characters, 0-9, A-F): ");
        if (strlen(input) == 2 && isxdigit(input[0]) && isxdigit(input[1])) {
            unsigned char new_value = (unsigned char)strtol(input, NULL, 16);
            sectors->buffer[index] = new_value;

            save_undo_state(sectors, OP_BYTE_CHANGE, index, old_value, new_value, NULL, NULL, 0);

            redo_top = -1;  // очищаем redo стек
        } else {
            display_error("Invalid HEX input. Please enter two hexadecimal characters.");
        }
    } else if (display_mode == 1) {  // ASCII режим
        display_message("Enter ASCII value: ");
        echo();
        curs_set(1);
        ch = getch();
        if (ch >= 0x20 && ch <= 0x7E) {  // проверка на печатные символы
            unsigned char new_value = (unsigned char)ch;
            sectors->buffer[index] = new_value;

            save_undo_state(sectors, OP_BYTE_CHANGE, index, old_value, new_value, NULL, NULL, 0);

            redo_top = -1;
            clear_redo_stack();
        } else {
            display_error("Invalid ASCII input. Only printable characters allowed.");
        }
        noecho();
        curs_set(0);
    } else {  // Ввод числа
        input_string(input, "Enter decimal value (0-255): ");
        int value = atoi(input);
        if (value >= 0 && value <= 255) {
            unsigned char new_value = (unsigned char)value;
            sectors->buffer[index] = new_value;

            save_undo_state(sectors, OP_BYTE_CHANGE, index, old_value, new_value, NULL, NULL, 0);

            redo_top = -1;
            clear_redo_stack();
        } else {
            display_error("Invalid input. Please enter a decimal number between 0 and 255.");
        }
    }

    free(input);  // освобождаем память
}


void replace_string_in_sector(unsigned char *buffer, const char *search_str, const char *replace_str, bool is_hex) {
    unsigned char *search_bytes = (unsigned char *)malloc(17);
    unsigned char *replace_bytes = (unsigned char *)malloc(17);
    if (!search_bytes || !replace_bytes) {
        display_error("Memory allocation failed");
        free(search_bytes);
        free(replace_bytes);
        return;
    }
    memset(search_bytes, 0, 17);
    memset(replace_bytes, 0, 17);

    int search_len = 0;
    int replace_len = 0;

    if (is_hex) {
        search_len = parse_hex_string(search_str, search_bytes);
        replace_len = parse_hex_string(replace_str, replace_bytes);
    } else {
        search_len = strlen(search_str);
        replace_len = strlen(replace_str);
        memcpy(search_bytes, search_str, search_len);
        memcpy(replace_bytes, replace_str, replace_len);
    }

    if (search_len == 0 || replace_len == 0 || search_len > 16 || replace_len > 16) {
        display_error("Invalid search or replacement length");
        free(search_bytes);
        free(replace_bytes);
        return;
    }

    if (replace_len > search_len) {
        display_error("Replacement string is longer than search string!");
        free(search_bytes);
        free(replace_bytes);
        return;
    }

    int replacements = 0;
    for (int i = 0; i <= SECTOR_SIZE - search_len; i++) {
        if (memcmp(buffer + i, search_bytes, search_len) == 0) {
            memcpy(buffer + i, replace_bytes, replace_len);
            for (int j = replace_len; j < search_len; j++) {
                buffer[i + j] = 0x00; // Заполняем остаток нулями
            }
            replacements++;
        }
    }

    free(search_bytes);
    free(replace_bytes);

    if (replacements > 0) {
        display_message("Replacement complete.");
    } else {
        display_message("No matches found.");
    }
}

int search_string_in_sector(unsigned char *buffer, const char *search_str, int *matches, bool is_hex) {
    unsigned char *search_bytes = (unsigned char *)malloc(17);
    if (!search_bytes) {
        display_error("Memory allocation failed for search_bytes");
        return 0;
    }
    memset(search_bytes, 0, 17);

    int search_len = 0;

    if (is_hex) {
        search_len = parse_hex_string(search_str, search_bytes);
    } else {
        search_len = strlen(search_str);
        memcpy(search_bytes, search_str, search_len);
    }

    if (search_len == 0 || search_len > 16) {
        display_error("Invalid search string length");
        free(search_bytes);
        return 0;
    }

    int match_count = 0;

    for (int i = 0; i <= SECTOR_SIZE - search_len; i++) {
        if (memcmp(buffer + i, search_bytes, search_len) == 0) {
            if (match_count < MAX_MATCHES) {
                matches[match_count++] = i;  // Сохраняем индекс совпадения
            }
        }
    }

    free(search_bytes);
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

void replace_string_at_cursor(sector_t *sectors, const char *input_str, bool is_hex) {
    int index = sectors->cursor_y * 16 + sectors->cursor_x;
    unsigned char *data = (unsigned char *)malloc(17);
    char *old_line = (char *)malloc(17);
    if (!data || !old_line) {
        display_error("Memory allocation failed");
        free(data);
        free(old_line);
        return;
    }
    memset(data, 0, 17);
    memset(old_line, 0, 17);

    int data_len = 0;

    if (is_hex) {
        data_len = parse_hex_string(input_str, data);
    } else {
        data_len = strlen(input_str);
        memcpy(data, input_str, data_len);
    }

    if (index + data_len > SECTOR_SIZE) {
        display_error("Insert string exceeds sector boundaries!");
        free(data);
        free(old_line);
        return;
    }

    for (int i = 0; i < data_len; i++) {
        old_line[i] = sectors->buffer[index + i];
    }

    // Сохраняем в undo стек
    save_undo_state(sectors, OP_LINE_REPLACE, index, 0, 0, old_line, (char *)data, data_len);

    // Заменяем байты
    for (int i = 0; i < data_len; i++) {
        sectors->buffer[index + i] = data[i];
    }

    redo_top = -1;
    clear_redo_stack();

    free(data);
    free(old_line);
}

void delete_bytes_from_cursor(sector_t *sectors, int count) {
    int index = sectors->cursor_y * 16 + sectors->cursor_x;

    if (index + count > SECTOR_SIZE) {
        display_error("Delete range exceeds sector boundaries!");
        return;
    }

    // Выделяем память для удаляемых байтов
    char *old_data = (char *)malloc(count);
    if (!old_data) {
        display_error("Memory allocation failed");
        return;
    }

    memcpy(old_data, &sectors->buffer[index], count);

    // Удаляем байты
    memset(&sectors->buffer[index], 0x00, count);

    // Сохраняем в undo
    save_undo_state(sectors, OP_LINE_REMOVE, index, 0, 0, old_data, NULL, count);

    free(old_data);

    // Сброс redo
    redo_top = -1;
    clear_redo_stack();
    display_message("Bytes deleted.");
}

void delete_string_at_cursor(sector_t *sectors, const char *search_str, bool is_hex) {
    unsigned char *search_bytes = (unsigned char *)malloc(17);
    char *old_data = (char *)malloc(17);
    if (!search_bytes || !old_data) {
        display_error("Memory allocation failed");
        free(search_bytes);
        free(old_data);
        return;
    }
    memset(search_bytes, 0, 17);
    memset(old_data, 0, 17);

    int search_len = 0;

    if (is_hex) {
        search_len = parse_hex_string(search_str, search_bytes);
    } else {
        search_len = strlen(search_str);
        memcpy(search_bytes, search_str, search_len);
    }

    if (search_len == 0 || search_len > 16) {
        display_error("Invalid search string length");
        free(search_bytes);
        free(old_data);
        return;
    }

    unsigned char *buffer = sectors->buffer;

    for (int i = 0; i <= SECTOR_SIZE - search_len; i++) {
        if (memcmp(buffer + i, search_bytes, search_len) == 0) {
            memcpy(old_data, buffer + i, search_len);

            // Обнуляем найденную строку
            memset(buffer + i, 0x00, search_len);

            // Сохраняем в undo
            save_undo_state(sectors, OP_LINE_REMOVE, i, 0, 0, old_data, NULL, search_len);

            redo_top = -1;

            display_message("String deleted.");
            free(search_bytes);
            free(old_data);
            return;
        }
    }

    display_message("String not found.");
    free(search_bytes);
    free(old_data);
}

int parse_hex_string(const char *hex_str, unsigned char *output) {
    int count = 0;
    while (*hex_str && *(hex_str + 1)) {
        if (count >= 16) break;  // максимум 16 байт
        
        // Пропускаем пробелы
        if (*hex_str == ' ') {
            hex_str++;
            continue;
        }

        // Считываем 2 символа как один байт
        char byte_str[3] = { hex_str[0], hex_str[1], '\0' };
        output[count++] = (unsigned char)strtoul(byte_str, NULL, 16);
        hex_str += 2;
    }
    return count; // Возвращает сколько байтов распарсено
}