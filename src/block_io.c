#include "block_io.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>


int open_device(const char *path) {
    return open(path, O_RDWR);
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

    return bytes_read;  // Успешно прочитали сектор
}


int write_sector(sector_t *sector_data) {
    // Позиционируем указатель на начало нужного сектора
    if (lseek(sector_data->fd, sector_data->sector * SECTOR_SIZE, SEEK_SET) == -1) {
        perror("Failed to seek to sector");
        return -1;
    }

    // Пытаемся записать данные из буфера
    ssize_t bytes_written = write(sector_data->fd, sector_data->buffer, SECTOR_SIZE);
    if (bytes_written == -1) {
        perror("Failed to write sector");
        return -1;
    }

    // Проверяем, что записали все байты
    if (bytes_written != SECTOR_SIZE) {
        fprintf(stderr, "Partial write: expected %d bytes, wrote %zd bytes\n", SECTOR_SIZE, bytes_written);
        return -1;
    }

    return bytes_written;  // Успех
}


void close_device(int fd) {
    close(fd);
}
