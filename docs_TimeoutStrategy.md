# TimeoutStrategy — Tài liệu kỹ thuật

Mô phỏng đề tài **"Chiến lược Timeout vs Phát hiện Deadlock"** bằng **C11**. Tài liệu này mô tả chính xác cách hệ thống hoạt động dựa trên mã nguồn hiện tại, dùng làm cơ sở viết báo cáo.

---

## 1. Mục tiêu đề tài

Timeout là cách xử lý deadlock phổ biến vì đơn giản, chi phí thấp, nhưng có thể gây **false positive** (kill nhầm process không thực sự deadlock). Hệ thống mô phỏng theo **thời gian logic**: mỗi sự kiện là một đơn vị thời gian (time unit).

Định nghĩa cốt lõi (bắt buộc theo đề):

```
waiting_time = current_time - request_time
```

- `request_time` — thời điểm process bắt đầu chờ tài nguyên (bị block).
- Khi `waiting_time >= TIMEOUT` → kích hoạt cơ chế xử lý (kill / retry / rollback).

Cần thí nghiệm với **≥ 3 giá trị TIMEOUT** và so sánh qua các metric: số process bị kill, deadlock resolved, throughput, false positive rate.

---

## 2. Kiến trúc tổng quan

| Thành phần | File | Vai trò |
| --- | --- | --- |
| Models | `include/Models.h` | Kiểu dữ liệu chung: `Process`, `Resource`, `Event`, `PendingRequest`, `TimeoutRecord`, `SimulationMetrics`. |
| CSVParser | `src/CSVParser.c` | Đọc + validate dataset CSV, sort theo `time`. |
| SimulationEngine | `src/SimulationEngine.c` | Đồng hồ logic, event loop, cấp phát/giải phóng tài nguyên, gom metrics. |
| TimeoutManager | `src/TimeoutManager.c` | Tính `waiting_time`, thực thi chiến lược kill / retry / rollback. |
| DeadlockDetector | `src/DeadlockDetector.c` | Wait-For Graph + DFS phát hiện chu trình; làm ground truth cho false positive. |

Luồng: `CSVParser_parse` → danh sách `Event` đã sort → `SimulationEngine_run` → mỗi time unit gọi `TimeoutManager_checkTimeouts` (truyền cả `DeadlockDetector`) → trả về `SimulationMetrics`.

> Lưu ý chung về kiểu C: code là C11 thuần nên không có constructor/method. Mọi struct đều được khởi tạo qua `memset` + gán field, và "method" được mô phỏng bằng cách truyền con trỏ struct làm tham số đầu (vd `TimeoutManager_init(&mgr, cfg)`). Việc cấp phát/giải phóng chuỗi (char *) là rõ ràng — caller có trách nhiệm `free()` khi không dùng nữa.

---

## 3. Mô hình dữ liệu (`Models.h`)

**ProcessState** — enum: `PROCESS_STATE_NEW`, `PROCESS_STATE_RUNNING`, `PROCESS_STATE_BLOCKED`, `PROCESS_STATE_COMPLETED`, `PROCESS_STATE_TERMINATED`.

**TimeoutStrategy** — enum: `TIMEOUT_STRATEGY_KILL`, `TIMEOUT_STRATEGY_RETRY`, `TIMEOUT_STRATEGY_ROLLBACK`.

**Event** (1 dòng CSV): `time, processId, action ("request"|"release"), resourceId, duration`.

- `duration` = thời gian **giữ tài nguyên sau khi được cấp phát**. `duration = 0` nghĩa là request tức thời, không giữ.

**PendingRequest** — yêu cầu đang chờ: `processId, resourceId, requestTime, duration, retryCount`.

**Process** — `id, state, heldResources (mảng char* + count/capacity), has_requestTime + requestTime, waitingFor, retryAfter, rollbackCount, completionCounted`.

**Resource** — `id, owner, has_releaseTime + releaseTime`. `Resource_isFree(r)` khi `owner == NULL`.

