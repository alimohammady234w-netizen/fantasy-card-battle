# فعال‌سازی Build ویندوز از حساب مالک مخزن

## وضعیت این مرحله

انتشار Commit کامل توسط اتصال GitHub App به دلیل نداشتن مجوز `workflows` رد شد.
به همین دلیل، کد بازی و ابزارهای Build روی شاخهٔ جلسه منتشر می‌شوند، اما فایل
`.github/workflows/test-and-build.yml` فعلاً از انتشار این اتصال کنار گذاشته شده است.
نسخهٔ کامل آن در پروژهٔ Arena و بستهٔ سورس قابل دریافت وجود دارد و متن دقیقش پایین آمده است.
شاخهٔ `main` تغییر نکرده است. هنوز هیچ Build ویندوز یا CI موفقی ادعا نمی‌شود.

## راه اول: اصلاح مجوز اتصال Arena

اتصال GitHub در Arena باید مجوز افزودن/ویرایش Workflows را داشته باشد. صرفاً اتصال
مجدد ممکن است کافی نباشد؛ در نصب GitHub App باید مجوز جدید ارائه و تأیید شده باشد.
اگر این مجوز در نصب App ارائه نمی‌شود، موضوع را با پشتیبانی Arena بررسی کنید یا از
راه دوم استفاده کنید. هیچ رمز، PAT، OAuth token یا کد ورود را در گفتگو ارسال نکنید.

## راه دوم: افزودن فایل از حساب مالک در GitHub

1. با حساب مالک وارد مخزن شوید:
   https://github.com/alimohammady234w-netizen/fantasy-card-battle
2. در انتخاب شاخه، دقیقاً **`arena/a4ca498e-fantasy-card-battle`** را انتخاب کنید؛
   روی `main` تغییری ندهید.
3. گزینهٔ **Add file → Create new file** را انتخاب کنید.
4. نام فایل را **`.github/workflows/test-and-build.yml`** قرار دهید.
5. محتوای YAML زیر را بدون علامت‌های fence کپی کنید.
6. فایل را مستقیماً روی همان شاخهٔ `arena/a4ca498e-fantasy-card-battle` Commit کنید.
7. در تب **Actions** اجرای **Test and build PC game** را بررسی کنید. اگر Actions
   برای مخزن غیرفعال است، ابتدا از حساب مالک آن را فعال کنید. اجرای runnerها تابع
   تنظیمات و محدودیت‌های حساب GitHub شماست.

این Workflow چهار محیط Linux/Windows × Python 3.12/3.13 را تست می‌کند. در jobهای
3.12 خروجی native همان سیستم ساخته و اجرا می‌شود. Artifact مربوط به Windows فقط
پس از موفقیت همهٔ بررسی‌ها ایجاد می‌شود؛ `CardGame.exe` داخل ZIP تحویل خواهد بود.
افزودن Workflow به‌تنهایی به معنی موفقیت Build نیست؛ باید نتیجهٔ اجرای آن دیده شود.

## محتوای دقیق Workflow

```yaml
name: Test and build PC game
on:
  push:
  pull_request:
  workflow_dispatch:
permissions:
  contents: read
jobs:
  test:
    strategy:
      fail-fast: false
      matrix:
        os: [ubuntu-latest, windows-latest]
        python: ['3.12', '3.13']
    runs-on: ${{ matrix.os }}
    timeout-minutes: 20
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with:
          python-version: ${{ matrix.python }}
      - run: python -m pip install -r requirements.txt
      - run: python tools/validate_data.py
      - run: python -m unittest discover -s tests -v
      - run: python main.py --self-test --report build/source-runtime.json
      - if: matrix.python == '3.12'
        run: python -m pip install -r requirements-build.txt
      - if: matrix.python == '3.12'
        run: python tools/build_release.py
      - if: always()
        uses: actions/upload-artifact@v4
        with:
          name: Verification-${{ runner.os }}-${{ matrix.python }}
          path: |
            build/source-runtime.json
            build/release-report.json
            build/release-checks/
          if-no-files-found: warn
      - if: success() && matrix.python == '3.12'
        uses: actions/upload-artifact@v4
        with:
          name: CardGame-${{ runner.os }}
          path: |
            dist/*.zip
            dist/*.sha256
            dist/manifest.json
            dist/runtime-report.json
          if-no-files-found: error
```

## ساخت بدون GitHub Actions

روی Windows با Python 3.12+ در پوشهٔ پروژه:

```bat
python -m venv venv
venv\Scripts\activate
python -m pip install -r requirements-build.txt
python tools\build_release.py
```

راهنمای کامل و محدودیت‌ها: [PACKAGING.md](PACKAGING.md).
