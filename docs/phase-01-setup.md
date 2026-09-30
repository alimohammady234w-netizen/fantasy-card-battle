# مرحله ۱ — پروژه پایه (زیرساخت)

بازی کارتی فانتزی برای Android با Unreal Engine 5.8 — تک‌نفره، کاملاً Offline.

> **قانون مرحله ۱:** هیچ سیستم کارت، Battle، AI، Deck، Ability یا Shop ساخته نشده است.
> فقط زیرساخت پروژه (Project + C++ base classes + تنظیمات) آماده شده است.

---

## ۱) فایل‌های ساخته‌شده و مسیر دقیق هرکدام

```
<ریشه پروژه>/
├── FantasyCardBattle.uproject                      پروژه UE 5.8 (ماژول CardGame، پلتفرم Android)
├── .gitignore                                      نادیده‌گرفتن Binaries/Intermediate/Saved/...
├── Config/
│   ├── DefaultEngine.ini                           Maps & Modes + Mobile/Scalable + تنظیمات Android
│   ├── DefaultGame.ini                             نام و شناسه پروژه
│   └── DefaultInput.ini                            پیکربندی ورودی (بدون Binding هنوز)
├── Source/
│   ├── FantasyCardBattle.Target.cs                 Target بازی (برای Package گرفتن)
│   ├── FantasyCardBattleEditor.Target.cs           Target ادیتور (توسعه / PIE)
│   └── CardGame/
│       ├── CardGame.Build.cs                       قوانین build ماژول CardGame
│       ├── CardGame.h / CardGame.cpp               ماژول اولیه + دسته لاگ LogCardGame
│       ├── CardGameGameMode.h / CardGameGameMode.cpp         کلاس ACardGameGameMode
│       ├── CardGameGameInstance.h / CardGameGameInstance.cpp کلاس UCardGameGameInstance
│       └── CardGamePlayerController.h / .cpp                 کلاس ACardGamePlayerController
├── Content/
│   ├── Blueprints/  Cards/  Characters/  UI/
│   ├── Materials/  Textures/  Icons/
│   ├── Sounds/  Music/  VFX/  Data/
│   └── Maps/            ← دو مپ MainMenu و Battle را اینجا در ادیتور می‌سازید
└── docs/
    └── phase-01-setup.md                           همین مستند
```

### نام کلاس‌ها (نام‌گذاری استاندارد UE)

| نقش | کلاس | نوع | ماژول |
|---|---|---|---|
| GameMode | `ACardGameGameMode` | `AGameModeBase` | CardGame |
| GameInstance | `UCardGameGameInstance` | `UGameInstance` | CardGame |
| PlayerController | `ACardGamePlayerController` | `APlayerController` | CardGame |

پیشوندها طبق قاعده UE: `A` برای Actor-ها، `U` برای Object-ها، پیشوند پروژه `CardGame`،
نمای API برابر `CARDGAME_API`.

---

## ۲) تنظیماتی که از قبل در فایل‌های Config انجام شده

### `Config/DefaultEngine.ini`

- **GameDefaultMap و EditorStartupMap** = `/Game/Maps/MainMenu`
- **GlobalDefaultGameMode** = `/Script/CardGame.CardGameGameMode` ← GameMode پیش‌فرض
- **GameInstanceClass** = `/Script/CardGame.CardGameGameInstance`
- **TargetedHardwareClass=Mobile** + **DefaultGraphicsPerformance=Scalable**
  (معادل انتخاب Mobile/Tablet و Scalable 3D or 2D در ویزارد پروژه)
- **ریدر مقیاس‌پذیر:** `r.DefaultFeature.AutoExposure=False`، `MotionBlur=False`، `AntiAliasing=0`
- **Android:** `PackageName=com.cardgame.fantasycardbattle`، `Orientation=Portrait`،
  `MinSDKVersion=26` (Android 8+)، `TargetSDKVersion=35`، `bBuildForArm64=True`

### `Config/DefaultGame.ini`

- `ProjectID`، `ProjectName=Fantasy Card Battle`، شرکت و توضیحات.

### `Config/DefaultInput.ini`

- `DefaultTouchInterface=None` ← جوی‌استیک مجازی حذف (بازی کارتی = لمس/کشیدن روی UI).

