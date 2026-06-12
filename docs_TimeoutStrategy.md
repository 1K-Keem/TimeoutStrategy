# TimeoutStrategy — Tài liệu kỹ thuật

Mô phỏng đề tài **"Chiến lược Timeout vs Phát hiện Deadlock"** bằng C++17. Tài liệu này mô tả chính xác cách hệ thống hoạt động dựa trên mã nguồn hiện tại, dùng làm cơ sở viết báo cáo.

---

## 1. Mục tiêu đề tài

Timeout là cách xử lý deadlock phổ biến vì đơn giản, chi phí thấp, nhưng có thể gây **false positive** (kill nhầm process không thực sự deadlock). Hệ thống mô phỏng theo **thời gian logic**: mỗi sự kiện là một đơn vị thời gian (time unit).

Định nghĩa cốt lõi (bắt buộc theo đề):
```
waiting_time = current_time - request_time
```
- `request_time`: thời điểm process bắt đầu chờ tài nguyên (bị block).
- Khi `waiting_time >= TIMEOUT` → kích hoạt cơ chế xử lý (kill / retry).

Cần thí nghiệm với **≥ 3 giá trị TIMEOUT** và so sánh qua các metric: số process bị kill, deadlock resolved, throughput, false positive rate.

---

## 2. Kiến trúc tổng quan

| Thành phần | File | Vai trò |
| --- | --- | --- |
| Models | `include/Models.hpp` | Kiểu dữ liệu chung: `Process`, `Resource`, `Event`, `PendingRequest`, `TimeoutRecord`, `SimulationMetrics`. |
| CSVParser | `src/CSVParser.cpp` | Đọc + validate dataset CSV, sort theo `time`. |
| SimulationEngine | `src/SimulationEngine.cpp` | Đồng hồ logic, event loop, cấp phát/giải phóng tài nguyên, gom metrics. |
| TimeoutManager | `src/TimeoutManager.cpp` | Tính `waiting_time`, thực thi chiến lược kill / retry / rollback. |
| DeadlockDetector | `src/DeadlockDetector.cpp` | Wait-For Graph + DFS phát hiện chu trình; làm ground truth cho false positive. |

Luồng: `CSVParser` → danh sách `Event` đã sort → `SimulationEngine.run()` → mỗi time unit gọi `TimeoutManager.checkTimeouts()` (truyền cả `DeadlockDetector`) → trả về `SimulationMetrics`.

---

## 3. Mô hình dữ liệu (`Models.hpp`)

**ProcessState**: `New, Running, Blocked, Completed, Terminated`.

**TimeoutStrategy**: `Kill, Retry, Rollback`.

**Event** (1 dòng CSV): `time, processId, action ("request"|"release"), resourceId, duration`.
- `duration` = thời gian **giữ tài nguyên sau khi được cấp phát**. `duration = 0` nghĩa là request tức thời, không giữ.

**PendingRequest**: yêu cầu đang chờ — `processId, resourceId, requestTime, duration, retryCount`.

**Process**: `id, state, heldResources (set), requestTime (optional), waitingFor (optional), retryAfter`.

**Resource**: `id, owner (optional), releaseTime (optional)`. `isFree()` khi không có owner.

**TimeoutRecord** (kết quả mỗi lần xử lý timeout): `time, processId, resourceId, waitingTime, strategy, deadlockedAtTimeout, killed, retried, falsePositive`.

**SimulationMetrics**:
- `killedProcesses, timeoutEvents, retryEvents, deadlockResolved, completedProcesses, totalProcesses, falsePositives`.
- `throughput() = completedProcesses / totalProcesses`.
- `falsePositiveRate() = falsePositives / timeoutEvents`.

---

## 4. CSVParser

- Bỏ qua dòng header (dòng 1) và dòng trống.
- Mỗi dòng phải có đúng **5 cột**, nếu không → `runtime_error` kèm số dòng.
- Validate: `processId`/`resourceId` không rỗng; `action` ∈ {`request`, `release`}; `duration >= 0`; `time`/`duration` là số nguyên hợp lệ.
- Sau khi đọc xong, **sort tăng dần theo `time`**.

---

## 5. SimulationEngine — Event Loop

Hàm chính `run(sortedEvents)`. Mỗi vòng lặp = 1 time unit, thực hiện đúng thứ tự sau:

