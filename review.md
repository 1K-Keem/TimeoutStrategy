# Báo cáo — Chiến lược Timeout vs Phát hiện Deadlock

Môn: Hệ điều hành (Extra Assignment) · Ngôn ngữ: C++17 · Branch: `deadlock_detection`
Ngày: 2026-06-12

---

## 1. Tóm tắt

Hệ thống mô phỏng xử lý deadlock bằng **timeout** theo thời gian logic, đối chiếu với **deadlock detection** (Wait-For Graph) làm ground truth để đo false positive. Cài đặt 3 chiến lược xử lý timeout — **kill**, **retry**, **rollback** — và so sánh trên 3 mức TIMEOUT (3, 5, 10) qua 4 metric: killed processes, deadlock resolved, throughput, false positive rate.

Định nghĩa bắt buộc theo đề:
```
waiting_time = current_time - request_time
```
Mỗi event = 1 time unit. Khi `waiting_time >= TIMEOUT` → kích hoạt xử lý.

---

## 2. Kiến trúc hệ thống

| Thành phần | File | Vai trò |
| --- | --- | --- |
| Models | `include/Models.hpp` | Kiểu dữ liệu chung. |
| CSVParser | `src/CSVParser.cpp` | Đọc + validate dataset, sort theo `time`. |
| SimulationEngine | `src/SimulationEngine.cpp` | Đồng hồ logic, event loop, cấp/giải phóng resource, gom metrics. |
| TimeoutManager | `src/TimeoutManager.cpp` | Tính `waiting_time`, thực thi kill / retry / rollback. |
| DeadlockDetector | `src/DeadlockDetector.cpp` | Wait-For Graph + DFS phát hiện chu trình. |
| main | `src/main.cpp` | CLI: parse args → chạy engine → in metrics. |

Event loop mỗi time unit: giải phóng resource hết hạn → cấp pending cũ → xử lý event mới → áp dụng timeout → cấp lại pending → quét process hoàn thành.

---

## 3. Cơ chế timeout

`TimeoutManager::checkTimeouts` duyệt `pendingRequests`, chỉ xét process `Blocked`, tính `waitingTime = currentTime - request.requestTime`, trigger khi `>= timeout`. Constructor ném `std::invalid_argument` nếu `timeout < 1`.

Ground truth deadlock: `detector.isInDeadlock(pid)` kiểm tra **đúng process bị timeout có nằm trong chu trình** (reachability `pid → ... → pid`), không dùng cycle toàn cục — nhờ vậy false positive đúng định nghĩa đề.

### 3.1 Kill (`killProcess`)
State → `Terminated`, giải phóng toàn bộ `heldResources`, xóa mọi pending của process. Quyết liệt nhất, throughput của process đó = 0.

### 3.2 Retry (`retryRequest`)
Gỡ request, `retryCount += 1`. Vượt `maxRetries` → leo thang kill. Ngược lại → `Running`, hẹn `retryAfter = currentTime + retryDelay`, đẩy lại đúng request đang chờ. Chỉ xin lại tài nguyên đang chờ, không trả lại tài nguyên đang giữ.

### 3.3 Rollback (`rollbackProcess`) — bổ sung mới
`rollbackCount += 1`. Vượt `maxRollbacks` (mặc định 3) → leo thang kill (chống livelock). Ngược lại:
- Thu hồi **toàn bộ** `heldResources`.
- Xóa **mọi** pending của process.
- State → `New` (về trạng thái ban đầu).
- Engine gọi `replayProcess`: **re-inject toàn bộ request event** của process vào hàng đợi với `requestTime = currentTime`, reset `remainingEventCount`.

So sánh bản chất:

| | Kill | Retry | Rollback |
| --- | --- | --- | --- |
| Hủy process? | Có | Không | Không |
| Trả tài nguyên đang giữ? | Có | Không | Có |
| Phạm vi xin lại | — | 1 request đang chờ | Toàn bộ event (chạy lại từ đầu) |
| Throughput process đó | 0 | giữ | giữ |
| Rủi ro | mất throughput | livelock | livelock |
| Chặn rủi ro | — | `maxRetries` → kill | `maxRollbacks` → kill |

Để hỗ trợ rollback, engine lưu `processEvents_` (map process → danh sách event gốc) khi `registerEventSources`, dùng làm nguồn replay. Đây là thay đổi kiến trúc chính so với bản kill/retry.

---

## 4. Phát hiện deadlock (ground truth)

