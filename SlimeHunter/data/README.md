# Thư mục `data/` - dữ liệu người chơi (CSV)

Game tự tạo hai file này (kèm dòng tiêu đề) ở lần chạy đầu tiên nếu chúng chưa tồn tại.
Bạn có thể mở bằng Notepad/Excel để quan sát, nhưng nên **đóng game trước khi sửa tay**.

| File | Nội dung |
|---|---|
| `users.csv` | `user_id,username,password,created_at` |
| `player_progress.csv` | `user_id,current_level,max_level_unlocked,player_max_hp,player_damage,total_kills,total_score,updated_at` |

## Dữ liệu lâu dài và dữ liệu trong phiên

* **Lâu dài (lưu CSV):** tài khoản, màn đang chơi, màn cao nhất đã mở khóa, máu tối đa, sát thương cơ bản, tổng quái đã hạ, tổng điểm.
* **Trong phiên (không lưu):** vị trí, máu hiện tại, cấp kinh nghiệm của lượt chơi, hồi chiêu, quái và hiệu ứng đang tồn tại.
* `max_level_unlocked = 6` nghĩa là đã thông quan cả 5 màn.

## Khi nào game ghi file

Không ghi mỗi khung hình. Chỉ ghi khi: tạo tài khoản, **hoàn thành màn (đúng một lần)**, Game Over, về menu từ màn tạm dừng,
đăng xuất, hoặc đóng game giữa lúc chơi. Mỗi lần ghi chỉ thay **đúng dòng của người chơi hiện tại**; dòng của người khác
được giữ nguyên văn. File được ghi ra `*.tmp` rồi mới thay thế file thật.

## Giới hạn đầu vào (để file CSV không bị hỏng)

* Username: 3-20 ký tự gồm chữ không dấu, số, `_` hoặc `-`; không phân biệt hoa/thường khi kiểm tra trùng.
* Mật khẩu: 4-32 ký tự in được, không có khoảng trắng, không có dấu phẩy `,` và ngoặc kép `"`.
* Dữ liệu đọc từ file được kiểm tra kiểu và khoảng giá trị. Dòng sai định dạng bị bỏ qua (dùng dữ liệu mặc định), game không crash.

## CẢNH BÁO BẢO MẬT

**Mật khẩu đang được lưu dạng văn bản thuần** (không băm) theo yêu cầu học tập, để bạn thấy rõ cách xử lý file.
Chỉ dùng cho dữ liệu thử nghiệm trên máy của bạn. **Không dùng mật khẩu thật hay mật khẩu bạn đang dùng ở nơi khác.**
Game không bao giờ in mật khẩu ra console/log. Việc lưu/kiểm tra mật khẩu nằm gọn trong hai hàm
`UserRepository::encodePassword()` và `UserRepository::verifyPassword()`: sau này chuyển sang băm chỉ cần sửa hai hàm này.
Các file `*.csv` đã được đưa vào `.gitignore` để tránh vô tình đưa tài khoản lên kho mã nguồn.