1. **`releaseExpiredResources(t)`** — giải phóng tài nguyên có `releaseTime <= t` (hết hạn giữ).
2. **`grantPendingRequests(t)`** — cấp phát cho các request đang chờ (ưu tiên cũ trước, tránh starvation). Chỉ xét request có `requestTime <= t` và tài nguyên đang free.
3. **`processEventsAt(t, ...)`** — xử lý mọi event có `time == t`:
   - `request` + tài nguyên free → cấp phát ngay; nếu `duration == 0` thì complete process.
   - `request` + tài nguyên bận → **block process**, tạo `PendingRequest` với `requestTime = t`.
   - `release` + đúng owner → giải phóng tài nguyên.
4. **`applyTimeouts(t)`** — gọi `TimeoutManager.checkTimeouts()`, cập nhật metrics từ các `TimeoutRecord`.
5. **`grantPendingRequests(t)`** (lần 2) — cấp phát lại vì kill/retry có thể vừa giải phóng tài nguyên.
6. **`checkAndCompleteProcesses()`** — process `Running`, không giữ tài nguyên, hết event, không chờ gì → đánh dấu `Completed`.

**Điều kiện dừng**: hết event mới (`nextEventIndex >= size`) **và** `pendingRequests` rỗng **và** không còn tài nguyên nào sẽ release trong tương lai. Nếu chưa dừng → `++currentTime`.

Chi tiết cấp phát (`allocateResource`): set `owner`, thêm vào `heldResources`, state → `Running`, reset `requestTime`/`waitingFor`. Nếu `duration > 0` đặt `releaseTime = t + duration`.

---

## 6. TimeoutManager — phần trọng tâm (Kim)

### Cấu hình (`TimeoutConfig`)
- `timeout` (mặc định 5): ngưỡng TIMEOUT.
- `strategy`: `Kill`, `Retry` hoặc `Rollback`.
- `retryDelay` (mặc định 1): số time unit chờ trước khi xin lại.
- `maxRetries` (mặc định 3): số lần retry tối đa trước khi buộc kill.
- `maxRollbacks` (mặc định 3): số lần rollback tối đa trước khi buộc kill.
- Constructor ném `std::invalid_argument` nếu `timeout < 1`.

### `checkTimeouts(currentTime, processes, resources, pendingRequests, detector)`
Duyệt `pendingRequests`:
- Bỏ qua request mà process không ở trạng thái `Blocked`.
- Tính `waitingTime = currentTime - request.requestTime`.
- Nếu `waitingTime < timeout` → bỏ qua.
- Nếu `>= timeout` → kích hoạt strategy tương ứng.
- Sau khi một process bị kill (container `pendingRequests` thay đổi nhiều phần tử) → reset `index = 0` để duyệt lại an toàn.

Biến `deadlocked` lấy từ `detector.isInDeadlock(processId)` — kiểm tra **đúng process bị timeout có nằm trong chu trình chờ hay không**, không dùng cycle toàn cục. Nhờ vậy false positive được tính chuẩn theo định nghĩa đề (bị kill mà không thực sự deadlock).

### Chiến lược Kill (`killProcess`)
- State → `Terminated`, reset `requestTime`/`waitingFor`.
- **Giải phóng toàn bộ** `heldResources` (reset owner + releaseTime).
- **Xóa mọi** `PendingRequest` của process đó.
- `TimeoutRecord`: `killed = true`, `falsePositive = !deadlocked`.

### Chiến lược Retry (`retryRequest`)
- Gỡ request hiện tại khỏi pending, `retryCount += 1`, reset `requestTime`/`waitingFor`.
- Nếu `retryCount > maxRetries` → leo thang gọi `killProcess` (đảm bảo giải phóng tài nguyên).
- Ngược lại: state → `Running`, đặt `retryAfter = currentTime + retryDelay`, đẩy lại pending với `requestTime = retryAfter`.
- `TimeoutRecord`: `retried = true`.

### Chiến lược Rollback (`rollbackProcess`)
- `rollbackCount += 1`. Nếu `rollbackCount > maxRollbacks` → leo thang gọi `killProcess` (chống livelock).
- Ngược lại: thu hồi **toàn bộ** `heldResources`, xóa **mọi** `PendingRequest` của process, state → `New` (về trạng thái ban đầu).
- `TimeoutRecord`: `rolledBack = true`.
- Engine bắt record này trong `applyTimeouts` và gọi `replayProcess`: **re-inject toàn bộ request event** của process vào hàng đợi với `requestTime = currentTime`, đồng thời reset `remainingEventCount` của process. Đây là khác biệt cốt lõi với Kill — process chạy lại từ đầu thay vì bị hủy, nên throughput không mất.
- Khác Kill: process không bị hủy (throughput giữ nguyên). Khác Retry: rollback trả lại **tất cả** tài nguyên đang giữ và chạy lại từ đầu, còn retry chỉ xin lại đúng request đang chờ.

