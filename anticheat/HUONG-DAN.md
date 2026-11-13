# Dùng TIZAntiCheat trong 6 bước

Tóm tắt một câu: **file `anticheat/build.yml` là toàn bộ dự án dylib**; GitHub
Actions sẽ tự biến nó thành `TIZAntiCheat.dylib`; bạn tải file `.dylib` đó về
máy tính và copy vào iPhone.

> Không cần mua máy Mac. Runner macOS của GitHub lo hết phần build.

---

## Bạn cần có

| thứ | bắt buộc? | ghi chú |
|---|---|---|
| Tài khoản GitHub | có | miễn phí, đăng ký 2 phút |
| iPhone đã jailbreak | có | dylib phải được inject vào game, cần jailbreak |
| Máy tính có trình duyệt | có | để upload code và tải artifact |

---

## Bước 1 — Tạo kho chứng (repo) trên GitHub

1. Vào **github.com**, đăng nhập.
2. Góc trên bên phải bấm nút **`+`** → **`New repository`**.
3. Điền:
   - **Repository name**: `tiz-anticheat` (tên gì cũng được)
   - **Public** → để **Private** nếu bạn không muốn ai thấy (vẫn chạy Actions
     bình thường).
   - **KHÔNG** tick vào *"Add a README file"* (để tránh xung đột).
4. Bấm **Create repository**.

## Bước 2 — Đẩy code lên GitHub

### Cách A: kéo thả trên web (dễ nhất, không cần git)

1. Trong repo vừa tạo, bấm nút **uploading an existing file**.
2. Kéo thả **thư mục `anticheat`** vào.
3. Kéo thả **thư mục `.github`** vào (thư mục có dấu chấm ở đầu, phải bật
   "Show hidden files" trong hộp thoại chọn file của Windows).
4. Kéo thả thêm **`anticheat_cheatsheet.txt`** nếu bạn muốn giữ bản ghi chú.
5. Kéo thả **`global-metadata.dat`** (54 MB) — *không bắt buộc*, thư mục này
   không cần nó để build; chỉ cần khi bạn muốn làm lại bảng hook từ game mới.
6. Ở ô commit bên dưới gõ: `first commit` → bấm **Commit changes**.

### Cách B: dùng git (nhanh hơn nếu bạn dùng git)

Mở terminal trong thư mục dự án (trên Windows: mở PowerShell tại đúng thư mục),
chạy:

```powershell
git init
git add anticheat .github .gitignore
git commit -m "anticheat module"
git branch -M main
git remote add origin https://github.com/<TÊN-USER-CỦA-BẠN>/tiz-anticheat.git
git push -u origin main
```

(Nếu hỏi user/password: dùng **Personal access token**, không dùng mật khẩu
GitHub. Tạo token ở Settings → Developer settings → Personal access tokens →
Fine-grained tokens, quyền `Contents: Read and write`.)

## Bước 3 — Bấm "build" trên GitHub

1. Vào repo → tab **Actions** (trên thanh ngang).
2. Bên trái chọn workflow **build-dylib**.
3. Bấm nút **Run workflow** (góc trên bên phải).
4. Nếu có ô chọn, để **toolchain = xcrun** (mặc định) → bấm **Run workflow**.
5. **Chờ 3–6 phút.** Tab workflow hiện:
   - `host-check` (một vòng xanh ✓ = pass)
   - `dylib` (một vòng xanh ✓ = pass)
   Nếu đỏ ✗ thì bấm vào job đỏ để đọc log, gửi log đó cho tao.

## Bước 4 — Tải file `.dylib` về

1. Ở lần chạy vừa rồi, cuộn xuống phần **Artifacts**, thấy:
   - `TIZAntiCheat-dylib`
   - `TIZAntiCheat-hooks`
2. Bấm vào `TIZAntiCheat-dylib` → GitHub tải về một file **.zip**.
3. Giải nén zip trên máy tính → bên trong có **`TIZAntiCheat.dylib`** (khoảng
   vài chục KB).

## Bước 5 — Copy `.dylib` vào iPhone

Cần iPhone đã jailbreak. Cách nhanh nhất nếu bạn dùng **SSH/SFTP** (WindSFTP,
iFile, Cyberduck…):

```
đường dẫn đích trên iPhone:
/Library/MobileSubstrate/DynamicLibraries/
```

Copy `TIZAntiCheat.dylib` vào đó. Xong phải **reboot máy** (hoặc respring) để
Substrate nạp lại danh sách dylib.

> Cách làm trên điện thoại: mở **Filza**, đi tới
> `/Library/MobileSubstrate/DynamicLibraries/`, dán file vào.

## Bước 6 — Mở game và xem log

1. Mở **Free Fire**.
2. Xem log để biết hook có chạy không. Cách xem: dùng **Console** app trên
   iPhone (lọc theo tên process của game), hoặc `idevicesyslog` trên máy tính.
3. Trong log sẽ thấy các dòng bắt đầu bằng `[TIZAntiCheat]`:
   ```
   [TIZAntiCheat] il2cpp API bound from /var/containers/.../UnityFramework
   [TIZAntiCheat] hooked COW.GameFacade.IsEmulator (method info 0x...)
   [TIZAntiCheat] installed 13, skipped 2, failed 0
   ```
   - `installed 13` → 13 hook ăn ✓
   - `failed 0` → không hook nào lỗi ✓
   - `unresolved ...` → game build này khác tên/class → gửi log cho tao để sửa bảng hook.

---

## Nếu bị lỗi

| triệu chứng | nguyên nhân | cách sửa |
|---|---|---|
| Không thấy tab **Actions** | repo chưa push workflow lên | kiểm tra `.github/workflows/build-dylib.yml` đã được commit chưa |
| Workflow báo *"no sources_inline section"* | `build.yml` bị hỏng khi upload | tải lại `build.yml` từ máy (bản này có đủ source inline) |
| `host-check` đỏ với *"DRIFT"* | `source/` và `build.yml` lệch nhau | chạy `python tools/inline_sources.py` rồi commit lại |
| `dylib` đỏ ở bước compile | không có iOS SDK | kiểm tra runner là `macos-14`, đừng đổi |
| Build xong nhưng game không đổi gì | game không export `il2cpp_*` | cần bản **pattern** → cần binary game, gửi tao |
| Không thấy `[TIZAntiCheat]` trong log | dylib chưa được load | kiểm tra đúng thư mục + reboot máy |

## Sửa hành vi sau khi đã build

Không cần build lại: copy file này lên iPhone
`/var/mobile/Library/TIZAntiCheat/hooks.json`, nó **đè lên** bảng hook nhúng
trong dylib. Mẫu:

```json
{
 "hooks": [
  {"type":"COW.GameFacade","method":"IsEmulator","action":"return_false","token":100698482,"arg_count":0},
  {"type":"COW.EventLogger","method":"LogReportCheat","action":"skip","token":100679441,"arg_count":10}
 ]
}
```

`action` nhận một trong: `return_false`, `return_true`, `skip`,
`set_static_int`, `set_static_string`.
