#define _GNU_SOURCE

#include <cstdio>       // printf, perror
#include <cstdlib>      // exit, EXIT_FAILURE
#include <cstring>      // memset, strerror
#include <aio.h>        // aio_read, aio_write, etc.
#include <fcntl.h>      // open, O_RDONLY | O_NONBLOCK, etc.
#include <unistd.h>     // close
#include <signal.h>     // sigevent, SIGEV_THREAD
#include <sys/stat.h>   // fstat
#include <sys/types.h>  // off_t
#include <pthread.h>    // pthread_mutex_t, pthread_mutex_lock, etc.
#include <chrono>       // для замера времени
#include <inttypes.h>   // PRId64

// ---------------------------------------------------------------
// Структура, описывающая одну асинхронную операцию чтения/записи
// ---------------------------------------------------------------
struct aio_operation {
    struct aiocb aio;       // Управляющая структура для aio
    char *buffer;           // Буфер для данных
    size_t buffer_size;     // Размер буфера (байтов)
    int write_operation;    // Флаг: 0=чтение, 1=запись
    int slot_index;         // Индекс данного слота (для отладки)
};

// ---------------------------------------------------------------
// Глобальные переменные и мьютекс
// ---------------------------------------------------------------
static int g_fd_in = -1;            // Дескриптор исходного файла
static int g_fd_out = -1;           // Дескриптор выходного файла
static off_t g_file_size = 0;       // Общий размер исходного файла
static off_t g_bytes_read = 0;      // Сколько байт запланировано к чтению
static off_t g_bytes_written = 0;   // Сколько байт записано
static size_t g_block_size = 0;     // Размер блока копирования
static int g_concurrency = 1;       // Число слотов (одновременных операций)
static bool g_copy_done = false;    // Признак завершения копирования

static aio_operation *g_slots = nullptr;
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

// ---------------------------------------------------------------
// ПРОТОТИПЫ вспомогательных функций
// ---------------------------------------------------------------
static bool schedule_read(aio_operation &op);

static bool schedule_write(aio_operation &op, ssize_t bytes_to_write, off_t offset);

// ---------------------------------------------------------------
// ГЛАВНАЯ функция-обработчик AIO, вызываемая по SIGEV_THREAD
// ---------------------------------------------------------------
static void aio_completion_handler(sigval_t sigval) {
    aio_operation *op = (aio_operation *) sigval.sival_ptr;
    if (!op) return;

    if (op->write_operation == 0) {
        // Завершилась операция чтения
        ssize_t rbytes = aio_return(&op->aio);
        if (rbytes < 0) {
            perror("aio_return (read)");
            delete[] op->buffer;
            op->buffer = nullptr;
            return;
        }

        // Если rbytes == 0 -> EOF, освобождаем буфер
        if (rbytes == 0) {
            delete[] op->buffer;
            op->buffer = nullptr;
            return;
        }

        // Успешно прочитано rbytes байт. Запланируем запись
        off_t write_offset = op->aio.aio_offset;
        if (!schedule_write(*op, rbytes, write_offset)) {
            perror("schedule_write");
            // Запись не стартовала, освободим буфер
            delete[] op->buffer;
            op->buffer = nullptr;
        }

    } else {
        // Завершилась операция записи
        ssize_t wbytes = aio_return(&op->aio);
        if (wbytes < 0) {
            perror("aio_return (write)");
        } else {
            // Обновим счётчик записанных байт
            pthread_mutex_lock(&g_mutex);
            g_bytes_written += wbytes;
            if (g_bytes_written >= g_file_size) {
                g_copy_done = true;
            }
            pthread_mutex_unlock(&g_mutex);
        }

        // Освободим буфер
        delete[] op->buffer;
        op->buffer = nullptr;

        // Попробуем запланировать новое чтение, если копирование не завершено
        pthread_mutex_lock(&g_mutex);
        bool done = g_copy_done;
        pthread_mutex_unlock(&g_mutex);

        if (!done) {
            (void) schedule_read(*op);
        }
    }
}

