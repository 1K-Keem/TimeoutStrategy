# Review — Assignment "Chiến lược Timeout vs Phát hiện Deadlock"

Branch: `deadlock_detection` · Ngày: 2026-06-12 · Ngôn ngữ: C++17

---

## 1. Đề bài & yêu cầu

Mô phỏng cơ chế **timeout** xử lý deadlock theo **thời gian logic** (mỗi event = 1 time unit), so sánh với **deadlock detection** làm ground truth.

Định nghĩa cốt lõi (bắt buộc):
```
waiting_time = current_time - request_time
```
Khi `waiting_time >= TIMEOUT` → kích hoạt xử lý (kill / retry / rollback).

Yêu cầu:
1. Định nghĩa thời gian logic, mỗi event = 1 time unit.
2. Cài timeout: lưu `request_time` khi block, trigger khi `waiting_time >= TIMEOUT`.
3. Cài ≥ 2 cách xử lý: **kill** + **retry** (rollback bỏ qua theo phân công).
4. Thí nghiệm với ≥ 3 giá trị TIMEOUT.
5. Metrics: killed processes, deadlock resolved, throughput, false positive rate.
6. Phân tích: TIMEOUT nhỏ vs lớn; false positive xảy ra khi nào.

Phân công (`assign.md`): Quang (core/engine), Kim (timeout), Khánh (detector), Khoa (data/metrics/report).

---

## 2. Kiến trúc

| Thành phần | File | Vai trò |
| --- | --- | --- |
| Models | `include/Models.hpp` | Kiểu chung: `Process`, `Resource`, `Event`, `PendingRequest`, `TimeoutRecord`, `SimulationMetrics`. |
| CSVParser | `src/CSVParser.cpp` | Đọc + validate dataset, sort theo `time`. |
| SimulationEngine | `src/SimulationEngine.cpp` | Đồng hồ logic, event loop, cấp/giải phóng resource, gom metrics. |
| TimeoutManager | `src/TimeoutManager.cpp` | Tính `waiting_time`, thực thi kill/retry. |
| DeadlockDetector | `src/DeadlockDetector.cpp` | Wait-For Graph + DFS phát hiện chu trình (ground truth false positive). |
| main | `src/main.cpp` | CLI: parse args → chạy engine → in metrics. |

Luồng: `CSVParser` → `Event[]` đã sort → `SimulationEngine.run()` → mỗi time unit gọi `TimeoutManager.checkTimeouts(..., detector)` → `SimulationMetrics`.

Event loop mỗi time unit (`SimulationEngine::run`):
1. Giải phóng resource hết hạn (`releaseTime <= currentTime`).
2. Cấp pending requests cũ (tránh starvation).
3. Xử lý event mới tại `currentTime`.
4. Áp dụng timeout.
5. Cấp lại pending (resource có thể vừa được giải phóng sau kill/retry).
6. Quét process hoàn thành.

---

## 3. Cơ chế timeout (yêu cầu 2 & 3)

`TimeoutManager::checkTimeouts` duyệt `pendingRequests`, chỉ xét process `Blocked`:
- `waitingTime = currentTime - request.requestTime`.
- Trigger khi `waitingTime >= timeout`.
- Constructor ném `std::invalid_argument` nếu `timeout < 1`.

**Kill** (`killProcess`): state → `Terminated`, giải phóng toàn bộ `heldResources`, xóa mọi pending của process.

**Retry** (`retryRequest`): gỡ request, `retryCount += 1`. Nếu vượt `maxRetries` → leo thang sang kill (đảm bảo giải phóng tài nguyên). Ngược lại → `Running`, hẹn `retryAfter = currentTime + retryDelay`, đẩy lại pending với `requestTime` mới.

**Rollback**: cố ý bỏ qua theo phân công.

---

## 4. Phát hiện deadlock (ground truth)

