# مرحله ۳ — سیستم Collection کارت‌های بازیکن

پایه‌ی **مالکیت کارت‌ها** برای بازی کارتی فانتزی (UE 5.8 / Android / ماژول `CardGame`).

> **Scope:** فقط Player-Owned Cards · Collection · Level/XP پایه · شناسایی یکتا ·
> ذخیره/بارگذاری. هیچ Deck، UI، Battle، AI، Shop یا Packی ساخته نشده است.

---

## ۱) معماری

```
UCardDataAsset                    (تعریف ثابت - Stage 2 - مشترک بین همه کپی‌ها)
      │
      │ CardID  (ارجاع - بدون کپی‌برداری از Name/Stats/Ability)
      ▼
FPlayerCardInstance               (یک کپی متعلق به بازیکن)
      ├── UniqueInstanceID  (FGuid - یکتای هر کپی)
      ├── CardID
      ├── Level   (پیش‌فرض 1)
      ├── XP      (پیش‌فرض 0)
      └── Quantity (همیشه 1 در معماری فعلی - رزرو برای آینده)
      │
      ▼
FPlayerCardCollection             (کانتینر داده خالص - TArray + عملیات امن)
      │
      ▼
UCardCollectionSubsystem          (API عمومی + رفع ارجاع تعریف + ذخیره/بارگذاری)
      │                            UGameInstanceSubsystem روی UCardGameGameInstance
      ▼
UCardGameSaveGame                 (ذخیره محلی - slot = CardGameSave)
```

**تفکیک معماری (نکته کلیدی Stage 3):**

| لایه | کلاس | چه چیزی را نگه می‌دارد |
|---|---|---|
| تعریف ثابت (static) | `UCardDataAsset` | Name، Description، Stats، Rarity، Category، Ability، تصویر/صدا، تگ‌ها |
| مالکیت بازیکن | `FPlayerCardInstance` | فقط `CardID` + `UniqueInstanceID` + `Level` + `XP` + `Quantity` |

هیچ اطلاعات static داخل instance کپی **نمی‌شود** — رفع ارجاع:
`FPlayerCardInstance → CardID → UCardDataAsset` (توسط Subsystem).

**چرا `UGameInstanceSubsystem`؟** Collection وضعیت پایدار بازیکن است، نه جلسه نبرد —
باید بین رفتن از MainMenu به Battle زنده بماند. subsystem روی
`UCardGameGameInstance` Stage 1 سوار می‌شود **بدون هیچ تغییری** در آن کلاس.

---

## ۲) تصمیم معماری: تکثیر کارت‌ها (Option A)

دو گزینه Spec بررسی شد:

- **Option A — هر کپی یک Instance جدا** (انتخاب شد ✓)
- **Option B — یک ردیف انباشته با Quantity بالا** (رد شد)

**چرا Option A؟** Spec تأکید دارد که معماری نباید جلوی «سطوح متفاوت کپی‌های مختلف»
را بگیرد (Lion_A سطح ۳، Lion_B سطح ۱). در Option B همه کپی‌ها یک Level/XP مشترک
داشتند و برای ارتقای فردی باید بعداً stack «شکسته» می‌شد (بازنویسی معماری).

**نحوه نمایش تکثیر:**
```
Lion × 3  =  سه ورودی FPlayerCardInstance با سه FGuid متفاوت
             هرکدام Level/XP مستقل (الان همه Level=1)
GetCardQuantity(Lion) = جمع Quantity همه ورودی‌های Lion  (= 3)
```

**نقش فیلد `Quantity` (اجباری از Spec):** در معماری فعلی همیشه `1` است و
`GetQuantity()` آن را جمع می‌زند. رزرو شده برای عملیات انبوه/stack در آینده —
عملیات فعلی حتی اگر Quantity>1 هم داشته باشید درست کار می‌کنند.

**ارتقای کارت در آینده:** `SetCardLevel(InstanceID, N)` / `AddCardXP(InstanceID, X)`
روی همان instance — هویت (Guid/CardID) دست نمی‌خورد. هزینه ارتقا، فرمول XP،
سطح‌بندی خودکار = مراحل بعد (الان هیچ‌کدام نیست).

**نحوه حذف با `RemoveCard(CardID, Count)`:** از **آخرین کپی اضافه‌شده** شروع می‌کند
(ترتیب آرایه = ترتیب اضافه). برای حذف انتخابی دقیق‌تر، آینده از
`RemoveCardInstance(InstanceID)` استفاده می‌کند.

---

## ۳) فایل‌های جدید (مسیر دقیق)

```
Source/CardGame/
├── Collection/
│   ├── PlayerCardInstance.h/.cpp        FPlayerCardInstance + FPlayerCardCollection
│   ├── CardCollectionSubsystem.h/.cpp   UCardCollectionSubsystem (+ کنسول-کماندها)
│   └── CardCollectionTests.cpp          ۴ تست اتوماسیون (WITH_DEV_AUTOMATION_TESTS)
└── Save/
    └── CardGameSaveGame.h/.cpp          UCardGameSaveGame
```