`WFGraph` — đồ thị chờ có hướng (process → process đang giữ tài nguyên nó chờ):
- `detectDeadlock()` — DFS bắt chu trình bất kỳ.
- `isInDeadlock(pid)` — reachability xác định process có thực sự trong chu trình.

Engine cập nhật đồ thị: thêm edge khi block, gỡ khi cấp resource / kill / rollback / complete.

---

## 5. Metrics

Mỗi `TimeoutRecord`: `timeoutEvents++`; `falsePositives++` nếu process không trong chu trình; `retryEvents`/`rollbackEvents`/`killedProcesses` theo loại; `deadlockResolved++` chỉ khi process thực sự deadlock và đồ thị **có chu trình trước → hết chu trình sau** khi xử lý.

Tổng hợp:
- `throughput = completedProcesses / totalProcesses` (luôn ∈ [0, 1]).
- `falsePositiveRate = falsePositives / timeoutEvents`.

---

## 6. Kết quả thí nghiệm

12 + 6 tổ hợp: 2 dataset × {kill, retry, rollback} × TIMEOUT {3, 5, 10}.

### 6.1 `data/three_process_deadlock.csv` (duration=10, có deadlock thật)

| TIMEOUT | Strategy | killed | rollback | resolved | throughput | fp_rate | timeouts |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 3 | kill | 1 | 0 | 1 | 0.80 | 0.00 | 1 |
| 5 | kill | 1 | 0 | 1 | 0.80 | 0.00 | 1 |
| 10 | kill | 0 | 0 | 0 | 1.00 | 0.00 | 0 |
| 3 | retry | 0 | 0 | 1 | 1.00 | 0.80 | 5 |
| 5 | retry | 0 | 0 | 1 | 1.00 | 0.50 | 2 |
| 10 | retry | 0 | 0 | 0 | 1.00 | 0.00 | 0 |
| 3 | rollback | 0 | 1 | 1 | 1.00 | 0.00 | 1 |
| 5 | rollback | 0 | 1 | 1 | 1.00 | 0.00 | 1 |
| 10 | rollback | 0 | 0 | 0 | 1.00 | 0.00 | 0 |

### 6.2 `data/sample_deadlock.csv` (request vòng duration=0)

Mọi cấu hình (cả 3 strategy, mọi TIMEOUT): 0 timeout, throughput=1. Request tạo vòng có `duration=0`, resource gốc hết hạn (t=5/6/7) gỡ vòng trước khi chạm ngưỡng → không trigger timeout. Muốn demo deadlock trên sample cần tăng duration request vòng.

---

## 7. Phân tích

### 7.1 Ảnh hưởng của TIMEOUT
- **Nhỏ (t=3)**: trigger sớm, can thiệp nhiều. Với retry → 5 timeout, fp_rate 0.80 (giết/xin lại nhầm process chỉ chờ lâu). Với kill → giải quyết deadlock nhanh nhưng mất 1 process (throughput 0.8).
- **Vừa (t=5)**: retry fp_rate giảm còn 0.50. Cân bằng hơn.
- **Lớn (t=10)**: không kịp trigger trước khi resource hết hạn → 0 can thiệp, throughput=1 nhưng hệ thống "treo" lâu hơn trước khi deadlock tự gỡ.

### 7.2 So sánh 3 chiến lược (t=3, dataset deadlock)
- **Kill**: throughput 0.80, fp_rate 0 — giải quyết chắc chắn nhưng hy sinh 1 process.
- **Retry**: throughput 1.00, fp_rate 0.80 — giữ được mọi process nhưng nhiễu false positive cao, nhiều lần xin lại vô ích.
- **Rollback**: throughput 1.00, fp_rate 0, chỉ 1 lần can thiệp — gỡ deadlock bằng cách trả tài nguyên + chạy lại, không hy sinh process. Trên dataset này là chiến lược tốt nhất cả về throughput lẫn độ chính xác.

### 7.3 False positive xảy ra khi nào
Khi process chờ vượt ngưỡng vì process khác **giữ tài nguyên lâu** (không phải kẹt chu trình). TIMEOUT càng nhỏ → càng nhiều false positive (retry: t=3 fpr=0.8 > t=5 fpr=0.5 > t=10 fpr=0).

### 7.4 Tradeoff tổng quát
- Kill: quyết liệt, giảm throughput, gỡ deadlock chắc.
- Retry: mềm, giữ throughput, nhưng nhiễu false positive và nguy cơ livelock.
- Rollback: dung hòa — giữ throughput như retry, nhưng trả toàn bộ tài nguyên nên phá vỡ chu trình hiệu quả hơn. Đổi lại tốn chi phí chạy lại từ đầu và vẫn cần `maxRollbacks` chặn livelock.