`WFGraph` — Wait-For Graph có hướng (process → process đang giữ tài nguyên nó chờ):
- `detectDeadlock()` — DFS bắt chu trình bất kỳ trong đồ thị (`hasCycle`).
- `isInDeadlock(pid)` — reachability `pid → ... → pid`, xác định process **có thực sự nằm trong chu trình** hay không.

Engine cập nhật đồ thị theo hành động: thêm edge khi block, gỡ khi cấp resource / kill / complete.

---

## 5. Metrics (yêu cầu 5)

Trong `applyTimeouts`, mỗi `TimeoutRecord`:
- `timeoutEvents += 1`.
- `falsePositives += 1` nếu process **không** nằm trong chu trình (`!deadlockedAtTimeout`).
- `retryEvents += 1` nếu retry; `killedProcesses += 1` nếu kill.
- `deadlockResolved += 1` chỉ khi: process thực sự deadlock + đồ thị **có chu trình trước** và **hết chu trình sau** khi xử lý.

Tổng hợp:
- `throughput = completedProcesses / totalProcesses` (luôn ∈ [0, 1]).
- `falsePositiveRate = falsePositives / timeoutEvents`.

---

## 6. Các lỗi đã phát hiện & sửa

### 6.1 Double-count throughput (bug chặn báo cáo)
- Triệu chứng: retry timeout=3 cho `completed=8/total=5` → throughput **1.6** (vô lý).
- Nguyên nhân: process complete xong nhưng pending retry còn trong hàng đợi → `grantPendingRequests` cấp lại resource → `allocateResource` set state về `Running` → complete lần nữa → đếm trùng.
- Sửa: `grantPendingRequests` (`src/SimulationEngine.cpp`) bỏ qua/purge pending của process `!isAlive()`. Throughput về **1.0**.

### 6.2 False positive tính sai
- Trước: dùng `detectDeadlock()` (cycle toàn cục) — process chờ lâu ngoài vòng vẫn bị tính là true positive.
- Sửa: `src/TimeoutManager.cpp` dùng `detector.isInDeadlock(processId)`; thêm `WFGraph::pidInCycle` (reachability). FP giờ đúng định nghĩa đề: bị kill mà không thực sự deadlock.

### 6.3 deadlock_resolved tính sai
- Trước: đếm theo mỗi timeout record có cycle, không kiểm tra deadlock có thật sự được gỡ.
- Sửa: đo `cycleBefore`/`cycleAfter` quanh lúc xử lý; chỉ tăng khi "có chu trình → hết chu trình" (đúng yêu cầu log của đề).

### 6.4 Dọn warning, vệ sinh repo
- `-Wmissing-field-initializers` (`Process{}`/`Resource{}`) → khởi tạo tường minh. Build `-Wall -Wextra` sạch.
- `.gitignore` thêm `*.o *.a /timeout_strategy`; gỡ binary 1.4MB khỏi tracking (`git rm --cached`).
- `Makefile` `clean` cross-platform (`del` Windows / `rm` *nix).
- `main.cpp` từ stub `Hello World!` → CLI đầy đủ.

---

## 7. Kết quả thí nghiệm (yêu cầu 4)

12 tổ hợp: 2 dataset × {kill, retry} × TIMEOUT {3, 5, 10}.

### `data/three_process_deadlock.csv` (duration=10, có deadlock thật)

| TIMEOUT | Strategy | killed | resolved | throughput | fp_rate | timeouts |
| --- | --- | --- | --- | --- | --- | --- |
| 3 | kill | 1 | 1 | 0.80 | 0.00 | 1 |
| 5 | kill | 1 | 1 | 0.80 | 0.00 | 1 |
| 10 | kill | 0 | 0 | 1.00 | 0.00 | 0 |
| 3 | retry | 0 | 1 | 1.00 | 0.80 | 5 |
| 5 | retry | 0 | 1 | 1.00 | 0.50 | 2 |
| 10 | retry | 0 | 0 | 1.00 | 0.00 | 0 |