**فایل اصلاح‌شده:** `CardGame.Build.cs` ← +`AssetRegistry` (private، برای fallback
پیدا‌کردن کارت‌ها در پوشه‌های فرعی). **هیچ فایل Stage 1/2 دیگری تغییر نکرده.**

---

## ۴) توضیح اجزا

### `FPlayerCardInstance`

فقط دیتا: `UniqueInstanceID (FGuid)`، `CardID (FName)`، `Level (int32=1)`،
`XP (int32=0)`، `Quantity (int32=1)`. کارخانه `MakeNew(CardID)` یک Guid تازه می‌سازد.
`IsValid()` = Guid معتبر + CardID خالی نباشد.

### `FPlayerCardCollection` (کانتینر)

`UPROPERTY TArray<FPlayerCardInstance> OwnedCards` + عملیات امن (هیچ‌کدام crash نمی‌کنند):

| متد | رفتار |
|---|---|
| `AddCard(Instance)` | رد کردن instance نامعتبر یا Guid تکراری |
| `RemoveInstance(Guid)` | حذف یک کپی مشخص؛ false اگر نبود |
| `RemoveCards(CardID, Count)` | حداکثر Count کپی (از آخر)؛ برمی‌گرداند چندتا حذف شد |
| `FindInstance / GetCardsOf / HasCard / HasInstance / GetQuantity / Num / Clear` | کوئری‌ها |
| `SetLevel / AddXP` | ردِ سطح <1 و XP منفی؛ اشباع در `MAX_int32` |
| `Sanitize()` | ترمیم داده خراب بعد از Load (بخش ۵) |

### `UCardCollectionSubsystem`

API عمومی (Blueprint) — همه با `LogCardGame` و fail-safe:

```
AddCard · RemoveCard · RemoveCardInstance
HasCard · HasCardInstance · GetCardQuantity · GetCardInstance
GetCardInstances · GetAllOwnedCards · GetCardsByCategory · GetCardsByRarity
ClearCollection · SetCardLevel · AddCardXP
GetCardDefinition (BlueprintPure) · SaveCollection · LoadCollection
GrantDevTestCollection (فقط توسعه)
```

- **Initialize** → خودکار `LoadCollection()` (بدون save ⇒ خالی + یک لاگ، بدون خطا)
- **Deinitialize** → بدون Save خودکار (ذخیره همیشه صریح است)
- بدون Tick، بدون Actor، بدون UObject اضافه

### رفع ارجاع تعریف (`ResolveCardDefinition`)

1. کش weak (`TMap<FName, TWeakObjectPtr<UCardDataAsset>>`) ← بدون Reload
2. مسیر قراردادی `/Game/Data/Cards/<CardID>.<CardID>` (کارت‌های Stage 2)
3. جستجوی Asset Registry در `/Game/Data` با فیلتر کلاس `UCardDataAsset`
   (اگر کارت‌ها در پوشه فرعی باشند)

تکسچرها هرگز برای تشخیص مالکیت لود **نمی‌شوند** — فقط خود DataAsset سبک.

### `UCardGameSaveGame`

| فیلد | مقدار |
|---|---|
| `SaveVersion` | نسخه قالب فایل (فعلی **1**) |
| `Collection` | کل `FPlayerCardCollection` |

- **Slot:** `CardGameSave` (UserIndex 0) — ثابت روی خود کلاس
  (`DefaultSaveSlotName` / `DefaultSaveUserIndex` / `CurrentSaveVersion`) تا هیچ
  کلاس دیگری slot را Hard-Code نکند.
- **Save:** `SaveCollection()` ← CreateSaveGameObject → نسخه + Collection → `SaveGameToSlot`.
- **Load:** `LoadCollection()` رفتار امن:
  - save وجود ندارد ← کالکشن خالی + لاگ (اولین اجرا عادی)
  - save خراب/نامعتبر/کلاس اشتباه ← خالی + Error لاگ (**بدون کرش**)
  - نسخه قدیمی‌تر ← `MigrateSaveData` (جای مهاجرت‌های آینده آماده است)
  - نسخه جدیدتر ← هشدار + بارگذاری best-effort
  - هر ورودی خراب ← **`Sanitize()`**: CardID خالی/Quantity<1 حذف · Guid نامعتبر/تکراری
    بازتولید · Level<1 ← 1 · XP منفی ← 0
- **آینده (Decks/XP سکه‌ها/...):** `CurrentSaveVersion` را بالا ببرید + یک case
  به `MigrateSaveData` اضافه کنید. تگ‌دار شدن UPROPERTYها باعث می‌شود فیلدهای
  جدید بدون شکستن فایل قدیمی اضافه شوند.

---

## ۵) داده توسعه (Test Collection)

`GrantDevTestCollection()` — **فقط داده توسعه** (نام و دسته `CardGame|Development`
و متن توضیح همه‌جا این را مشخص کرده؛ قابل حذف/جایگزینی در آینده):

Lion ×3 · Eagle ×1 · **T-Rex ×4** · **Dragon ×2** · Robot ×1 · Persian Warrior ×1 ·
Egyptian Pharaoh ×1 · Greek Hero ×1 · Space Monster ×1 · Magic Golem ×1