**TimeoutRecord** (kết quả mỗi lần xử lý timeout): `time, processId, resourceId, waitingTime, strategy, deadlockedAtTimeout, killed, retried, rolledBack, falsePositive`.

**SimulationMetrics**:

- `killedProcesses, timeoutEvents, retryEvents, rollbackEvents, deadlockResolved, completedProcesses, totalProcesses, falsePositives`.
- `SimulationMetrics_throughput(m) = completedProcesses / totalProcesses`.
- `SimulationMetrics_falsePositiveRate(m) = falsePositives / timeoutEvents`.

Ngoài ra Models.h còn định nghĩa các kiểu entry dùng chung (`ProcessEntry`, `ResourceEntry`, `IntEntry`) — đóng vai trò "cặp key-value" cho các bảng mà SimulationEngine quản lý (vì C không có `std::map`).

---

## 4. CSVParser

- Bỏ qua dòng header (dòng 1) và dòng trống.
- Mỗi dòng phải có đúng **5 cột**, nếu không thì in lỗi kèm số dòng rồi `exit(EXIT_FAILURE)`.
- Validate: `processId`/`resourceId` không rỗng; `action ∈ {request, release}`; `duration >= 0`; `time`/`duration` là số nguyên hợp lệ (parse bằng `strtol` để bắt được kí tự không phải số).
- Sau khi đọc xong, **sort tăng dần theo `time`** bằng `qsort`.

API: `Event* CSVParser_parse(const char* path, size_t* out_count)`. Trả về mảng `Event` cấp phát động — caller free sau khi dùng.

---

## 5. SimulationEngine — Event Loop

Hàm chính `SimulationEngine_run(engine, sortedEvents, eventsCount)`. Mỗi vòng lặp = 1 time unit, thực hiện đúng thứ tự sau:

1. **`releaseExpiredResources(t)`** — giải phóng tài nguyên có `releaseTime <= t` (hết hạn giữ).
2. **`grantPendingRequests(t)`** — cấp phát cho các request đang chờ (ưu tiên cũ trước, tránh starvation). Chỉ xét request có `requestTime <= t` và tài nguyên đang free.
3. **`processEventsAt(t, ...)`** — xử lý mọi event có `time == t`:
   - `request` + tài nguyên free → cấp phát ngay; nếu `duration == 0` thì complete process.
   - `request` + tài nguyên bận → **block process**, tạo `PendingRequest` với `requestTime = t` và đăng ký cạnh chờ vào Wait-For Graph.
   - `release` + đúng owner → giải phóng tài nguyên.
4. **`applyTimeouts(t)`** — gọi `TimeoutManager_checkTimeouts`, cập nhật metrics từ các `TimeoutRecord` trả về.
5. **`grantPendingRequests(t)`** (lần 2) — cấp phát lại vì kill/retry có thể vừa giải phóng tài nguyên.
6. **`checkAndCompleteProcesses()`** — process `Running`, không giữ tài nguyên, hết event, không chờ gì → đánh dấu `Completed`.

**Điều kiện dừng**: hết event mới (`nextEventIndex >= size`) **và** `pendingRequests` rỗng **và** không còn tài nguyên nào sẽ release trong tương lai. Nếu chưa dừng → `++currentTime`.

Chi tiết cấp phát (`SimulationEngine_allocateResource`): set `owner`, thêm vào `heldResources`, state → `Running`, reset `has_requestTime`/`waitingFor`. Nếu `duration > 0` đặt `releaseTime = t + duration`.

Để hỗ trợ rollback, engine còn lưu thêm:

- `remainingEventCount_` — bộ đếm event chưa xử lý của mỗi process (giúp `checkAndCompleteProcesses` biết khi nào hoàn tất).
- `processEvents_` — vector event gốc per-process (key = processId, value = `EventVector`). `replayProcess` dùng vector này để re-inject event khi rollback.

