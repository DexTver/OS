#define _GNU_SOURCE
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <aio.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>
#include <chrono>
#include <string>
#include <iostream>
#include <iomanip>

struct aio_operation {
    struct aiocb aio;
    char *buffer;
    size_t buffer_size;
    int write_operation;
    int slot_index;
};

static int g_fd_in = -1;
static int g_fd_out = -1;
static off_t g_file_size = 0;
static off_t g_bytes_read = 0;
static off_t g_bytes_written = 0;
static size_t g_block_size = 0;
static int g_concurrency = 1;
static bool g_copy_done = false;

static aio_operation *g_slots = nullptr;
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

static std::string centerStr(const std::string &str, int width) {
    if ((int) str.size() >= width) {
        return str;
    }
    int left = (width - (int) str.size()) / 2;
    int right = width - left - (int) str.size();
    return std::string(left, ' ') + str + std::string(right, ' ');
}

static bool schedule_read(aio_operation &op);

static bool schedule_write(aio_operation &op, ssize_t bytes_to_write, off_t offset);

static void aio_completion_handler(sigval_t sigval) {
    aio_operation *op = (aio_operation *) sigval.sival_ptr;
    if (!op) return;

    if (op->write_operation == 0) {
        ssize_t rbytes = aio_return(&op->aio);
        if (rbytes < 0) {
            perror("aio_return (read)");
            delete[] op->buffer;
            op->buffer = nullptr;
            return;
        }

        if (rbytes == 0) {
            delete[] op->buffer;
            op->buffer = nullptr;
            return;
        }

        off_t write_offset = op->aio.aio_offset;
        if (!schedule_write(*op, rbytes, write_offset)) {
            perror("schedule_write");

            delete[] op->buffer;
            op->buffer = nullptr;
        }
    } else {
        ssize_t wbytes = aio_return(&op->aio);
        if (wbytes < 0) {
            perror("aio_return (write)");
        } else {
            pthread_mutex_lock(&g_mutex);
            g_bytes_written += wbytes;
            if (g_bytes_written >= g_file_size) {
                g_copy_done = true;
            }
            pthread_mutex_unlock(&g_mutex);
        }


        delete[] op->buffer;
        op->buffer = nullptr;


        pthread_mutex_lock(&g_mutex);
        bool done = g_copy_done;
        pthread_mutex_unlock(&g_mutex);

        if (!done) {
            (void) schedule_read(*op);
        }
    }
}


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


    memset(&op.aio, 0, sizeof(op.aio));
    op.write_operation = 0;
    op.buffer_size = to_read;
    op.buffer = new char[to_read];

    op.aio.aio_fildes = g_fd_in;
    op.aio.aio_buf = op.buffer;
    op.aio.aio_nbytes = to_read;
    op.aio.aio_offset = offset;


    op.aio.aio_sigevent.sigev_notify = SIGEV_THREAD;
    op.aio.aio_sigevent.sigev_notify_function = aio_completion_handler;
    op.aio.aio_sigevent.sigev_notify_attributes = nullptr;
    op.aio.aio_sigevent.sigev_value.sival_ptr = (void *) &op;


    if (aio_read(&op.aio) < 0) {
        perror("aio_read");
        delete[] op.buffer;
        op.buffer = nullptr;
        return false;
    }

    return true;
}

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

int main(int argc, char *argv[]) {
    if (argc < 5) {
        printf("Usage: %s <source_file> <dest_file> <block_size> <concurrency>\n", argv[0]);
        return 0;
    }

    const char *source_path = argv[1];
    const char *dest_path = argv[2];
    g_block_size = (size_t) atoll(argv[3]);
    g_concurrency = atoi(argv[4]);

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

    struct stat st;
    if (fstat(g_fd_in, &st) < 0) {
        perror("fstat");
        close(g_fd_in);
        close(g_fd_out);
        exit(EXIT_FAILURE);
    }
    g_file_size = st.st_size;

    auto start_time = std::chrono::high_resolution_clock::now();

    g_slots = new aio_operation[g_concurrency];
    for (int i = 0; i < g_concurrency; i++) {
        memset(&g_slots[i], 0, sizeof(aio_operation));
        g_slots[i].slot_index = i;
    }

    for (int i = 0; i < g_concurrency; i++) {
        if (!schedule_read(g_slots[i])) {
            break;
        }
    }

    while (true) {
        pthread_mutex_lock(&g_mutex);
        bool done = g_copy_done;
        pthread_mutex_unlock(&g_mutex);
        if (done) break;
        usleep(10000);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    double duration_sec = std::chrono::duration<double>(end_time - start_time).count();

    close(g_fd_in);
    close(g_fd_out);

    delete[] g_slots;
    g_slots = nullptr;

    double speed_mbs = 0.0;
    if (duration_sec > 0.0) {
        speed_mbs = (double) g_bytes_written / (1024.0 * 1024.0) / duration_sec;
    }

    std::stringstream durationStream, speedStream;
    durationStream << std::fixed << std::setprecision(2) << duration_sec;
    speedStream << std::fixed << std::setprecision(2) << speed_mbs;
    std::string duration_str = durationStream.str();
    std::string speed_str = speedStream.str();

    std::cout << std::string(103, '-') << "\n";
    std::cout
            << "|" << centerStr("File size (B)", 16)
            << "|" << centerStr("Block size (B)", 16)
            << "|" << centerStr("Threads", 16)
            << "|" << centerStr("Copied (B)", 16)
            << "|" << centerStr("Time (s)", 16)
            << "|" << centerStr("Speed (MB/s)", 16)
            << "|\n";
    std::cout << std::string(103, '-') << "\n";

    std::cout
            << "|" << centerStr(std::to_string((long long) g_file_size), 16)
            << "|" << centerStr(std::to_string((long long) g_block_size), 16)
            << "|" << centerStr(std::to_string((long long) g_concurrency), 16)
            << "|" << centerStr(std::to_string((long long) g_bytes_written), 16)
            << "|" << centerStr(duration_str, 16)
            << "|" << centerStr(speed_str, 16)
            << "|\n";

    std::cout << std::string(103, '-') << "\n";

    return 0;
}
