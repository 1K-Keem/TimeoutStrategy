# TimeoutStrategy

Mô phỏng đề tài **Chiến lược Timeout vs Phát hiện Deadlock** bằng C++17. Hệ thống xử lý deadlock theo thời gian logic (mỗi event = 1 time unit) với 3 chiến lược timeout — `kill`, `retry`, `rollback` — và đối chiếu với deadlock detection (Wait-For Graph) làm ground truth để đo false positive.

Định nghĩa cốt lõi: `waiting_time = current_time - request_time`. Khi `waiting_time >= TIMEOUT` → kích hoạt xử lý.

## Cấu trúc

- `include/Models.hpp`: `Process`, `Resource`, `Event`, `PendingRequest`, `TimeoutRecord`, `SimulationMetrics`.
- `src/CSVParser.cpp`: đọc + validate dataset CSV, sort theo `time`.
- `src/SimulationEngine.cpp`: event loop, clock logic, cấp phát/nhả tài nguyên, lưu event gốc để replay (rollback), gom metrics.
- `src/DeadlockDetector.cpp`: Wait-For Graph + DFS. `detectDeadlock()` (cycle toàn cục), `isInDeadlock(pid)` (process có nằm trong chu trình).
- `src/TimeoutManager.cpp`: tính `waiting_time`, xử lý `kill` / `retry` / `rollback`.
- `src/main.cpp`: CLI.
- `tests/test_main.cpp`: test tự động (harness tự viết, không framework ngoài).
- `data/sample_deadlock.csv`: dataset theo đề bài.
- `data/three_process_deadlock.csv`: dataset deadlock rõ hơn (duration dài hơn).

## Build

Yêu cầu: trình biên dịch C++17 (`g++` >= 7 hoặc `clang++`), tùy chọn `cmake`/`make`. Code thuần chuẩn C++17, không phụ thuộc API riêng OS — chạy trên Linux, macOS và Windows.

### Linux / macOS

```bash
make            # build ./timeout_strategy
make test       # build + chạy test
make clean
```

Hoặc dùng script:

```bash
chmod +x run.sh
./run.sh data/three_process_deadlock.csv 3 kill
```

Hoặc g++ trực tiếp:

```bash
g++ -std=c++17 -Wall -Wextra -Iinclude src/*.cpp -o timeout_strategy
```

### Windows (PowerShell)

```powershell
make            # build timeout_strategy.exe (can MinGW/MSYS2)
make test
```

Hoặc g++ trực tiếp:

```powershell
g++ -std=c++17 -Wall -Wextra -Iinclude src/main.cpp src/CSVParser.cpp src/DeadlockDetector.cpp src/SimulationEngine.cpp src/TimeoutManager.cpp -o timeout_strategy.exe
```

### CMake (mọi nền tảng)

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Build sạch với `-Wall -Wextra`, không warning.

## Chạy

Linux / macOS:

```bash
./timeout_strategy data/three_process_deadlock.csv 3 kill
./timeout_strategy data/three_process_deadlock.csv 3 retry 1 3
./timeout_strategy data/three_process_deadlock.csv 3 rollback 1 3
```

Windows:

```powershell
.\timeout_strategy.exe data\three_process_deadlock.csv 3 kill
```

Cú pháp:

```text
timeout_strategy <dataset.csv> [timeout] [kill|retry|rollback] [retry_delay] [max_retries|max_rollbacks] [-v|--verbose] [-c|--compare]
```

- `timeout`: ngưỡng TIMEOUT (>= 1, mặc định 5).
- `strategy`: `kill` | `retry` | `rollback` (mặc định `kill`).
- `retry_delay`: số time unit chờ trước khi xin lại (mặc định 1).
- `max_retries` / `max_rollbacks`: số lần tối đa trước khi leo thang sang kill (mặc định 3).
- `-v` / `--verbose`: in log từng sự kiện theo time unit.
- `-c` / `--compare`: chạy cả 3 chiến lược, in bảng so sánh metrics một lần.

### Log từng sự kiện (`-v`)

```powershell
./timeout_strategy data/three_process_deadlock.csv 3 kill -v
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
./timeout_strategy data/three_process_deadlock.csv 3 --compare
```

```text
strategy   completed  killed retries rollbacks  resolved      fp  throughput   fp_rate
--------------------------------------------------------------------------------------
kill               4       1       0         0         1       0       0.800     0.000
retry              5       0       5         0         1       4       1.000     0.800
rollback           5       0       0         1         1       0       1.000     0.000
```

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
g++ -std=c++17 -Iinclude tests/test_main.cpp src/CSVParser.cpp src/DeadlockDetector.cpp src/SimulationEngine.cpp src/TimeoutManager.cpp -o run_tests
./run_tests
```

## Cơ chế timeout

- Khi process bị block, engine tạo `PendingRequest` có `requestTime`.
- Mỗi time unit, `TimeoutManager::checkTimeouts()` tính `waitingTime = currentTime - requestTime`.
- Nếu `waitingTime >= TIMEOUT`, kích hoạt strategy:
  - `kill`: terminate process, giải phóng toàn bộ resource, xóa pending của process.
  - `retry`: rút request hiện tại, chờ `retryDelay`, xin lại; vượt `maxRetries` → leo thang kill.
  - `rollback`: thu hồi toàn bộ resource, đưa process về trạng thái ban đầu (`New`) và replay toàn bộ event; vượt `maxRollbacks` → leo thang kill.
- Trước khi xử lý, gọi `detector.isInDeadlock(pid)` để đánh dấu timeout là true positive hay false positive.

Metrics: `killed_processes`, `retry_events`, `rollback_events`, `deadlock_resolved`, `throughput`, `false_positives`, `false_positive_rate`.

## Tài liệu

- `docs_TimeoutStrategy.md`: tài liệu kỹ thuật chi tiết.
- `review.md`: báo cáo (kiến trúc, kết quả thí nghiệm, phân tích).