---

## ۳) تنظیماتی که باید خودتان در Unreal Engine انجام دهید

### ۳-۱) پیش‌نیازها

1. **Unreal Engine 5.8** از Epic Games Launcher (گزینه Android هنگام نصب تیک خورده باشد).
2. **Visual Studio 2026 (نسخه 18.0+)** — پیشنهادی Epic برای 5.8 — یا Visual Studio 2022
   نسخه 17.14+ با workload های:
   - *Game development with C++*
   - *Desktop development with C++*
   - *.NET desktop development*
   - Windows SDK 10.0.22621 به بالا
3. فقط برای اجرای روی گوشی: **Android Studio Koala 2024.1.2 Patch 1** با
   Android SDK 35، NDK **r27c**، Build-tools 35.0.1 و JDK **21.0.3** (الزامات UE 5.8).

### ۳-۲) اتصال پروژه به انجین

1. روی `FantasyCardBattle.uproject` راست‌کلیک → **Switch Unreal Engine version…** → انتخاب نسخه 5.8
   (اگر خودکار پیدا نشد).
2. راست‌کلیک روی `.uproject` → **Generate Visual Studio project files**.

### ۳-۳) ساخت دو مپ (مراحل دقیق — فایل `.umap` باینری است و در ادیتور ساخته می‌شود)

> تا قبل از ساخت مپ‌ها، ادیتور هنگام باز شدن یک هشدار می‌دهد که مپ استارت‌آپ پیدا نشد.
> این طبیعی است و بعد از ساخت مپ‌ها برطرف می‌شود.

**مپ MainMenu:**

1. در Content Browser وارد پوشه `Content/Maps` شوید (خالی است — پوشه از قبل ساخته شده).
2. راست‌کلیک → **Level** → نام را `MainMenu` بگذارید و Enter بزنید.
3. دوبار روی آن کلیک کنید تا باز شود.
4. از **Window → Place Actors** یک **Player Start** را به صحنه اضافه کنید
   (مکان پیش‌فرض 0,0,0 کافی است). بدون PlayerStart، اسپاون Spectator هشدار می‌دهد.
5. `Ctrl+S` (یا File → Save Current As…) و مطمئن شوید مسیر `Content/Maps/MainMenu` است.
6. **Window → World Settings** → بخش GameMode:
   - **GameMode Override** را خالی بگذارید (از Global Default استفاده می‌کند) یا صراحتاً
     `CardGameGameMode` را انتخاب کنید — هر دو یک نتیجه می‌دهند.
   - **World Gravity / سایر تنظیمات** را دست نزنید.

**مپ Battle:**

1. مراحل ۲ تا ۵ بالا را با نام `Battle` تکرار کنید.
2. برای دیدن صحنه در هر دو مپ می‌توانید یک **Directional Light** و **Sky Atmosphere**
   اضافه کنید (اختیاری؛ برای مرحله ۱ لازم نیست).

### ۳-۴) تأیید تنظیمات در Project Settings (Edit → Project Settings)

| بخش | مقدار مورد انتظار |
|---|---|
| **Maps & Misc → Editor Startup Map** | `/Game/Maps/MainMenu` |
| **Maps & Misc → Game Default Map** | `/Game/Maps/MainMenu` |
| **Maps & Misc → Global Default GameMode** | `CardGame GameMode` |
| **Maps & Misc → Game Instance Class** | `Card Game Game Instance` |
| **Hardware → Targeted Hardware Class** | `Mobile` |
| **Hardware → Default Graphics Performance** | `Scalable` |
| **Android → Orientation** | `Portrait` |
| **Android → Package Name** | `com.cardgame.fantasycardbattle` |

(این مقادیر از قبل در Config ست شده‌اند؛ فقط بررسی کنید.)

---

## ۴) چگونه پروژه را Compile کنم؟

**روش A — از داخل ادیتور:**
پروژه را باز کنید؛ اگر کد تغییر کرده باشد خودکار compile می‌کند.
برای تغییرات زنده هنگام باز بودن ادیتور: `Ctrl+Alt+F11` (Live Coding).

