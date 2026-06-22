# TimeoutStrategy

Mô phỏng đề tài **Chiến lược Timeout vs Phát hiện Deadlock** bằng **C17**. Hệ thống xử lý deadlock theo thời gian logic (mỗi event = 1 time unit) với 3 chiến lược timeout: `kill`, `retry`, `rollback`, và đối chiếu với deadlock detection (Wait-For Graph) làm ground truth để đo false positive.

Định nghĩa cốt lõi: `waiting_time = current_time - request_time`. Khi `waiting_time >= TIMEOUT` thì kích hoạt xử lý.

## Cấu trúc

- `include/Models.h` — các kiểu dữ liệu chung: `Process`, `Resource`, `Event`, `PendingRequest`, `TimeoutRecord`, `SimulationMetrics`.
- `include/CSVParser.h`, `src/CSVParser.c` — đọc + validate dataset CSV, sort theo `time`.
- `include/SimulationEngine.h`, `src/SimulationEngine.c` — event loop, đồng hồ logic, cấp phát/nhả tài nguyên, lưu event gốc để replay (rollback), gom metrics.
- `include/DeadlockDetector.h`, `src/DeadlockDetector.c` — Wait-For Graph + DFS. `detectDeadlock()` (cycle toàn cục), `isInDeadlock(pid)` (process có nằm trong chu trình).
- `include/TimeoutManager.h`, `src/TimeoutManager.c` — tính `waiting_time`, xử lý `kill` / `retry` / `rollback`.
- `src/main.c` — CLI.
- `tests/test_timeout.c` — test tự động (harness tự viết, không framework ngoài).
- `tests/test_deadlock.c` — chương trình demo Wait-For Graph chạy tay.
- `data/sample_deadlock.csv` — dataset theo đề bài.
- `data/three_process_deadlock.csv` — dataset deadlock rõ hơn (duration dài hơn).
- `data/tc_1.csv` … `data/tc_4.csv` — các kịch bản dùng cho benchmark.

## Build

Yêu cầu: môi trường Linux/WSL với `make`, trình biên dịch C17 (`gcc` >= 8 hoặc `clang`) và `bash`.
Nếu chạy benchmark để vẽ biểu đồ, cài Python dependencies bằng một lệnh:

```bash
make deps
```

Project build theo **C17 + POSIX**. Makefile dùng `-D_POSIX_C_SOURCE=200809L` để khai báo các hàm POSIX như `strdup()` khi compile với `-std=c17`.

### Build và chạy ngay

```bash
make
make run RUN_ARGS="data/three_process_deadlock.csv 3 kill"
make benchmark
```

Trên Windows, chạy các lệnh trên trong WSL:

```bash
wsl
cd /mnt/c/Users/iyixn/Desktop/TimeoutStrategy
make
```

### Các target Makefile

```bash
make                         # build ./timeout_strategy
make run RUN_ARGS="..."      # build + chạy mô phỏng
make deps                    # tạo .venv và cài Python packages
make benchmark               # build + chạy benchmark
make test                    # build + chạy test
make clean                   # xóa binary build ra
make rebuild                 # clean rồi build lại
```

`make` không build sạch lại từ đầu nếu source không đổi. Muốn build sạch dùng `make rebuild` hoặc `make clean && make`.

Hoặc gcc trực tiếp:

```bash
gcc -std=c17 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -Iinclude src/*.c -o timeout_strategy
```

### CMake

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Build sạch với `-Wall -Wextra`, không warning.

## Chạy chương trình

Có thể chạy qua Makefile hoặc gọi `run.sh` trực tiếp. `run.sh` sẽ tự gọi `make -B` trước khi chạy.

### Chạy mô phỏng bình thường

Qua Makefile:

```bash
make run RUN_ARGS="data/three_process_deadlock.csv 3 kill"
make run RUN_ARGS="data/three_process_deadlock.csv 3 retry 1 3"
make run RUN_ARGS="data/three_process_deadlock.csv 3 rollback 1 3"
make run RUN_ARGS="data/three_process_deadlock.csv 3 --compare"
```

Hoặc qua `run.sh`:

