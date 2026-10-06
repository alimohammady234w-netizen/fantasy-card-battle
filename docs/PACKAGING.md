# ساخت و بررسی نسخهٔ مستقل ARCANA

## وضعیت واقعی این تحویل

| بررسی | نتیجه |
|---|---|
| اجرای سورس با Python 3.11.2 / Pygame 2.6.1 | موفق |
| مجموعهٔ تست‌های خودکار | ۱۶۸ تست موفق |
| Self-test داخلی از روی سورس | هر ۲۶ بررسی موفق |
| Preflight ساخت در sandbox | ناموفق؛ Python قدیمی‌تر از 3.12 و نبود shared library |
| تست محلی با Python 3.12 | انجام نشده؛ دانلود runtime به خطای شبکه خورد |
| ساخت و اجرای باینری مستقل Linux | انجام نشده |
| ساخت و اجرای Windows EXE | انجام نشده |
| اجرای GitHub Actions | در این جلسه اجرا نشده |

گزارش‌های **واقعاً تولیدشده**:

- [`runtime-verification.json`](runtime-verification.json): وضعیت `passed` با `frozen: false`؛ این گزارش اجرای سورس است، نه EXE.
- [`build-environment.json`](build-environment.json): وضعیت `failed` و علت شکست پیش‌نیازها؛ هیچ artifact موفقی در آن گزارش نشده است.
- [`TEST_REPORT.md`](TEST_REPORT.md): گزارش کلی آزمون‌ها.

دریافت از python.org، فایل‌های runtime روی GitHub Release و مخزن بستهٔ Python نیز در sandbox با خطای TLS متوقف شد. از فایل اجرایی ناشناس جایگزین استفاده نشده است. این محدودیت را با ادعای تست Windows یا تولید ZIP آماده پنهان نمی‌کنیم.

## ۱. ساخت روی Windows

Python **3.12 یا جدیدتر** را با نصب‌کنندهٔ استاندارد python.org نصب کنید. در پوشهٔ پروژه:

```bat
python --version
python -m venv venv
venv\Scripts\activate
python -m pip install -r requirements-build.txt
python tools\build_release.py --check
python tools\build_release.py
```

پس از نصب وابستگی‌ها، اجرای دوبارکلیکی **`build_game.bat`** نیز همین مسیر را انجام می‌دهد.

`requirements-build.txt` وابستگی‌های Build را از اجرای عادی جدا نگه می‌دارد و نسخه‌های اصلی Pygame و PyInstaller را مشخص می‌کند. ابزار Build خودش چیزی از اینترنت نصب نمی‌کند.

### ترتیب بررسی‌ها

1. Python حداقل 3.12، سیستم‌عامل میزبان Windows/Linux، وجود Pygame/PyInstaller و shared library لازم بررسی می‌شوند.
2. دیتابیس‌ها و تنظیمات با `validate_data.py` بررسی می‌شوند.
3. همهٔ تست‌های `unittest` اجرا می‌شوند.
4. Self-test کامل سورس اجرا می‌شود.
5. PyInstaller فایل مستقل را در پوشهٔ staging می‌سازد.
6. **همان فایل مستقل** با `--self-test` از یک پوشهٔ موقت خارج از پروژه اجرا می‌شود.
7. ابزار منتظر پایان فرآیند می‌ماند و هم exit code و هم گزارش JSON را بررسی می‌کند؛ `frozen` باید `true` باشد و هیچ بررسی لازم غایب یا ناموفق نباشد.
8. فقط پس از موفقیت، خروجی قابل توزیع، manifest و SHA-256 منتشر می‌شوند.

گزارش قدیمیِ self-test پیش از اجرای جدید حذف می‌شود؛ یک فایل JSON موفقِ باقی‌مانده از Build قبلی نمی‌تواند شکست اجرای تازه را پنهان کند.

## ۲. خروجی‌های مورد انتظار، فقط پس از Build موفق

