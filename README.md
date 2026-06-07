# TimeoutStrategy

Mô phỏng đề tài **Timeout vs Phát hiện deadlock** bằng C++17.

## Cấu trúc

- `include/Models.hpp`: `Process`, `Resource`, `Event`, `PendingRequest`, metrics.
- `src/CSVParser.cpp`: đọc dataset CSV.
- `src/SimulationEngine.cpp`: event loop, clock logic, cấp phát/nhả tài nguyên.
- `src/DeadlockDetector.cpp`: Wait-For Graph, phát hiện chu trình làm ground truth.
- `src/TimeoutManager.cpp`: tính `waiting_time` và xử lý `kill`/`retry`.
- `data/sample_deadlock.csv`: dataset theo đề bài.
- `data/three_process_deadlock.csv`: dataset deadlock rõ hơn vì duration dài hơn.

## Build

```powershell
cmake -S . -B build
cmake --build build
```

## Chạy

```powershell
.\build\timeout_strategy.exe data\three_process_deadlock.csv 3 kill
.\build\timeout_strategy.exe data\three_process_deadlock.csv 3 retry 1 3
```

Cú pháp:

```text
timeout_strategy <dataset.csv> [timeout] [kill|retry] [retry_delay] [max_retries]
```

## `TimeoutManager`:

- Khi process bị block, engine tạo `PendingRequest` có `requestTime`.
- Mỗi time unit, `TimeoutManager::checkTimeouts()` tính `waitingTime = currentTime - requestTime`.
- Nếu `waitingTime >= TIMEOUT`, hệ thống kích hoạt strategy.
- `kill`: terminate process, giải phóng toàn bộ resource process đang giữ, xóa request đang chờ.
- `retry`: rút request hiện tại, chờ `retryDelay`, xin lại; nếu quá `maxRetries` thì terminate.
- Trước khi xử lý timeout, gọi deadlock detector để đánh dấu timeout đó là true positive hay false positive.

Metrics in ra gồm `killed_processes`, `deadlock_resolved`, `throughput`, `false_positive_rate`, `retry_events`.
