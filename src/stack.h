#ifndef STACK_H
#define STACK_H
#define UNDO_STACK_SIZE 20
#include"variables.h"
#include"ui.h"
typedef enum {
    OP_BYTE_CHANGE,   
    OP_LINE_REMOVE,   
    OP_LINE_REPLACE   
} operation_t;
typedef struct {
    operation_t op_type;     // Тип операции
    int index;               // Индекс изменения (байт или строка)
    unsigned char old_value; // Старое значение байта (если операция OP_BYTE_CHANGE)
    unsigned char new_value; // Новое значение байта (если операция OP_BYTE_CHANGE)
    char *old_line;          // Динамическая строка старого содержимого
    char *new_line;          // Динамическая строка нового содержимого
    int data_length;         // Размер строки
} undo_item_t;
void clear_all_stacks();
bool redo(sector_t *sectors);
bool undo(sector_t *sectors);
void save_undo_state(sector_t *sectors, operation_t op_type, int index,
    unsigned char old_value, unsigned char new_value, const char *old_line, const char *new_line, int data_length);
extern undo_item_t undo_stack[UNDO_STACK_SIZE];
extern undo_item_t redo_stack[UNDO_STACK_SIZE];
extern int undo_top;
extern int redo_top;
#endif