```text
dist/
├── CardGame.exe                       # Windows؛ در Linux نام فایل CardGame است
├── manifest.json
├── runtime-report.json                # گزارش اجرای واقعی باینری با frozen=true
├── CardGame-Windows-AMD64.zip          # نام وابسته به OS/architecture میزبان است
└── CardGame-Windows-AMD64.zip.sha256
```

داخل ZIP: فایل بازی، `README.txt` مخصوص کاربر نهایی، manifest و گزارش runtime قرار می‌گیرند. کاربر مقصد Python لازم ندارد. داده‌ها و Assetها داخل خروجی one-file بسته‌بندی می‌شوند.

`manifest.json` شامل نسخهٔ Python/Pygame/PyInstaller، سیستم‌عامل، معماری، اندازه و SHA-256 فایل اجرایی و تعداد بررسی‌های runtime است. فایل `.sha256` کناری، checksum خود ZIP را دارد. برای مثال روی Windows:

```powershell
Get-FileHash .\dist\CardGame-Windows-AMD64.zip -Algorithm SHA256
Get-Content .\dist\CardGame-Windows-AMD64.zip.sha256
```

Checksum برای تشخیص تغییر فایل است، نه اثبات هویت منتشرکننده. برای توزیع عمومی، مجوزهای خود پروژه و اجزای ثالث را بررسی و در صورت نیاز code signing معتبر انجام دهید. ZIP و EXE بدون امضای دیجیتال ممکن است هشدار امنیتی سیستم‌عامل داشته باشند.

## ۳. ساخت روی Linux

از Python 3.12+ دارای کتابخانهٔ مشترک استفاده کنید:

```bash
python3.12 -m venv venv
source venv/bin/activate
python -m pip install -r requirements-build.txt
./build_game.sh --check
./build_game.sh
```

خروجی Linux، **EXE ویندوز نیست**. PyInstaller معمولاً cross-compile نمی‌کند؛ هر خروجی باید روی سیستم‌عامل مقصد ساخته شود. برای توزیع Linux، سازگاری glibc و کتابخانه‌های سیستم مقصد نیز نیازمند آزمون است. نسخهٔ فعلی این ابزار فقط بسته‌بندی native برای Windows و Linux را هدف می‌گیرد؛ اجرای سورس روی macOS موضوع جداگانه‌ای است.

## ۴. تست داخلی بازی

### از روی سورس

```bash
python main.py --self-test --report build/runtime.json
```

### از خروجی مستقل Windows

در PowerShell، برای منتظرماندن تا پایان EXE پنجره‌ای:

```powershell
$p = Start-Process .\CardGame.exe -ArgumentList '--self-test','--report','runtime.json' -Wait -PassThru
$p.ExitCode
Get-Content runtime.json
```

### از خروجی مستقل Linux

```bash
./CardGame --self-test --report runtime.json
```

تست به‌طور خودکار SDL dummy video/audio را فعال می‌کند و از یک پروفایل موقت استفاده می‌کند. **Save شخصی کاربر خوانده یا ویرایش نمی‌شود.** خروجی‌ها:

- exit code `0`: بررسی‌ها موفق و در صورت درخواست گزارش نوشته شده است.
- exit code `1`: بررسی runtime شکست خورده است؛ جزئیات داخل گزارش هستند.
- exit code `2`: خطای آرگومان یا ناتوانی در نوشتن گزارش.

بررسی‌ها شامل داده‌های بسته، فونت و fallback تصویر، هر ۹ صدای همراه، دو موسیقی، صفحات، کلیک‌های Play/انتخاب Attribute، Resume بدون تکرار پاداش، پایان Quick/Classic/Practice، دفترچه، پک، دستاورد، ارتقا، Deck Builder، Save/Load و رندر 720p/1080p هستند.

این Self-test برای محتوای پیش‌فرض همراه پروژه طراحی شده است. تغییر اساسی در نام Mode/Pack/Achievementهای پایه یا حذف عمدی صداهای همراه ممکن است به تطبیق تست نیاز داشته باشد؛ اجرای عادی بازی همچنان fallbackهای Asset را دارد.

### تست سبک راه‌اندازی