---

## 7. Metrics — cách tính

Trong `applyTimeouts`, mỗi `TimeoutRecord` cập nhật:
- `timeoutEvents += 1` (mỗi lần timeout kích hoạt).
- `falsePositives += 1` nếu `!deadlockedAtTimeout` (process bị timeout nhưng không nằm trong chu trình).
- `retryEvents += 1` nếu `retried`.
- `rollbackEvents += 1` nếu `rolledBack` (đồng thời gỡ process khỏi Wait-For Graph và re-inject events).
- `killedProcesses += 1` nếu `killed` (đồng thời gỡ process khỏi Wait-For Graph).
- `deadlockResolved += 1` chỉ khi: process thực sự deadlock (`deadlockedAtTimeout`), đồ thị **có chu trình trước** khi xử lý và **hết chu trình sau** khi xử lý. Tức đo đúng khoảnh khắc "có chu trình → hết chu trình" theo đề.

Tổng hợp cuối:
- `throughput = completedProcesses / totalProcesses`.
- `falsePositiveRate = falsePositives / timeoutEvents`.

> Throughput luôn trong khoảng [0, 1]: process đã `Completed`/`Terminated` không bị "hồi sinh" (pending request của nó bị purge trong `grantPendingRequests` và `completeProcess`), nên `completedProcesses` không đếm trùng.

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

`data/three_process_deadlock.csv`: cùng cấu trúc nhưng `duration = 10` → giữ tài nguyên lâu hơn, deadlock rõ hơn, dễ kích hoạt timeout.

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

> `CMakeLists.txt` build static library `timeout_kim`, executable `timeout_strategy` và `run_tests`. Build sạch với `-Wall -Wextra`, không warning.

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
- **TIMEOUT nhỏ**: kill sớm → killed cao, throughput thấp, false positive cao (giết nhầm process chỉ đang chờ lâu).
- **TIMEOUT lớn**: ít false positive hơn nhưng hệ thống "treo" lâu trước khi giải quyết deadlock → deadlock resolved chậm.
- **Rollback** giữ throughput cao (không hủy process) nhưng có rủi ro livelock → giới hạn bằng `maxRollbacks` rồi leo thang kill.

---

## 11. Hạng mục còn dở (trạng thái hiện tại)

Đã hoàn thành và verify (build `-Wall -Wextra` sạch, chạy end-to-end):
- `CSVParser` — đọc + validate + sort dataset.
- `SimulationEngine` — event loop time-unit, cấp/giải phóng resource, fix double-count throughput.
- `TimeoutManager` — kill + retry + rollback, `waiting_time`, leo thang retry/rollback → kill.
- `DeadlockDetector` — Wait-For Graph + DFS, `detectDeadlock()` (cycle toàn cục) và `isInDeadlock(pid)` (process có trong chu trình).
- `SimulationEngine` — lưu event gốc per-process, re-inject khi rollback (`replayProcess`).
- `main.cpp` — CLI đầy đủ (kill/retry/rollback), in config + 10 metrics, cờ `-v` in per-event log.
- `CMakeLists.txt` — bật executable + run_tests; `Makefile` clean cross-platform.
- `tests/` — có test tự động (xem mục 12).

Còn lại theo phân công / báo cáo:
- `data/sample_deadlock.csv` không trigger timeout (request vòng có `duration=0`, resource hết hạn gỡ vòng trước ngưỡng). Dùng `data/three_process_deadlock.csv` để demo deadlock; nếu cần demo trên sample, tăng duration request vòng.

---

## 12. Tests tự động

`tests/test_main.cpp` — harness nhẹ tự viết (assert macro, không cần framework ngoài). Phủ:
- Parser: parse hợp lệ, lỗi sai số cột, action không hợp lệ, sort theo time.
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
g++ -std=c++17 -Iinclude tests/test_main.cpp src/CSVParser.cpp src/DeadlockDetector.cpp src/SimulationEngine.cpp src/TimeoutManager.cpp -o run_tests.exe
.\run_tests.exe
```






