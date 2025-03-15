import subprocess
import matplotlib.pyplot as plt

INPUT_FILE = "input.mkv"
OUTPUT_FILE = "output.mkv"
MIN_EXP = 11
MAX_EXP = 30
FIXED_THREAD_COUNT = 1
MIN_THREAD_COUNT = 1
MAX_THREAD_COUNT = 20
FIXED_BLOCK_SIZE = 32768
NUM_EXPERIMENTS = 1

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
    print(f"Начало эксперимента 1: фиксированное число потоков ({FIXED_THREAD_COUNT})")
    block_sizes = [2 ** i for i in range(MIN_EXP, MAX_EXP + 1)]
    avg_speeds_exp1 = []
    total_blocks = len(block_sizes)
    for idx, block in enumerate(block_sizes, start=1):
        print(f"Эксперимент 1 ({idx}/{total_blocks}): обработка размера блока {block} байт")
        speeds = []
        for exp in range(NUM_EXPERIMENTS):
            print(f"  Прогон {exp+1}/{NUM_EXPERIMENTS}")
            speeds.append(measure_speed(block, FIXED_THREAD_COUNT))
        avg_speed = sum(speeds) / len(speeds)
        avg_speeds_exp1.append(avg_speed)
        print(f"  Средняя скорость: {avg_speed:.2f} МБ/с")
    plt.figure()
    plt.plot(block_sizes, avg_speeds_exp1, marker='o')
    plt.xscale('log', base=2)
    plt.xlabel("Размер блока (байт)")
    plt.ylabel("Скорость (МБ/с)")
    plt.title(f"Фиксированное число потоков ({FIXED_THREAD_COUNT})")
    plt.grid(True)
    plt.savefig("eksperiment1.png", dpi=200)

    print(f"Начало эксперимента 2: фиксированный размер блока ({FIXED_BLOCK_SIZE} байт)")
    thread_counts = range(MIN_THREAD_COUNT, MAX_THREAD_COUNT + 1)
    avg_speeds_exp2 = []
    total_threads = MAX_THREAD_COUNT - MIN_THREAD_COUNT + 1
    for idx, threads in enumerate(thread_counts, start=1):
        print(f"Эксперимент 2 ({idx}/{total_threads}): обработка числа потоков {threads}")
        speeds = []
        for exp in range(NUM_EXPERIMENTS):
            print(f"  Прогон {exp+1}/{NUM_EXPERIMENTS}")
            speeds.append(measure_speed(FIXED_BLOCK_SIZE, threads))
        avg_speed = sum(speeds) / len(speeds)
        avg_speeds_exp2.append(avg_speed)
        print(f"  Средняя скорость: {avg_speed:.2f} МБ/с")
    plt.figure()
    plt.plot(range(MIN_THREAD_COUNT, MAX_THREAD_COUNT + 1), avg_speeds_exp2, marker='o')
    plt.xlabel("Число потоков")
    plt.ylabel("Скорость (МБ/с)")
    plt.title(f"Фиксированный размер блока ({FIXED_BLOCK_SIZE / 1024} КБ)")
    plt.grid(True)
    plt.savefig("eksperiment2.png", dpi=200)

if __name__ == "__main__":
    main()
