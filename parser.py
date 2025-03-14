import subprocess
import matplotlib.pyplot as plt

INPUT_FILE = "input.mp4"
OUTPUT_FILE = "output.mp4"
MIN_EXP = 20
MAX_EXP = 25
FIXED_THREAD_COUNT = 1
MIN_THREAD_COUNT = 1
MAX_THREAD_COUNT = 8
FIXED_BLOCK_SIZE = 1048576
NUM_EXPERIMENTS = 5


def measure_speed(block_size, threads):
    command = ["./main", INPUT_FILE, OUTPUT_FILE, str(block_size), str(threads)]
    try:
        output = subprocess.check_output(command, stderr=subprocess.STDOUT).decode("utf-8")
    except subprocess.CalledProcessError:
        return 0.0
    speed = 0.0
    for line in reversed(output.splitlines()):
        if line.strip().startswith("|"):
            parts = line.split("|")
            try:
                speed = float(parts[-2].strip())
            except:
                speed = 0.0
            break
    return speed


def main():
    block_sizes = [2 ** i for i in range(MIN_EXP, MAX_EXP + 1)]
    avg_speeds_exp1 = []
    for block in block_sizes:
        speeds = []
        for _ in range(NUM_EXPERIMENTS):
            speeds.append(measure_speed(block, FIXED_THREAD_COUNT))
        avg_speeds_exp1.append(sum(speeds) / len(speeds))
    plt.figure()
    plt.plot(block_sizes, avg_speeds_exp1, marker='o')
    plt.xscale('log', base=2)
    plt.xlabel("Размер блока (байт)")
    plt.ylabel("Скорость (МБ/с)")
    plt.title("Эксперимент 1: фиксированное число потоков")
    plt.grid(True)
    plt.savefig("eksperiment1.png", dpi=200)

    thread_counts = range(MIN_THREAD_COUNT, MAX_THREAD_COUNT + 1)
    avg_speeds_exp2 = []
    for threads in thread_counts:
        speeds = []
        for _ in range(NUM_EXPERIMENTS):
            speeds.append(measure_speed(FIXED_BLOCK_SIZE, threads))
        avg_speeds_exp2.append(sum(speeds) / len(speeds))
    plt.figure()
    plt.plot(thread_counts, avg_speeds_exp2, marker='o')
    plt.xlabel("Число потоков")
    plt.ylabel("Скорость (МБ/с)")
    plt.title("Эксперимент 2: фиксированный размер блока")
    plt.grid(True)
    plt.savefig("eksperiment2.png", dpi=200)


if __name__ == "__main__":
    main()