```bash
python main.py --headless --smoke 60
```

برخلاف `--self-test`، تست smoke عادی از مسیر Save معمول بازی استفاده می‌کند. `--headless` بدون `--smoke` یا `--self-test` رد می‌شود تا فرآیند بی‌پنجره بدون خروج خودکار باقی نماند. تعداد فریم باید مثبت باشد. گزینهٔ `--report` فقط با `--self-test` مجاز است.

## ۵. گزارش‌ها و خطاهای Build

- `build/release-report.json`: نتیجهٔ نهایی و پیش‌نیازها/مرحله‌ها.
- `build/release-checks/`: Logها و گزارش‌های سورس/باینری.
- `build/release-stage/`: فایل ساخته‌شدهٔ موقت پیش از انتشار.
- `build/` و `dist/` فایل‌های تولیدی و خارج از Git هستند.

مسیر خروجی و گزارش را می‌توان تغییر داد:

```bash
python tools/build_release.py --output dist --report build/my-release-report.json
```

`--check` فقط پیش‌نیازها را بررسی می‌کند. وضعیت `preflight_passed` به معنی ساخت EXE نیست و هیچ ZIP تولید نمی‌کند. هر خطای بررسی، Build یا اجرای باینری باعث exit code غیرصفر می‌شود. اگر Build قبلی موجود باشد، به گزارش و checksum همان Build توجه کنید؛ وجود یک فایل قدیمی در `dist` به معنی موفقیت تلاش جدید نیست.

خطاهای متداول:

- **Python کمتر از 3.12:** محیط مجازی را با Python مناسب دوباره بسازید.
- **PyInstaller/Pygame پیدا نشد:** requirements-build را با Python همان venv نصب کنید.
- **shared Python library پیدا نشد:** runtime کامل/استاندارد نصب کنید؛ روی Linux نسخهٔ دارای libpython متناظر لازم است. فقط کپی‌کردن یک DLL/so ناشناس راه‌حل قابل اعتماد نیست.
- **Self-test سورس ناموفق:** داده‌ها، Assetها و traceback گزارش را بررسی کنید؛ هنوز سراغ انتشار نروید.
- **Self-test باینری ناموفق:** Log بسته‌بندی، فایل‌های data/assets داخل spec، DLLها و مسیرهای Resource را بررسی کنید.
- **خطای مجوز گزارش یا خروجی:** پوشهٔ قابل نوشتن انتخاب کنید.

## ۶. CI و بررسی انسانی

گردش‌کار GitHub Actions، تست و Self-test سورس را روی Python 3.12/3.13 در Windows/Linux اجرا می‌کند. در jobهای 3.12، همین ابزار Build را اجرا و ZIP/manifest/checksum را **فقط در صورت موفقیت** آپلود می‌کند. گزارش‌ها تا جایی که job اجازه دهد حتی در شکست نیز آپلود می‌شوند. اجرای این workflow نیاز به push یا اجرای دستی روی GitHub دارد؛ در این جلسه اجرا نشده است.

آزمون‌های واحد ابزار Build از subprocess و باینری‌های ساختگی در پوشهٔ موقت استفاده می‌کنند تا ترتیب مراحل و جلوگیری از انتشار خروجی تأییدنشده را بسنجند؛ این تست‌ها **مدرک ساخت واقعی EXE نیستند**.

پیش از انتشار عمومی، روی دستگاه واقعی نیز بررسی کنید:

- اجرای عادی پنجره و ورود به Main Menu، بدون نصب Python در مقصد.
- صدای واقعی، Volume، Fullscreen، رزولوشن و DPI.
- یک Match کامل، Suspend/Resume پس از بستن برنامه، دریافت جایزه و Save/Load.
- اجرای بازی از مسیری دارای فاصله و نویسه‌های غیرلاتین.
- هشدارهای SmartScreen/آنتی‌ویروس، مجوز نوشتن Save و سازگاری سیستم مقصد.

Self-test headless جای آزمون سخت‌افزار واقعی یا امضای نرم‌افزار را نمی‌گیرد.