---

## 6. TimeoutManager — phần trọng tâm

### Cấu hình (`TimeoutConfig`)

- `timeout` (mặc định 5) — ngưỡng TIMEOUT.
- `strategy` — `TIMEOUT_STRATEGY_KILL` / `..._RETRY` / `..._ROLLBACK`.
- `retryDelay` (mặc định 1) — số time unit chờ trước khi xin lại.
- `maxRetries` (mặc định 3) — số lần retry tối đa trước khi buộc kill.
- `maxRollbacks` (mặc định 3) — số lần rollback tối đa trước khi buộc kill.
- `TimeoutManager_init` kiểm tra `timeout >= 1`, nếu không thì in lỗi rồi `exit(1)`.

### `TimeoutManager_checkTimeouts(...)`

Duyệt mảng `pendingRequests`:

- Bỏ qua request mà process không ở trạng thái `PROCESS_STATE_BLOCKED`.
- Tính `waitingTime = currentTime - request.requestTime`.
- Nếu `waitingTime < timeout` → bỏ qua.
- Nếu `>= timeout` → kích hoạt strategy tương ứng.
- Sau khi một process bị kill/rollback (mảng `pendingRequests` thay đổi nhiều phần tử) → reset `index = 0` để duyệt lại an toàn.

Biến `deadlocked` lấy từ `DeadlockDetector_isInDeadlock(detector, req->processId)` — kiểm tra **đúng process bị timeout có nằm trong chu trình chờ hay không**, không dùng cycle toàn cục. Nhờ vậy false positive được tính chuẩn theo định nghĩa đề (bị kill mà không thực sự deadlock).

Trả về mảng `TimeoutRecord*` cấp phát động kèm `*outCount`. Caller có trách nhiệm `free()` từng `processId`/`resourceId` rồi free mảng.

### Chiến lược Kill (`doKillProcess`)

- State → `PROCESS_STATE_TERMINATED`, reset `has_requestTime`/`waitingFor`.
- **Giải phóng toàn bộ** `heldResources` (reset owner + has_releaseTime trong resource map).
- **Xóa mọi** `PendingRequest` của process đó (dồn in-place).
- `TimeoutRecord`: `killed = true`, `falsePositive = !deadlocked`.

### Chiến lược Retry (`doRetryRequest`)

- Lưu sẵn `processId`/`resourceId` (vì `removePendingAt` sẽ free chuỗi gốc).
- Gỡ request hiện tại khỏi pending, `savedRetryCount = retryCount + 1`.
- Nếu `savedRetryCount > maxRetries` → leo thang gọi `doKillProcess` (đảm bảo giải phóng tài nguyên).
- Ngược lại: state → `Running`, đặt `retryAfter = currentTime + retryDelay`, đẩy lại pending mới với `requestTime = retryAfter`.
- `TimeoutRecord`: `retried = true`.

### Chiến lược Rollback (`doRollbackProcess`)

- `process.rollbackCount += 1`. Nếu `rollbackCount > maxRollbacks` → leo thang `doKillProcess` (chống livelock).
- Ngược lại:
  - Thu hồi **toàn bộ** `heldResources`.
  - Xóa **mọi** pending của process.
  - State → `PROCESS_STATE_NEW` (về trạng thái ban đầu).
- `TimeoutRecord`: `rolledBack = true`.
- Engine bắt record này trong `applyTimeouts` và gọi `SimulationEngine_replayProcess`: **re-inject toàn bộ request event** của process vào hàng đợi với `requestTime = currentTime`, đồng thời reset `remainingEventCount` của process. Đây là khác biệt cốt lõi với Kill — process chạy lại từ đầu thay vì bị hủy, nên throughput không mất.
- Khác Kill: process không bị hủy (throughput giữ nguyên). Khác Retry: rollback trả lại **tất cả** tài nguyên đang giữ và chạy lại từ đầu, còn retry chỉ xin lại đúng request đang chờ.