```bash
chmod +x run.sh
./run.sh data/three_process_deadlock.csv 3 kill
./run.sh data/three_process_deadlock.csv 3 retry 1 3
./run.sh data/three_process_deadlock.csv 3 rollback 1 3
./run.sh data/three_process_deadlock.csv 3 --compare
```

Cú pháp:

```text
timeout_strategy <dataset.csv> [timeout] [kill|retry|rollback] [retry_delay] [max_retries|max_rollbacks] [-v|--verbose] [-c|--compare]
```

- `timeout` — ngưỡng TIMEOUT (`>= 1`, mặc định 5).
- `strategy` — `kill` | `retry` | `rollback` (mặc định `kill`).
- `retry_delay` — số time unit chờ trước khi xin lại (mặc định 1).
- `max_retries` / `max_rollbacks` — số lần tối đa trước khi leo thang sang kill (mặc định 3).
- `-v` / `--verbose` — in log từng sự kiện theo time unit.
- `-c` / `--compare` — chạy cả 3 chiến lược, in bảng so sánh metrics một lần.

### Log từng sự kiện (`-v`)

```bash
./run.sh data/three_process_deadlock.csv 3 kill -v
```

```text
Time 0: P1 requests R1 -> Granted
Time 3: P1 requests R2 -> Blocked (held by P2)
Time 6: P1 TIMEOUT (waited 3, deadlock) -> Killed
Time 6: Deadlock resolved (cycle broken)
Time 7: P5 -> Completed
```

### So sánh 3 chiến lược (`-c`)

```bash
./run.sh data/three_process_deadlock.csv 3 --compare
```

```text
strategy   completed  killed retries rollbacks  resolved      fp  throughput   fp_rate
--------------------------------------------------------------------------------------
kill               4       1       0         0         1       0       0.800     0.000
retry              5       0       5         0         1       4       1.000     0.800
rollback           5       0       0         1         1       0       1.000     0.000
```

## Benchmark

Benchmark có thể chạy qua Makefile hoặc `run.sh`. Script tự export kết quả ra CSV rồi vẽ biểu đồ.

### Chạy benchmark

```bash
make deps
make benchmark
chmod +x run.sh
./run.sh benchmark
```

Kết quả được lưu vào `benchmark/benchmark_results.csv` với các cột:

- `testcase` — tên file testcase
- `timeout` — giá trị TIMEOUT đang kiểm tra
- `strategy` — kill / retry / rollback
- `killed` — số process bị kill
- `resolved` — số deadlock được phát hiện và giải quyết
- `throughput` — tỷ lệ process hoàn thành
- `fp_rate` — false positive rate

Biểu đồ được tạo bởi `benchmark/plot_benchmark.py` và lưu trong thư mục `benchmark/` dưới dạng PNG.

## Test

```bash
make test
```

Hoặc qua CMake:

```bash
ctest --test-dir build --output-on-failure
```

Hoặc build trực tiếp:

```bash
gcc -std=c17 -D_POSIX_C_SOURCE=200809L -Iinclude tests/test_timeout.c src/CSVParser.c src/DeadlockDetector.c src/SimulationEngine.c src/TimeoutManager.c -o run_tests
./run_tests
```

## Cơ chế timeout

- Khi process bị block, engine tạo `PendingRequest` có `requestTime`.
- Mỗi time unit, `TimeoutManager_checkTimeouts()` tính `waitingTime = currentTime - requestTime`.
- Nếu `waitingTime >= TIMEOUT`, kích hoạt strategy:
  - **kill** — terminate process, giải phóng toàn bộ resource, xóa pending của process.
  - **retry** — rút request hiện tại, chờ `retryDelay`, xin lại; vượt `maxRetries` thì leo thang kill.
  - **rollback** — thu hồi toàn bộ resource, đưa process về trạng thái ban đầu (`New`) và replay toàn bộ event; vượt `maxRollbacks` thì leo thang kill.
- Trước khi xử lý, gọi `DeadlockDetector_isInDeadlock(pid)` để đánh dấu timeout là true positive hay false positive.

Metrics: `killed_processes`, `retry_events`, `rollback_events`, `deadlock_resolved`, `throughput`, `false_positives`, `false_positive_rate`.
