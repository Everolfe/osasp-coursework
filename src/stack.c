#include "stack.h"  // Подключаем заголовочный файл

undo_item_t undo_stack[UNDO_STACK_SIZE];
undo_item_t redo_stack[UNDO_STACK_SIZE];

int undo_top = -1;
int redo_top = -1;

// Освобождение памяти одного элемента
void free_undo_item(undo_item_t *item) {
    if (item->old_line) {
        free(item->old_line);
        item->old_line = NULL;
    }
    if (item->new_line) {
        free(item->new_line);
        item->new_line = NULL;
    }
}

void save_undo_state(sector_t *sectors, operation_t op_type, int index,
    unsigned char old_value, unsigned char new_value, const char *old_line, const char *new_line, int data_length) {

    if (undo_top < UNDO_STACK_SIZE - 1) {
        undo_top++;
    } else {
        // Освобождаем память старого элемента перед сдвигом
        free(undo_stack[0].old_line);
        free(undo_stack[0].new_line);
        for (int i = 0; i < UNDO_STACK_SIZE - 1; i++) {
            undo_stack[i] = undo_stack[i + 1];
        }
        undo_top = UNDO_STACK_SIZE - 1;
    }

    undo_item_t *item = &undo_stack[undo_top];
    item->op_type = op_type;
    item->index = index;
    item->old_value = old_value;
    item->new_value = new_value;
    item->data_length = data_length;

    if (old_line && data_length > 0) {
        item->old_line = (char *)malloc(data_length);
        memcpy(item->old_line, old_line, data_length);
    } else {
        item->old_line = NULL;
    }

    if (new_line && data_length > 0) {
        item->new_line = (char *)malloc(data_length);
        memcpy(item->new_line, new_line, data_length);
    } else {
        item->new_line = NULL;
    }
}



bool undo(sector_t *sectors) {
    if (undo_top >= 0) {
        undo_item_t *item = &undo_stack[undo_top];
        switch (item->op_type) {
            case OP_BYTE_CHANGE:
                sectors->buffer[item->index] = item->old_value;
                break;
            case OP_LINE_REMOVE:
            case OP_LINE_REPLACE:
                if (item->old_line) {
                    memcpy(&sectors->buffer[item->index], item->old_line, item->data_length);
                }
                break;
        }

        // Сохраняем в redo стек (глубокое копирование!)
        if (redo_top < UNDO_STACK_SIZE - 1) {
            redo_top++;
            free_undo_item(&redo_stack[redo_top]);
            undo_item_t *redo_item = &redo_stack[redo_top];

            redo_item->op_type = item->op_type;
            redo_item->index = item->index;
            redo_item->old_value = item->old_value;
            redo_item->new_value = item->new_value;
            redo_item->data_length = item->data_length;

            if (item->old_line && item->data_length > 0) {
                redo_item->old_line = malloc(item->data_length);
                memcpy(redo_item->old_line, item->old_line, item->data_length);
            } else {
                redo_item->old_line = NULL;
            }

            if (item->new_line && item->data_length > 0) {
                redo_item->new_line = malloc(item->data_length);
                memcpy(redo_item->new_line, item->new_line, item->data_length);
            } else {
                redo_item->new_line = NULL;
            }
        }

        // Очистка и переход
        free_undo_item(item);
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
            case OP_LINE_REPLACE:
                if (item->new_line) {
                    memcpy(&sectors->buffer[item->index], item->new_line, item->data_length);
                }
                break;
        }

        // Сохраняем обратно в undo стек (глубокое копирование!)
        if (undo_top < UNDO_STACK_SIZE - 1) {
            undo_top++;
            free_undo_item(&undo_stack[undo_top]);
            undo_item_t *undo_item = &undo_stack[undo_top];

            undo_item->op_type = item->op_type;
            undo_item->index = item->index;
            undo_item->old_value = item->old_value;
            undo_item->new_value = item->new_value;
            undo_item->data_length = item->data_length;

            if (item->old_line && item->data_length > 0) {
                undo_item->old_line = malloc(item->data_length);
                memcpy(undo_item->old_line, item->old_line, item->data_length);
            } else {
                undo_item->old_line = NULL;
            }

            if (item->new_line && item->data_length > 0) {
                undo_item->new_line = malloc(item->data_length);
                memcpy(undo_item->new_line, item->new_line, item->data_length);
            } else {
                undo_item->new_line = NULL;
            }
        }

        // Очистка и переход
        free_undo_item(item);
        redo_top--;
        return true;
    } else {
        display_message("No more redo actions available.");
        return false;
    }
}

// Очистка undo стека
void clear_undo_stack() {
    for (int i = 0; i <= undo_top; i++) {
        free_undo_item(&undo_stack[i]);
    }
    undo_top = -1;
}

// Очистка redo стека
void clear_redo_stack() {
    for (int i = 0; i <= redo_top; i++) {
        free_undo_item(&redo_stack[i]);
    }
    redo_top = -1;
}

// Очистка обоих стеков сразу
void clear_all_stacks() {
    clear_undo_stack();
    clear_redo_stack();
}