**روش B — از Visual Studio:**
1. `FantasyCardBattle.uproject` راست‌کلیک → **Generate Visual Studio project files**.
2. فایل سالوشن تولیدشده را باز کنید.
3. کانفیگ سالوشن: **Development Editor | Win64**.
4. روی پروژه `FantasyCardBattleEditor` راست‌کلیک → **Build** (یا `Ctrl+Shift+B`).

**روش C — خط فرمان:**

```bat
"<مسیر انجین>\UE_5.8\Engine\Build\BatchFiles\Build.bat" FantasyCardBattleEditor Win64 Development -Project="<مسیر پروژه>\FantasyCardBattle.uproject" -WaitMutex
```

**Compile برای اندروید (در مرحله ۱ فقط در صورت نیاز):**

```bat
"<مسیر انجین>\Engine\Build\BatchFiles\Build.bat" FantasyCardBattle Android Development -Project="<مسیر پروژه>\FantasyCardBattle.uproject" -WaitMutex
```

اگر خطای `BuildSettingsVersion` یا `IncludeOrderVersion` گرفتید، یعنی انجین شما 5.8 نیست —
فایل‌های `.Target.cs` باید روی `V7` و `Unreal5_8` باشند (همین‌طور هستند).

---

## ۵) چگونه پروژه را اجرا و تست کنم؟

### در ادیتور (PIE)

1. ادیتور را باز کنید و `Content/Maps/MainMenu` را باز کنید (File → Open Level).
2. **Alt+P** (Play) — یک SpectatorPawn با دوربین در PlayerStart اسپاون می‌شود.
3. **Window → Output Log** را باز کنید و فیلتر را روی `LogCardGame` بگذارید.
   پیام‌های زیر باید دیده شوند (به‌ترتیب):

   ```
   [LogCardGame] CardGame module started.
   [LogCardGame] UCardGameGameInstance::Init - game instance initialized.
   [LogCardGame] ACardGameGameMode::BeginPlay - map 'MainMenu' started.
   [LogCardGame] ACardGamePlayerController::BeginPlay - player controller ready.
   ```

4. همین تست را روی مپ `Battle` تکرار کنید.

### روی گوشی Android

1. **تنظیمات یک‌بارمصرف اندروید:** Edit → Project Settings → **Android SDK** →
   مسیرهای SDK / NDK / JDK (یا Android Studio SDK) را مشخص کنید و Accept را بزنید.
2. گوشی را با **USB Debugging** وصل کنید.
3. منوی بالای ادیتور → **Platforms → Android → Launch On → <دستگاه شما>**
   (Build + نصب + اجرا را خودکار انجام می‌دهد).
4. یا **Platforms → Android → Package Project** ← خروجی APK در
   `<پروژه>/Binaries/Android/` ساخته می‌شود.

---

## ۶) برای مرحله بعد چه چیزی آماده شده است؟

- **کامپایل‌شدن پروژه C++ با یک ماژول تمیز** (`CardGame`) و Target های Game/Editor.
- **سه کلاس نقطه‌_extension استاندارد:** `ACardGameGameMode`، `UCardGameGameInstance`،
  `ACardGamePlayerController` — هر سیستم آینده از همین‌جا extend می‌شود.
- **دسته لاگ مشترک `LogCardGame`** برای دیباگ تمام مراحل بعد.
- **پوشه‌های Content از پیش تفکیک‌شده** (Cards، Data، UI، VFX…) مطابق معماری Modular —
  هر سیستم در پوشه خودش و جدا از UI قرار می‌گیرد.
- **Data-Driven بودن:** کارت‌ها در فازهای بعد به‌صورت `UPrimaryDataAsset` در `Content/Data`
  تعریف می‌شوند؛ Widgetها هرگز اطلاعات کارت را Hard-Code نمی‌کنند.
- **تنظیمات Android + Scalable + Portrait** آماده‌ی Package.
- **دو خانه خالی برای نقشه‌ها:** `Content/Maps/MainMenu` و `Content/Maps/Battle`.

### چه چیزی عمداً ساخته نشد (طبق قانون مرحله ۱)

سیستم کارت، مکانیک Battle، AI، Deck، Ability، Shop، هرگونه UI/Widget،
و هر سیستم Online/Multiplayer/Backend.
