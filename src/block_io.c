#include "block_io.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <ncurses.h>
// Функция вывода предупреждения и запроса через ncurses, возвращает 1 — согласие, 0 — отказ
int prompt_overwrite_ncurses(off_t sector_num) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);  // размер окна

    // Очистим нижнюю строку
    move(max_y - 1, 0);
    clrtoeol();

    mvprintw(max_y - 1, 0, "Warning: sector %ld modified by another process. Overwrite? (y/n): ", (long)sector_num);
    refresh();

    int c;
    while (1) {
        c = getch();
        if (c == 'y' || c == 'Y') {
            return 1;
        } else if (c == 'n' || c == 'N') {
            return 0;
        }
    }
}

uint8_t calc_checksum(const unsigned char *buf, size_t size) {
    uint8_t cs = 0;
    for (size_t i = 0; i < size; i++) {
        cs ^= buf[i];
    }
    return cs;
}

int lock_sector(int fd, off_t sector_num, short lock_type) {
    struct flock fl = {0};
    fl.l_type = lock_type;          // F_RDLCK (чтение), F_WRLCK (запись), F_UNLCK (снять)
    fl.l_whence = SEEK_SET;
    fl.l_start = sector_num * SECTOR_SIZE;
    fl.l_len = SECTOR_SIZE;
    fl.l_pid = 0;

    // Пробуем установить блокировку (неблокирующий вызов)
    if (fcntl(fd, F_SETLK, &fl) == -1) {
        // Не удалось заблокировать
        return -1;
    }
    return 0; // Успех
}

int unlock_sector(int fd, off_t sector_num) {
    struct flock fl = {0};
    fl.l_type = F_UNLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = sector_num * SECTOR_SIZE;
    fl.l_len = SECTOR_SIZE;

    if (fcntl(fd, F_SETLK, &fl) == -1) {
        perror("Failed to unlock sector");
        return -1;
    }
    return 0;
}

int open_device(const char *path) {
    int fd = open(path, O_RDWR);
    if (fd < 0) {
        perror("Failed to open device");
    }
    return fd;
}

int read_sector(sector_t *sector_data) {
    if (lseek(sector_data->fd, sector_data->sector * SECTOR_SIZE, SEEK_SET) < 0) {
        perror("lseek failed");
        return -1;
    }

    ssize_t bytes_read = read(sector_data->fd, sector_data->buffer, SECTOR_SIZE);
    if (bytes_read < 0) {
        perror("read failed");
        return -1;
    } else if (bytes_read != SECTOR_SIZE) {
        fprintf(stderr, "Partial read: expected %d bytes, got %zd bytes\n", SECTOR_SIZE, bytes_read);
        return -1;
    }
    sector_data->checksum = calc_checksum(sector_data->buffer, SECTOR_SIZE);
    return bytes_read;  // Успешно прочитали сектор
}


int write_sector(sector_t *sector_data) {
    unsigned char current_data[SECTOR_SIZE];

    if (lseek(sector_data->fd, sector_data->sector * SECTOR_SIZE, SEEK_SET) < 0) {
        perror("Failed to seek for verification");
        return -1;
    }
    ssize_t bytes_read = read(sector_data->fd, current_data, SECTOR_SIZE);
    if (bytes_read != SECTOR_SIZE) {
        perror("Failed to read sector for verification");
        return -1;
    }

    uint8_t current_checksum = calc_checksum(current_data, SECTOR_SIZE);

    if (current_checksum != sector_data->checksum) {
        // Используем ncurses для запроса
        if (!prompt_overwrite_ncurses(sector_data->sector)) {
            // Очистим строку предупреждения
            int max_y, max_x;
            getmaxyx(stdscr, max_y, max_x);
            move(max_y - 1, 0);
            clrtoeol();
            refresh();

            mvprintw(max_y - 1, 0, "Write aborted.");
            refresh();
            sleep(1);  // чтобы пользователь успел увидеть
            move(max_y - 1, 0);
            clrtoeol();
            refresh();
            return -1;
        }
        // Очистка строки предупреждения перед записью
        int max_y, max_x;
        getmaxyx(stdscr, max_y, max_x);
        move(max_y - 1, 0);
        clrtoeol();
        refresh();
    }

    if (lseek(sector_data->fd, sector_data->sector * SECTOR_SIZE, SEEK_SET) == -1) {
        perror("Failed to seek to sector for writing");
        return -1;
    }
    ssize_t bytes_written = write(sector_data->fd, sector_data->buffer, SECTOR_SIZE);
    if (bytes_written != SECTOR_SIZE) {
        perror("Failed to write sector");
        return -1;
    }

    sector_data->checksum = calc_checksum(sector_data->buffer, SECTOR_SIZE);

    return bytes_written;
}

void close_device(int* fd) {
    if(*fd != -1){
        close(*fd);
        *fd = -1;
    }
}