### `data/sample_deadlock.csv` (request vòng duration=0)

| TIMEOUT | Strategy | killed | resolved | throughput | fp_rate | timeouts |
| --- | --- | --- | --- | --- | --- | --- |
| 3/5/10 | kill | 0 | 0 | 1.00 | 0.00 | 0 |
| 3/5/10 | retry | 0 | 0 | 1.00 | 0.00 | 0 |

Sample không trigger timeout: request tạo vòng có `duration=0`, resource gốc hết hạn (t=5/6/7) gỡ vòng trước khi chạm ngưỡng. Muốn demo deadlock trên sample cần tăng duration request vòng.

---

## 8. Phân tích (yêu cầu 6)

- **TIMEOUT nhỏ (kill)**: kill sớm → killed cao, throughput giảm (0.8), nhưng gỡ deadlock nhanh. Trên dataset này kill chỉ giết đúng 1 process trong vòng → fp_rate=0.
- **TIMEOUT lớn (kill, t=10)**: không kịp trigger trước khi resource hết hạn → 0 can thiệp, throughput=1 nhưng hệ thống "treo" lâu hơn.
- **Retry**: throughput cao (1.0, không giết ai) nhưng đánh đổi bằng **false positive cao** — t=3 cho fpr=0.80 (5 timeout, chỉ 1 đúng deadlock). Process chờ lâu bị retry liên tục dù không deadlock.
- **False positive xảy ra khi**: process chờ lâu vượt ngưỡng vì process khác **giữ tài nguyên lâu** (không phải kẹt chu trình). TIMEOUT càng nhỏ → càng nhiều false positive (t=3 fpr=0.8 vs t=5 fpr=0.5 vs t=10 fpr=0).
- **Tradeoff**: kill quyết liệt (giảm throughput, gỡ deadlock chắc) vs retry mềm (giữ throughput, nhưng nhiễu false positive và có nguy cơ livelock nếu retry mãi).

---

## 9. Test tự động

`tests/test_main.cpp` — harness tự viết (không framework ngoài). **44 checks, 0 fail**. Phủ:
- Parser: parse hợp lệ, lỗi số cột, action sai, sort theo time.
- Detector: `detectDeadlock` bắt chu trình; `isInDeadlock` phân biệt trong/ngoài vòng.
- Timeout: `waiting_time`, kill giải phóng resource + xóa pending, retry tăng `retryCount` + leo thang sang kill.
- Engine: throughput ≤ 1 (không double-count), kill gỡ deadlock.

Nối vào CMake qua `enable_testing()` + `add_test`.

---

## 10. Build & Run

```powershell
# CMake
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure

# Hoặc Makefile / g++ trực tiếp
make
.\timeout_strategy.exe data\three_process_deadlock.csv 3 kill
.\timeout_strategy.exe data\three_process_deadlock.csv 3 retry 1 3
```

CLI: `timeout_strategy <dataset.csv> [timeout] [kill|retry] [retry_delay] [max_retries]`.

---

## 11. Trạng thái & việc còn lại

Hoàn thành (build `-Wall -Wextra` sạch, test 44/44, chạy end-to-end):
- Parser, event loop, timeout (kill + retry), detector (WFG + DFS), CLI, metrics, tests.
- 3 bug logic ảnh hưởng số liệu báo cáo đã sửa.

Còn lại (ngoài scope yêu cầu chính):
- Per-event log dạng `Time 0: P1 requests R1 -> Granted` (Phase 1) chưa in — hiện chỉ in metrics tổng hợp.
- `data/sample_deadlock.csv` không trigger timeout — cân nhắc thêm dataset có request vòng `duration > 0` để demo trực tiếp trên sample.
- Rollback: cố ý bỏ qua theo phân công.
- Biểu đồ (throughput/fp_rate theo TIMEOUT) cho slide — dữ liệu bảng mục 7 đã sẵn để vẽ.