---

## 8. Các lỗi đã phát hiện & sửa (trước khi thêm rollback)

- **Double-count throughput**: process complete xong nhưng pending retry còn trong hàng đợi → bị cấp lại resource → complete lần nữa → throughput 1.6. Sửa: `grantPendingRequests` purge pending của process `!isAlive()`. Throughput về [0, 1].
- **False positive sai**: trước dùng cycle toàn cục → process ngoài vòng vẫn bị tính true positive. Sửa: `isInDeadlock(pid)` theo reachability.
- **deadlock_resolved sai**: trước đếm theo mỗi record có cycle. Sửa: chỉ tăng khi "có chu trình → hết chu trình" quanh lúc xử lý.
- **Warning + vệ sinh**: dọn `-Wmissing-field-initializers`; `.gitignore` + gỡ binary khỏi tracking; `Makefile clean` cross-platform.

---

## 9. Kiểm thử

`tests/test_main.cpp` — harness tự viết (không framework ngoài). **61 checks, 0 fail**. Phủ:
- Parser: parse hợp lệ, lỗi số cột, action sai, sort theo time.
- Detector: `detectDeadlock` bắt chu trình; `isInDeadlock` phân biệt trong/ngoài vòng.
- Kill: giải phóng resource + xóa pending, tính `waiting_time`.
- Retry: tăng `retryCount`, leo thang kill khi vượt `maxRetries`.
- Rollback: state → `New`, giải phóng resource, `rollbackCount` tăng, leo thang kill khi vượt `maxRollbacks`.
- Engine: throughput ≤ 1 (kill/retry/rollback); rollback hội tụ không loop vô hạn và phát sinh `rollbackEvents`.

---

## 10. Build & Run

```powershell
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```
Hoặc `make` / g++ trực tiếp.

CLI:
```text
timeout_strategy <dataset.csv> [timeout] [kill|retry|rollback] [retry_delay] [max_retries|max_rollbacks] [-v|--verbose] [-c|--compare]
```
Ví dụ:
```powershell
.\timeout_strategy.exe data\three_process_deadlock.csv 3 kill
.\timeout_strategy.exe data\three_process_deadlock.csv 3 retry 1 3
.\timeout_strategy.exe data\three_process_deadlock.csv 3 rollback 1 3
.\timeout_strategy.exe data\three_process_deadlock.csv 3 kill -v
.\timeout_strategy.exe data\three_process_deadlock.csv 3 --compare
```

### Chế độ so sánh (cờ `-c`)
Chạy cả 3 chiến lược trên cùng dataset + TIMEOUT, in bảng metrics một lần (đỡ phải chạy lần lượt):
```text
strategy   completed  killed retries rollbacks  resolved      fp  throughput   fp_rate
--------------------------------------------------------------------------------------
kill               4       1       0         0         1       0       0.800     0.000
retry              5       0       5         0         1       4       1.000     0.800
rollback           5       0       0         1         1       0       1.000     0.000
```

### Per-event log (cờ `-v`)
In từng sự kiện theo time unit (request/grant/block/release, timeout, deadlock resolved, complete) — đáp ứng yêu cầu log Phase 1 của đề. Ví dụ (kill, t=3):
```text
Time 0: P1 requests R1 -> Granted
Time 3: P1 requests R2 -> Blocked (held by P2)
Time 6: P1 TIMEOUT (waited 3, deadlock) -> Killed
Time 6: Deadlock resolved (cycle broken)
Time 6: P3 acquires R1 -> Granted (was waiting)
Time 7: P5 -> Completed
```

---

## 11. Trạng thái & việc còn lại

Hoàn thành (build `-Wall -Wextra` sạch, test 61/61, chạy end-to-end):
- Parser, event loop, 3 chiến lược timeout (kill + retry + rollback), detector (WFG + DFS), CLI, metrics, per-event log (cờ `-v`), tests.
- 3 bug logic ảnh hưởng số liệu báo cáo đã sửa.
- Rollback: thu hồi tài nguyên + replay toàn bộ event + leo thang kill khi vượt `maxRollbacks`.

Còn lại:
- `data/sample_deadlock.csv` không trigger timeout — cân nhắc thêm dataset request vòng `duration > 0` để demo trực tiếp.
- Biểu đồ throughput / fp_rate theo TIMEOUT cho slide — dữ liệu bảng mục 6 đã sẵn để vẽ.



