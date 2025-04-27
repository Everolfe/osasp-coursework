#include "stack.h"  // Подключаем заголовочный файл

undo_item_t undo_stack[UNDO_STACK_SIZE];
undo_item_t redo_stack[UNDO_STACK_SIZE];

int undo_top = -1;
int redo_top = -1;

void insert_line(sector_t *sectors, int line_index, const char *line) {
    int start = line_index * 16;
    if (start >= SECTOR_SIZE) return;

    for (int i = 0; i < 16 && start + i < SECTOR_SIZE; i++) {
        sectors->buffer[start + i] = line[i];
    }
}

void remove_line(sector_t *sectors, int line_index) {
    int start = line_index * 16;
    if (start >= SECTOR_SIZE) return;

    for (int i = 0; i < 16 && start + i < SECTOR_SIZE; i++) {
        sectors->buffer[start + i] = 0x00;  // Обнуляем строку
    }
}

void replace_line(sector_t *sectors, int line_index, const char *new_line) {
    insert_line(sectors, line_index, new_line);  // Просто перезаписываем строку
}

char* get_line_at_cursor(sector_t *sectors) {
    static char line_buffer[17];  // 16 байт строки + 1 нулевой символ для безопасности
    memset(line_buffer, 0, sizeof(line_buffer));

    int line_start = sectors->cursor_y * 16;
    if (line_start >= SECTOR_SIZE) {
        return line_buffer; // Защита от выхода за границы
    }

    int line_end = line_start + 16;
    if (line_end > SECTOR_SIZE) {
        line_end = SECTOR_SIZE;
    }

    for (int i = line_start, j = 0; i < line_end; i++, j++) {
        line_buffer[j] = sectors->buffer[i];
    }

    return line_buffer;
}

void save_undo_state(sector_t *sectors, operation_t op_type, int index,
    unsigned char old_value, unsigned char new_value, const char *old_line, const char *new_line, int data_length) {
    if (undo_top < UNDO_STACK_SIZE - 1) {
        undo_top++;
    } else {
        for (int i = 0; i < UNDO_STACK_SIZE - 1; i++) {
            undo_stack[i] = undo_stack[i + 1];
        }
    }
   undo_item_t *item = &undo_stack[undo_top];
    item->op_type = op_type;
    item->index = index;
    item->old_value = old_value;
    item->new_value = new_value;
    if (old_line) {
        strcpy(item->old_line, old_line);
    }
    if (new_line) {
        strcpy(item->new_line, new_line);
    }
   item->data_length = data_length;  // Сохраняем длину
}


bool undo(sector_t *sectors) {
    if (undo_top >= 0) {
        undo_item_t *item = &undo_stack[undo_top];
        switch (item->op_type) {
            case OP_BYTE_CHANGE:
                sectors->buffer[item->index] = item->old_value;
                break;
            case OP_LINE_REMOVE:
                // Восстановление строки
                memcpy(&sectors->buffer[item->index], item->old_line, item->data_length);
                break;
            case OP_LINE_REPLACE:
                // Восстановление старой строки
                memcpy(&sectors->buffer[item->index], item->old_line, item->data_length);
                break;
        }
        // Перемещаем в стек redo
        if (redo_top < UNDO_STACK_SIZE - 1) {
            redo_top++;
            redo_stack[redo_top] = *item;
        }
        undo_top--;
        return true;
    } else {
        display_message("No more undo actions available.");
        return false;
    }
}

bool redo(sector_t *sectors) {
    if (redo_top >= 0) {
        undo_item_t *item = &redo_stack[redo_top];
        switch (item->op_type) {
            case OP_BYTE_CHANGE:
                sectors->buffer[item->index] = item->new_value;
                break;
            case OP_LINE_REMOVE:
                memcpy(&sectors->buffer[item->index], item->new_line, item->data_length);
                break;
            case OP_LINE_REPLACE:
                memcpy(&sectors->buffer[item->index], item->new_line, item->data_length);
                break;
        }
        // Перемещаем в стек undo
        if (undo_top < UNDO_STACK_SIZE - 1) {
            undo_top++;
            undo_stack[undo_top] = *item;
        }
        redo_top--;
        return true;
    } else {
        display_message("No more redo actions available.");
        return false;
    }
}

