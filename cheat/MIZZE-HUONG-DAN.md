# Mizze Free Fire — bản No Key + anti-ban 4 lớp

Dựng từ source `Documents/dylib-main/intex dylib` (TẬP 33e, đã gỡ license),
thêm lớp anti-ban của riêng tao và bỏ client archive 69 MB của hãng.

| thứ | trạng thái |
|---|---|
| license / key | **đã gỡ** — `sources/APIClientStub.mm` thay toàn bộ client của hãng, `apiclient_paid()` gọi callback ngay |
| token trong source | **xoá** (`StealthDylibInit.mm`) |
| `libAPIClient.a` 69 MB | **không link** nữa |
| anti-ban lớp 1 — mem spoof | `anticheat/Spoof.mm`: interpose `open`/`openat`/`fopen`/`stat`/`lstat`/`readdir`/`getenv`/`dlopen`; `/proc/self/maps` được lọc và trả về bản đã lọc qua fd tạm đã unlink; `DYLD_*` bị dấy khỏi `environ` |
| anti-ban lớp 2 — value guard | `anticheat/Hooks.cpp`: 43 hook managed (đổi `Il2CppMethodInfo::methodPointer`, **không g byte vào code page** nên checksum của game vẫn khớp) + 2 static field của `COW.GameVarDef` set qua runtime offset → mem check định kỳ không chạy |
| anti-ban lớp 3 — server-safe | `esp/Core/RunSpeed.h`: tốc độ chỉ trong ngưỡng người (≤1.5×); aimbot đã có gate hướng + hysteresis sẵn |
| anti-ban lớp 4 — sanitizer | `anticheat/Sanitizer.mm`: `ptrace(PT_DENY_ATTACH)`, dọn `environ`, xoá file pid/log, đóng-on-exec mọi fd |
| giữ hook | re-write pointer mỗi 2s (chống hotfix IFix) |
| audit | log mọi dylib ngoài `/System` mà game nạp → tìm SDK native thật |
| profile probe | `anticheat/ProfileProbe.mm`: đọc offset thật của field trong game đang chạy ra log |

## Build

GitHub Actions → **build-mizze** → **Run workflow** → variant `th` → chờ 3–8 phút.

Artifact: **Mizze-install** (zip) gồm

```
libsubsystem.dylib    <- dylib đã ký (ldid, entitlements đầy đủ)
iOSViet.plist         <- filter: chỉ inject vào com.dts.freefireth / com.dts.freefiremax
```

## Cài lên iPhone

1. Copy **cả 2 file** vào `/Library/MobileSubstrate/DynamicLibraries/`
2. **Respring** (hoặc reboot)
3. Mở **Free Fire** → menu hiện sau ~8 giây, **không cần key**

## Đọc log

Trên máy dùng app **Console** (lọc process `FreeFire`), hoặc qua SSH:

```bash
idevicesyslog | grep -i mizze
```

| dòng log | nghĩa |
|---|---|
| `[Mizze] bundle com.dts.freefireth version 1.132.1 build 2019121207` | đúng bản game, profile khớp |
| `[Mizze] runtime ready` | IL2CPP đã init, hook cài được |
| `[Mizze] hooks installed 43, failed 0` | 43 hook đã ăn (2 static field tính trong đó) |
| `[Mizze] profile COW.GamePlay.Player.<field> @0x…` | offset thật của field, đối chiếu với profile |
| `[Mizze] dlopen /…` | dylib lạ mà game nạp — tìm SDK native |
| `[Mizze] blocked native SDK load: …` | SDK bị chặn |

## Bản game đã xác nhận

| | |
|---|---|
| bundle id | `com.dts.freefireth` |
| version | `1.132.1` |
| build | `2019121207` |
| image | `UnityFramework.framework/UnityFramework`, arm64, 199 MB |

Profile offset trong source (`esp/Core/GameProfileFields.h`, `GameProfileMethods.h`)
được sinh từ đúng image 1.132.1 này → **đừng thay bằng bảng offset khác**.
