# TimeoutStrategy

Mô phỏng đề tài **Chiến lược Timeout vs Phát hiện Deadlock** bằng **C11**. Hệ thống xử lý deadlock theo thời gian logic (mỗi event = 1 time unit) với 3 chiến lược timeout: `kill`, `retry`, `rollback`, và đối chiếu với deadlock detection (Wait-For Graph) làm ground truth để đo false positive.

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

Yêu cầu: trình biên dịch C11 (`gcc` >= 7 hoặc `clang`), tùy chọn `cmake`/`make`. Code thuần chuẩn C11, không phụ thuộc API riêng OS, chạy trên Linux, macOS và Windows.

### Build và chạy ngay

```bash
chmod +x run.sh
./run.sh benchmark                                  # chạy benchmark
./run.sh data/three_process_deadlock.csv 3 kill     # chạy mô phỏng với tùy chọn
```

### Linux / macOS

```bash
make            # build ./timeout_strategy
make test       # build + chạy test
make clean
```

Hoặc gcc trực tiếp:

```bash
gcc -std=c11 -Wall -Wextra -Iinclude src/*.c -o timeout_strategy
```

### Windows (PowerShell / MinGW-MSYS2)

```powershell
make            # build timeout_strategy.exe
make test
```

Hoặc gcc trực tiếp:

```powershell
gcc -std=c11 -Wall -Wextra -Iinclude src/main.c src/CSVParser.c src/DeadlockDetector.c src/SimulationEngine.c src/TimeoutManager.c -o timeout_strategy.exe
```

### CMake (mọi nền tảng)

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Build sạch với `-Wall -Wextra`, không warning.

## Chạy chương trình

`run.sh` là entrypoint duy nhất cho cả chạy chương trình và benchmark.

### Chạy mô phỏng bình thường

Linux / macOS:

```bash
./run.sh data/three_process_deadlock.csv 3 kill
./run.sh data/three_process_deadlock.csv 3 retry 1 3
./run.sh data/three_process_deadlock.csv 3 rollback 1 3
./run.sh data/three_process_deadlock.csv 3 --compare
```

Windows:

```powershell
bash run.sh data/three_process_deadlock.csv 3 kill
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

```powershell
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

```powershell
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

Benchmark được chạy trực tiếp qua `run.sh` ở chế độ benchmark. Script này tự export kết quả ra CSV rồi vẽ biểu đồ.

### Chạy benchmark

Linux / macOS:

```bash
chmod +x run.sh
./run.sh benchmark
```

Windows (PowerShell):

```powershell
bash run.sh benchmark
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
make test       # Linux / macOS / Windows (MinGW)
```

Hoặc qua CMake:

```bash
ctest --test-dir build --output-on-failure
```

Hoặc build trực tiếp:

```bash
gcc -std=c11 -Iinclude tests/test_timeout.c src/CSVParser.c src/DeadlockDetector.c src/SimulationEngine.c src/TimeoutManager.c -o run_tests
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

## Tài liệu

- `docs_TimeoutStrategy.md` — tài liệu kỹ thuật chi tiết.
- `review.md` — báo cáo (kiến trúc, kết quả thí nghiệm, phân tích).