---

## 7. Metrics — cách tính

Trong `SimulationEngine_applyTimeouts`, mỗi `TimeoutRecord` cập nhật:

- `timeoutEvents += 1` (mỗi lần timeout kích hoạt).
- `falsePositives += 1` nếu `!deadlockedAtTimeout` (process bị timeout nhưng không nằm trong chu trình).
- `retryEvents += 1` nếu `retried`.
- `rollbackEvents += 1` nếu `rolledBack` (đồng thời gỡ process khỏi Wait-For Graph và re-inject events qua `replayProcess`).
- `killedProcesses += 1` nếu `killed` (đồng thời gỡ process khỏi Wait-For Graph).
- `deadlockResolved += 1` chỉ khi: process thực sự deadlock (`deadlockedAtTimeout`), đồ thị **có chu trình trước** khi xử lý và **hết chu trình sau** khi xử lý. Tức đo đúng khoảnh khắc "có chu trình → hết chu trình" theo đề. Cách này tránh đếm trùng khi nhiều process trong cùng một chu trình bị xử lý liên tiếp.

Tổng hợp cuối:

- `throughput = completedProcesses / totalProcesses`.
- `falsePositiveRate = falsePositives / timeoutEvents`.

> Throughput luôn trong khoảng [0, 1]: process đã `COMPLETED`/`TERMINATED` không bị "hồi sinh" (pending request của nó bị purge trong `grantPendingRequests` và `completeProcess`), nên `completedProcesses` không đếm trùng.

---

## 8. Dataset

`data/sample_deadlock.csv` (theo đề, duration = 5):

```
time,process_id,action,resource_id,duration
0,P1,request,R1,5
1,P2,request,R2,5
2,P3,request,R3,5
3,P1,request,R2,0
4,P2,request,R3,0
5,P3,request,R1,0
6,P4,request,R2,0
7,P5,request,R3,0
```

P1→R2, P2→R3, P3→R1 tạo chu trình chờ vòng (R1→R2→R3→R1) → deadlock.

`data/three_process_deadlock.csv` — cùng cấu trúc nhưng `duration = 10` → giữ tài nguyên lâu hơn, deadlock rõ hơn, dễ kích hoạt timeout.

`data/tc_1.csv` … `data/tc_4.csv` — các kịch bản dùng cho benchmark (kết hợp deadlock thật và chỉ đơn giản chờ lâu).

---

## 9. Build & Run

```powershell
cmake -S . -B build
cmake --build build
```

Hoặc dùng Makefile (cross-platform clean):

```powershell
make
```

> `CMakeLists.txt` build executable `timeout_strategy` và `run_tests`. Build sạch với `-Wall -Wextra`, không warning.

Cú pháp executable:

```text
timeout_strategy <dataset.csv> [timeout] [kill|retry|rollback] [retry_delay] [max_retries|max_rollbacks] [-v|--verbose] [-c|--compare]
```

Ví dụ:

```powershell
.\timeout_strategy.exe data\three_process_deadlock.csv 3 kill
.\timeout_strategy.exe data\three_process_deadlock.csv 3 retry 1 3
.\timeout_strategy.exe data\three_process_deadlock.csv 3 rollback 1 3
.\timeout_strategy.exe data\three_process_deadlock.csv 3 kill -v
```

Cờ `-v`/`--verbose` in log từng sự kiện theo time unit (request/grant/block/release, timeout, deadlock resolved, complete), ví dụ:

```text
Time 0: P1 requests R1 -> Granted
Time 3: P1 requests R2 -> Blocked (held by P2)
Time 6: P1 TIMEOUT (waited 3, deadlock) -> Killed
Time 6: Deadlock resolved (cycle broken)
```

Cờ `-c`/`--compare` chạy cả 3 chiến lược (kill/retry/rollback) trên cùng dataset + cùng TIMEOUT, in bảng so sánh metrics một lần:

