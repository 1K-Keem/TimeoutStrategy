# OS - Extra Assignment

| **Thành viên** | **Vai trò** | **Nhiệm vụ chính** |
| --- | --- | --- |
| Quang | **Core System & Event Loop** | Đọc file dataset, xây dựng đồng hồ logic (time unit), quản lý danh sách Process, Resource, và vòng lặp sự kiện (Event Loop). |
| Kim | **Timeout Mechanism (Yêu cầu 2 & 3)** | Tính toán `waiting_time`. Cài đặt các chiến lược xử lý khi Timeout: **Kill process** và **Retry request** (bỏ qua rollback nếu quá phức tạp, tập trung 2 cái này cho tốt). |
| Khánh | **Deadlock Detection (Ground Truth)** | Cài đặt thuật toán phát hiện deadlock (VD: Wait-For Graph - WFG). **Lưu ý:** Cần cái này chạy ngầm để biết quá trình bị timeout có *thực sự* đang bị deadlock hay không (phục vụ tính False Positive). |
| Khoa | **Data, Metrics & Report (Yêu cầu 4, 5, 6)** | Tạo thêm dataset (kịch bản nhỏ/lớn, kịch bản không có deadlock), thu thập metrics (log data), vẽ biểu đồ và viết báo cáo/slide phân tích. |

### Lộ trình thực hiện

### Phase 1: Setup & Core Engine

- **Mục tiêu:** Hệ thống có thể chạy giả lập đọc từng dòng dataset theo thời gian logic.
- **Công việc:**
    - Thống nhất ngôn ngữ lập trình (Khuyên dùng **Python** vì xử lý text dễ, vẽ biểu đồ pandas/matplotlib tiện; hoặc **C++/Java** nếu muốn hiệu năng).
    - Setup Git/GitHub.
    - **Thành viên 1:** Viết bộ parser đọc dataset. Xây dựng class `Process` (state: running, blocked, terminated), class `Resource` (state: free, allocated to...).
    - **Thành viên 1:** Tạo hàm `step()` - mỗi lần gọi là 1 time unit trôi qua.
- **Kết quả:** In ra được log dạng: `Time 0: P1 requests R1 -> Granted`.

### Phase 2: Cài đặt Deadlock Handling

- **Mục tiêu:** Hệ thống biết xử lý khi có timeout và biết phát hiện deadlock ngầm.
- **Công việc:**
    - **Thành viên 2:** Thêm thuộc tính `request_time` vào Process khi nó bị block. Ở mỗi time unit, duyệt danh sách block, nếu `current_time - request_time >= TIMEOUT` thì kích hoạt cơ chế (Thử trước cơ chế `Kill`: giải phóng toàn bộ resource P đang giữ và xóa P).
    - **Thành viên 3:** Xây dựng đồ thị cấp phát tài nguyên (Resource Allocation Graph). Viết hàm DFS/kiểm tra chu trình để phát hiện deadlock đang diễn ra.
    - **Thành viên 4:** Bắt đầu tạo thêm các file `.csv` dataset có kịch bản phức tạp hơn (ví dụ: chu trình 3 process P1->P2->P3->P1).
- **Kết quả:** Hệ thống chạy qua ngưỡng timeout và tự động giải phóng tài nguyên.

### Phase 3: Metrics & Experiments

- **Mục tiêu:** Thu thập đủ 4 loại metrics yêu cầu chạy trên $\ge$ 3 giá trị TIMEOUT.
- **Công việc:**
    - **Thành viên 2:** Cài đặt thêm chiến lược `Retry` (sau khi timeout, rút lại request, đợi một khoảng random rồi xin lại).
    - **Thành viên 3 & 4 phối hợp tính False Positive:**
        - Khi 1 process bị Timeout, gọi hàm Deadlock Detection của Thành viên 3.
        - Nếu Detection = TRUE (thực sự có chu trình) -> True Positive.
        - Nếu Detection = FALSE (chỉ là do nó đợi quá lâu vì quá trình khác giữ tài nguyên lâu) -> **False Positive**.
    - **Thành viên 4:** Chạy kịch bản với `TIMEOUT = 3, 5, 10` (ví dụ vậy). Xuất kết quả ra file log tổng hợp.
- **Kết quả:** Bảng số liệu so sánh 3 mức TIMEOUT cho 2 chiến lược (Kill, Retry).

### Phase 4: Phân tích & Báo cáo

- **Mục tiêu:** Trả lời các câu hỏi trong Yêu cầu 6.
- **Công việc cả nhóm:**
    - **Phân tích TIMEOUT nhỏ:** Quá trình bị kill liên tục -> số killed processes cao -> throughput giảm do phải làm lại -> False positive rate cao (chưa kịp chạy đã bị giết).
    - **Phân tích TIMEOUT lớn:** Ít bị false positive -> nhưng hệ thống bị "treo" lâu trước khi giải quyết deadlock -> deadlock resolved chậm.
    - Vẽ biểu đồ so sánh: Trục X là `TIMEOUT`, Trục Y là các metrics (Throughput, False Positive Rate...).
    - Hoàn thiện báo cáo & Slide thuyết trình.

### Một số lưu ý quan trọng (Bí kíp để được điểm cao)

1. **Hiểu đúng về `duration`:** Trong dataset mẫu, `duration` là thời gian **giữ tài nguyên sau khi được cấp phát**. Ví dụ: P1 xin R1 lúc `time=0`, lấy được luôn, nó sẽ giữ R1 trong 5 time unit (từ t=0 đến t=5). Đến `time=5`, P1 phải tự động nhả R1 ra.
2. **Định nghĩa "Giải quyết thành công Deadlock":** Sau khi kill 1 process tham gia vào deadlock, các process còn lại lấy được tài nguyên và hoàn thành. Hãy log lại khoảnh khắc "Đồ thị từ có chu trình -> hết chu trình".
3. **Cách dễ nhất để làm Rollback (nếu muốn làm):** Rollback thực chất gần giống Kill. Khác ở chỗ, Kill là hủy luôn Process (throughput = 0), còn Rollback là tịch thu lại tài nguyên, trả Process về `time=0` (hoặc trạng thái ban đầu) để nó chạy lại từ đầu.