- **خودکار اجرا نمی‌شود** (کالکشن دائمی پروژه نیست).
- فراخوانی: کنسول ادیتور `CardGame.Collection.GrantDevTestCards` یا از Blueprint.

**کماندهای کنسول (ابزار تست دستی):**

| دستور | عمل |
|---|---|
| `CardGame.Collection.GrantDevTestCards` | داده تست را اضافه می‌کند |
| `CardGame.Collection.Save` | ذخیره در slot |
| `CardGame.Collection.Load` | بارگذاری از slot |
| `CardGame.Collection.Dump` | چاپ همه instance ها در لاگ |

---

## ۶) تست‌ها

### خودکار (۴ تست — `CardGame.Collection.*`)

اجرای مسیر **Tools → Test Automation** (یا Window → Test Automation) ← فیلتر
`CardGame.Collection` ← Run. خط فرمان:

```bat
UnrealEditor-Cmd.exe "<پروژه>.uproject" -ExecCmds="Automation RunTests CardGame.Collection; Quit" -unattended -nullrhi -log
```

| تست | پوشش |
|---|---|
| `AddRemoveQuantity` | Test 1-4: افزودن، تعداد، مالکیت، حذف، ردِ ورودی نامعتبر، یکتایی Guid |
| `LevelAndXP` | Test 5-6: XP مثبت/منفی/overflow، Level ≥1، حفظ هویت، استقلال کپی‌ها |
| `SaveLoad` | Test 7: Save ← Load ← تطابق تعداد/سطح/XP/هویت + پاک‌سازی slot |
| `InvalidDataSanitize` | Test 8/10: داده خراب (CardID خالی، Guid صفر، Level منفی، Quantity 0، Guid تکراری) ترمیم/حذف می‌شود، idempotent، بدون کرش |

پوشش خودکار = لایه داده. تست سطح Subsystem (رفع ارجاع تعریف + ذخیره از طریق
subsystem) نیاز به GameInstance دارد و در automation قابل اتکا نیست → **روش دستی:**

### روش تست دستی (۸ سناریوی Spec)

1. PIE را باز کنید → کنسول: `CardGame.Collection.GrantDevTestCards`
2. `CardGame.Collection.Dump` ← Lion باید **۳ بار** ظاهر شود (هرکدام Guid متفاوت، Level 1، XP 0)
3. `CardGame.Collection.Dump` ← مالکیت برقرار است (HasCard(Lion)=true در Blueprint)
4. `CardGame.Collection.Save` ← توقف PIE ← شروع دوباره PIE ← `CardGame.Collection.Dump`
   ← کالکشن **بازسازی شده** (تعداد/کارت‌ها یکسان) ✓ (بارگذاری خودکار در Initialize)
5. حذف: از Blueprint `RemoveCard(Lion, 1)` ← Dump ← ۲ تا Lion باقی می‌ماند
6. XP: `AddCardXP(guid, 50)` ← Dump ← XP بالا می‌رود؛ `AddCardXP(guid, -10)` ← false، تغییری نمی‌کند
7. Level: `SetCardLevel(guid, 0)` ← false (رد می‌شود)؛ `SetCardLevel(guid, 4)` ← Dump ← Level 4
8. `AddCard(CARD_DOES_NOT_EXIST)` ← Warning در `LogCardGame` و **بدون کرش**
9. **اتوماسیون:** اجرای ۴ تست بالا ← همه سبز

---

## ۷) سازگاری مراحل قبل

- Stage 1: فایل‌ها/Config‌ها **بدون تغییر** (git diff خالی).
- Stage 2: `UCardDataAsset` دست‌نخورده؛ فقط **خوانده** می‌شود (ارجاع نرم/بارگذاری
  درخواستی) — کش weak فقط اشاره‌گر نگه می‌دارد.
- هیچ UI/Deck/Battle/Pack/Shop/Ability ایجاد نشده.

## ۸) سازگاری با آینده

```
Collection (امروز) → Deck Builder → Card Upgrade → Packs → Battle → AI → Progression
```

- Deck Builder: روی `GetAllOwnedCards/GetCardInstances` + `RemoveCardInstance` بنا می‌شود.
- Pack System: `AddCard` N بار (تکثیر ذاتی ✓) — بدون تغییر معماری.
- ارتقا/XP: `SetCardLevel/AddCardXP` آماده‌اند؛ هزینه/فرمول مرحله بعد.
- ذخیره: نسخه‌دار و قابل مهاجرت (Decks/Coins/... بعداً اضافه شوند).
- موبایل: بدون Tick/Actor؛ DataAsset های کوچک؛ بدون لود تکسچر؛ عملیات O(n)
  روی صدها/هزارها instance فقط هنگام اقدام کاربر (نه هر فریم).

## ۹) Build

```bat
"<UE_5.8>\Engine\Build\BatchFiles\Build.bat" FantasyCardBattleEditor Win64 Development -Project="<مسیر>\FantasyCardBattle.uproject" -WaitMutex
```

در این محیط Unreal Engine نصب نیست ← جزئیات وضعیت Build در گزارش پایانی.