```powershell
.\timeout_strategy.exe data\three_process_deadlock.csv 3 --compare
```

```text
strategy   completed  killed retries rollbacks  resolved      fp  throughput   fp_rate
--------------------------------------------------------------------------------------
kill               4       1       0         0         1       0       0.800     0.000
retry              5       0       5         0         1       4       1.000     0.800
rollback           5       0       0         1         1       0       1.000     0.000
```

---

## 10. Thí nghiệm đề xuất (cho báo cáo)

Chạy mỗi dataset với **TIMEOUT = 3, 5, 10** cho cả 3 chiến lược (kill, retry, rollback). Số liệu thực tế và phân tích đầy đủ xem `review.md` (bài report).

Phân tích kỳ vọng:

- **TIMEOUT nhỏ** — kill sớm → killed cao, throughput thấp, false positive cao (giết nhầm process chỉ đang chờ lâu).
- **TIMEOUT lớn** — ít false positive hơn nhưng hệ thống "treo" lâu trước khi giải quyết deadlock → deadlock resolved chậm.
- **Rollback** — giữ throughput cao (không hủy process) nhưng có rủi ro livelock → giới hạn bằng `maxRollbacks` rồi leo thang kill.

---

## 11. Trạng thái hiện tại

Đã hoàn thành và verify (build `-Wall -Wextra` sạch, chạy end-to-end):

- `CSVParser` — đọc + validate + sort dataset.
- `SimulationEngine` — event loop time-unit, cấp/giải phóng resource, fix double-count throughput.
- `TimeoutManager` — kill + retry + rollback, `waiting_time`, leo thang retry/rollback → kill.
- `DeadlockDetector` — Wait-For Graph + DFS, `detectDeadlock()` (cycle toàn cục) và `isInDeadlock(pid)` (process có trong chu trình).
- `SimulationEngine` — lưu event gốc per-process, re-inject khi rollback (`replayProcess`).
- `main.c` — CLI đầy đủ (kill/retry/rollback), in config + 10 metrics, cờ `-v` in per-event log.
- `CMakeLists.txt` — bật executable + run_tests; `Makefile` clean cross-platform.
- `tests/` — có test tự động (xem mục 12).

Hạng mục còn dở theo phân công / báo cáo:

- `data/sample_deadlock.csv` không trigger timeout (request vòng có `duration=0`, resource hết hạn gỡ vòng trước ngưỡng). Dùng `data/three_process_deadlock.csv` để demo deadlock; nếu cần demo trên sample, tăng duration request vòng.

---

## 12. Tests tự động

`tests/test_timeout.c` — harness nhẹ tự viết (assert macro, không cần framework ngoài). Phủ:

- Parser: parse hợp lệ, sort theo time.
- WFGraph/Detector: `detectDeadlock` bắt chu trình; `isInDeadlock` phân biệt process trong vòng vs ngoài vòng.
- TimeoutManager: tính `waiting_time`, kill giải phóng resource + xóa pending, retry tăng `retryCount` và leo thang kill khi vượt `maxRetries`, rollback đưa state về `New` + leo thang kill khi vượt `maxRollbacks`.
- Engine end-to-end: throughput luôn ≤ 1 (không double-count); kịch bản deadlock cho `killedProcesses`/`deadlockResolved` đúng; rollback hội tụ (không loop vô hạn) và phát sinh `rollbackEvents`.

Build + chạy:

```powershell
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Hoặc trực tiếp:

```powershell
gcc -std=c11 -Iinclude tests/test_timeout.c src/CSVParser.c src/DeadlockDetector.c src/SimulationEngine.c src/TimeoutManager.c -o run_tests.exe
.\run_tests.exe
```

`tests/test_deadlock.c` là chương trình demo nhỏ chạy tay (không có assert), in lại trạng thái Wait-For Graph qua từng bước add/remove cạnh; có thể build độc lập để kiểm tra trực giác chứ không nằm trong CMake test suite.