// ---------------------------------------------------------------
// Планирование асинхронного чтения
// ---------------------------------------------------------------
static bool schedule_read(aio_operation &op) {
    pthread_mutex_lock(&g_mutex);

    if (g_bytes_read >= g_file_size) {
        pthread_mutex_unlock(&g_mutex);
        return false;
    }

    off_t remaining = g_file_size - g_bytes_read;
    size_t to_read = (remaining < (off_t) g_block_size) ? (size_t) remaining : g_block_size;

    off_t offset = g_bytes_read;
    g_bytes_read += to_read;

    pthread_mutex_unlock(&g_mutex);

    // Настройка структуры
    memset(&op.aio, 0, sizeof(op.aio));
    op.write_operation = 0;
    op.buffer_size = to_read;
    op.buffer = new char[to_read];

    op.aio.aio_fildes = g_fd_in;
    op.aio.aio_buf = op.buffer;
    op.aio.aio_nbytes = to_read;
    op.aio.aio_offset = offset;

    // Указываем обработчик завершения
    op.aio.aio_sigevent.sigev_notify = SIGEV_THREAD;
    op.aio.aio_sigevent.sigev_notify_function = aio_completion_handler;
    op.aio.aio_sigevent.sigev_notify_attributes = nullptr;
    op.aio.aio_sigevent.sigev_value.sival_ptr = (void *) &op;

    // Запускаем чтение
    if (aio_read(&op.aio) < 0) {
        perror("aio_read");
        delete[] op.buffer;
        op.buffer = nullptr;
        return false;
    }

    return true;
}

// ---------------------------------------------------------------
// Планирование асинхронной записи
// ---------------------------------------------------------------
static bool schedule_write(aio_operation &op, ssize_t bytes_to_write, off_t offset) {
    op.write_operation = 1;
    memset(&op.aio, 0, sizeof(op.aio));
    op.aio.aio_fildes = g_fd_out;
    op.aio.aio_buf = op.buffer;
    op.aio.aio_nbytes = bytes_to_write;
    op.aio.aio_offset = offset;

    op.aio.aio_sigevent.sigev_notify = SIGEV_THREAD;
    op.aio.aio_sigevent.sigev_notify_function = aio_completion_handler;
    op.aio.aio_sigevent.sigev_notify_attributes = nullptr;
    op.aio.aio_sigevent.sigev_value.sival_ptr = (void *) &op;

    if (aio_write(&op.aio) < 0) {
        perror("aio_write");
        return false;
    }
    return true;
}

// ---------------------------------------------------------------
// main
// ---------------------------------------------------------------
int main(int argc, char *argv[]) {
    if (argc < 5) {
        printf("Usage: %s <source_file> <dest_file> <block_size> <concurrency>\n", argv[0]);
        return 0;
    }

    const char *source_path = argv[1];
    const char *dest_path = argv[2];
    g_block_size = (size_t) atoll(argv[3]);
    g_concurrency = atoi(argv[4]);

    // Открываем файлы
    g_fd_in = open(source_path, O_RDONLY | O_NONBLOCK);
    if (g_fd_in < 0) {
        perror("open source");
        exit(EXIT_FAILURE);
    }

    g_fd_out = open(dest_path, O_CREAT | O_WRONLY | O_TRUNC | O_NONBLOCK, 0666);
    if (g_fd_out < 0) {
        perror("open dest");
        close(g_fd_in);
        exit(EXIT_FAILURE);
    }

    // Узнаём размер исходного файла
    struct stat st;
    if (fstat(g_fd_in, &st) < 0) {
        perror("fstat");
        close(g_fd_in);
        close(g_fd_out);
        exit(EXIT_FAILURE);
    }
    g_file_size = st.st_size;

    printf("Copying from '%s' to '%s'\n", source_path, dest_path);
    printf("File size = %" PRId64 " bytes\n", (int64_t) g_file_size);
    printf("Block size = %zu, concurrency = %d\n", g_block_size, g_concurrency);

    auto start_time = std::chrono::high_resolution_clock::now();

    // Создаём массив слотов
    g_slots = new aio_operation[g_concurrency];
    for (int i = 0; i < g_concurrency; i++) {
        memset(&g_slots[i], 0, sizeof(aio_operation));
        g_slots[i].slot_index = i;
    }

    // Запускаем до g_concurrency операций чтения
    for (int i = 0; i < g_concurrency; i++) {
        if (!schedule_read(g_slots[i])) {
            // Возможно, файл слишком мал - дальше не читаем
            break;
        }
    }

    // Ждём, пока все байты не будут записаны
    while (true) {
        pthread_mutex_lock(&g_mutex);
        bool done = g_copy_done;
        pthread_mutex_unlock(&g_mutex);

        if (done) break;
        sleep(1);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    double duration_sec = std::chrono::duration<double>(end_time - start_time).count();

    // Закрываем файлы
    close(g_fd_in);
    close(g_fd_out);

    // Освобождаем массив слотов
    delete[] g_slots;
    g_slots = nullptr;

    printf("Copied %" PRId64 " bytes in %.6f seconds\n", (int64_t) g_bytes_written, duration_sec);
    if (duration_sec > 0.0) {
        double mbps = (double) g_bytes_written / (1024.0 * 1024.0) / duration_sec;
        printf("Speed: %.2f MB/s\n", mbps);
    }

    return 0;
}
