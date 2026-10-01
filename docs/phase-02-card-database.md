# مرحله ۲ — دیتابیس کارت‌های Data-Driven

زیرساخت **Card Definition** برای بازی کارتی فانتزی (UE 5.8 / Android / ماژول `CardGame`).

> **قانون Scope:** در این مرحله هیچ Deck، Collection، Battle، AI، Shop، Pack،
> Card UI یا اجرای Ability ساخته نشده است — فقط لایه تعریف داده (Data Asset).

---

## ۱) چه چیزی ساخته شد؟

| نوع | مورد |
|---|---|
| فایل C++ جدید | ۵ فایل (`CardTypes.h/.cpp`، `CardStats.h`، `CardDataAsset.h/.cpp`) |
| فایل اصلاح‌شده | `CardGame.Build.cs` (افزودن GameplayTags)، `FantasyCardBattle.uproject` (افزودن ۲ پلاگین ابزار ادیتور) |
| اسکریپت ابزار | `Tools/Python/create_sample_cards.py` (سازنده ۱۰ کارت نمونه) |
| پوشه‌های Content | ۱۵ پوشه دسته‌بندی زیر `Content/Cards/` + `Content/Data/Cards/` |
| مستندات | همین فایل (`docs/phase-02-card-database.md`) |

---

## ۲) فایل‌های C++ جدید — مسیر دقیق

```
Source/CardGame/Cards/
├── CardTypes.h          UENUM ها + تگ‌های Native + کتابخانه کمکی
├── CardTypes.cpp        تعریف تگ‌ها + پیاده‌سازی Helper ها
├── CardStats.h          USTRUCT:FCardStats (فقط دیتا)
├── CardDataAsset.h      UCLASS:UCardDataAsset (UPrimaryDataAsset)
└── CardDataAsset.cpp    GetPrimaryAssetId + اعتبارسنجی
```

## ۳) فایل‌های اصلاح‌شده

### `Source/CardGame/CardGame.Build.cs`

یک وابستگی **Public** اضافه شد (بقیه کاملاً دست‌نخورده):

```csharp
"GameplayTags"   // برای FGameplayTagContainer در هدرهای عمومی
```

### `FantasyCardBattle.uproject`

دو پلاگین ابزاریِ فقط-ادیتور (برای اجرای اسکریپت ساخت کارت‌های نمونه) اضافه شد؛
`TargetAllowList: Editor` باعث می‌شود روی بیلد Android قرار نگیرند:

```json
"Plugins": [
  { "Name": "PythonScriptPlugin",        "Enabled": true, "TargetAllowList": ["Editor"] },
  { "Name": "EditorScriptingUtilities",  "Enabled": true, "TargetAllowList": ["Editor"] }
]
```

**هیچ فایل Stage 1 تغییر منفی نکرده است** — GameMode/GameInstance/PlayerController
و تنظیمات Config دست‌نخورده‌اند.

---

## ۴) معماری — چرا این شکلی؟

```
Card Definition  (این مرحله ✓)
      ↓  UCardDataAsset  (asset های /Game/Data/Cards)
Card Database    (مرحله آینده: Asset Manager / scan کردن registry)
      ↓
Collection       (مرحله آینده: آیتم‌های متعلق به بازیکن)
      ↓
Deck Builder     (مرحله آینده)
      ↓
Battle System    (مرحله آینده)
```

نکته بسیار مهم **تفکیک تعریف از مالکیت**:

| لایه | چیست | کجاست |
|---|---|---|
| **CardDefinition** | هویت ثابت کارت: نام، شرح، آمار پایه، ندر، دسته، Ability، آرت | `UCardDataAsset` — هرگز در runtime تغییر نمی‌کند |
| **PlayerOwnedCardData** (آینده) | نمونه متعلق به بازیکن: `CardID + UniqueInstanceID + Level + XP + UpgradeState` | آینده (SaveGame/struct) — با ارجاع به CardID کار می‌کند |

یعنی Level/XP هر بازیکن روی Data Asset **نوشته نمی‌شود**؛ Data Asset فقط
`BaseLevel`/`BaseXP` (مقدار اولیه) نگه می‌دارد. این طراحی مسیر ارتقا/XP را برای
آینده باز گذاشته بدون این‌که معماری عوض شود.

**پراپرتی‌ها در هیچ Widget/GameMode/PlayerControllerای Hard-Code نمی‌شوند؛**
همه سیستم‌ها از طریق این Data Asset (و در آینده PlayerOwnedCardData) می‌خوانند.

---

## ۵) توضیح هر Enum (`CardTypes.h`)

### `ECardRarity` — ندر کارت

`Common → Uncommon → Rare → Epic → Legendary → Mythic`

اساساً برای آینده: تغییر Frame، افکت‌های بصری، احتمالات Pack، فیلتر Collection،
الزامات ارتقا. **هیچ‌کدام از این سیستم‌ها هنوز پیاده نشده — فقط واژگان آماده است.**

### `ECardCategory` — دسته کارت (۱۵ مورد)

`Animals, Dinosaurs, Robots, AncientEgypt, AncientPersia, AncientGreece,
AncientRome, Heroes, Villains, Fantasy, Mythology, SciFi, Space, Monsters, Magic`

**قانون طلایی گسترش:** مقدار هر enum همان ایندکس عددی است که در Asset ذخیره می‌شود.
هرگز جای موارد موجود را عوض یه حذف نکنید — **فقط آخر لیست اضافه کنید.**
(DisplayName های چندکلمه‌ای مثل `"Ancient Egypt"` با `UMETA(DisplayName=...)` تزیین شده‌اند.)

### `ECardAbilityType` — نوع Ability (۱۶ مورد)

`None, Shield, Heal, Freeze, Poison, Curse, CriticalStrike, Mirror, Copy,
DoubleAttack, Counter, Revive, Boost, Silence, Dodge, Rage`

⚠️ **فقط دیتا:** این enum می‌گوید کارت «چه Ability دارد» — منطق اجرایی
(محاسبه، هدف‌گیری، افکت) در مرحله آینده پیاده می‌شود.

### تگ‌های Native Gameplay Tag

ساختار `Card.*` (فقط ۱۱ تگ — بنیان، نه صدها تگ):

```
Card.Element.{Fire, Water, Earth, Air}
Card.Trait.{Ancient, Mechanical, Magical}
Card.Role.{LegendaryCreature, Warrior, Monster, Boss}
```

- انتخاب در ادیتور از پنجره Tag Picker با فیلتر `Categories = "Card"` محدود به همین زیردرخت است.
- برای افزودن تگ: یک `UE_DECLARE_GAMEPLAY_TAG_EXTERN` در `CardTypes.h` +
  یک `UE_DEFINE_GAMEPLAY_TAG` در `CardTypes.cpp` — **هیچ سیستم دیگری نیاز به تغییر ندارد.**
- یا از Project Settings → GameplayTags هم می‌توانید تگ بسازید؛ هر دو روش یکسان کار می‌کنند.

### `UCardTypesLibrary`

سه تابع `BlueprintPure` برای گرفتن DisplayName محلی‌سازی‌شده:
`GetRarityDisplayName` / `GetCategoryDisplayName` / `GetAbilityDisplayName`
(پایه‌ای برای فیلترها و لیست‌های آینده — هیچ Widgetی در این مرحله ساخته نشده).

---

## ۶) `FCardStats` (`CardStats.h`)

```text
Power, Speed, Height, Defense, Intelligence, Stamina, Luck, Age   (int32)
```

- `USTRUCT(BlueprintType)` — هر ۸ مورد `EditAnywhere + BlueprintReadWrite`
- هر مقدار `ClampMin = 0` در ادیتور
- **فقط دیتا:** هیچ منطق مقایسه/نبردی داخل آن نیست (نبرد مرحله بعد)
- توسعه: با `UPROPERTY` جدید **اضافه** کنید؛ ساختار موجود را حذف/جابه‌جا نکنید

چرا `int32`؟ مقادیر نمونه Spec همگی صحیح‌اند و سبک‌ترین حالت برای موبایل و
مقایسه‌های آینده Top-Trumps هستند.

---

## ۷) `UCardDataAsset` (`CardDataAsset.h/.cpp`)

`UCLASS(BlueprintType)` ارث‌بری از **`UPrimaryDataAsset`**.

| گروه | پراپرتی‌ها | نوع |
|---|---|---|
| Identification | `CardID` | `FName` (unique، مثلاً `CARD_ANIMAL_LION_001`) |
| | `Name`، `Description` | `FText` (Localize شده) |
| Classification | `Category`، `Rarity` | enum های بالا |
| Stats | `Stats` | `FCardStats` |
| Progression | `BaseLevel` (پیش‌فرض 1)، `BaseXP` (پیش‌فرض 0) | `int32` |
| Ability | `AbilityType` | `ECardAbilityType` |
| | `AbilityValue` | `float` (مقدار عددی، مثلاً 10) |
| | `AbilityDescription` | `FText` |
| Visual | `CardImage`، `CardFrame` | `TSoftObjectPtr<UTexture2D>` |
| Audio | `CardSound`، `CardVoice` | `TSoftObjectPtr<USoundBase>` |
| Animation | `CardAnimation` | `TSoftObjectPtr<UAnimationAsset>` |
| Tags | `CardTags` | `FGameplayTagContainer` |

همه پراپرتی‌ها **`BlueprintReadOnly`** هستند (خواندن از Blueprint بدون تکرار داده).

### Primary Asset Id

```cpp
virtual FPrimaryAssetId GetPrimaryAssetId() const override;
// نتیجه:  Card:CARD_ANIMAL_LION_001
```

- `AssetRegistrySearchable` روی `CardID` → در Asset Registry قابل جستجوست.
- سازگار با **`UAssetManager` آینده:** کافی است در مرحله بعد، تایپ `Card` را
  برای این کلاس ثبت کنید — هیچ بازنویسی لازم نیست. (خود Asset Manager هنوز
  طبق Spec ساخته نشده.)
- اگر `CardID` خالی باشد، به نام asset برمی‌گردد تا Id هیچ‌وقت نامعتبر نشود.

### اعتبارسنجی (`ValidateCardData`)

```cpp
bool ValidateCardData(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const;
void ValidateCardDataNow();   // دکمه CallInEditor در پنل Details
```

**خطاها (Error):** `CardID` خالی · `Name` خالی · هر آمار ≤ 0 · `Age` منفی ·
`BaseLevel < 1` · `BaseXP < 0`

**هشدارها (Warning):** `Age == 0` · `Description` خالی · Ability بدون مقدار/شرح ·
`CardImage` خالی · `CardID` متفاوت با نام asset (فقط ادیتور)

پیام‌ها دقیقاً با پیشوند `Card Data Validation Error/Warning:` و در دسته
`LogCardGame` ثبت می‌شوند. **هرگز کرش نمی‌کند** — داده ناقص فقط پیام دارد.

---

## ۸) چگونه یک کارت جدید بسازم؟

**روش A — دستی (همیشه کار می‌کند):**
1. در Content Browser وارد `Content/Data/Cards` شوید.
2. راست‌کلیک → **Miscellaneous → Data Asset** → در لیست کلاس **Card Data Asset** را انتخاب کنید.
3. نام asset را **دقیقاً برابر CardID** بگذارید (مثلاً `CARD_ANIMAL_WOLF_001`).
4. فیلدها را پر کنید (CardID, Name, Description, Category, Rarity, Stats, Ability...).
5. دکمه **Validate Card Data Now** در پنل Details را بزنید و خروجی را در
   Output Log (فیلتر `LogCardGame`) ببینید.
6. `Ctrl+S` برای ذخیره.

**روش B — اسکریپت (برای انبوه‌سازی):**
`Tools → Execute Python Script…` → فایل `Tools/Python/create_sample_cards.py`
(یا دستور کنسول `py "<مسیر>/Tools/Python/create_sample_cards.py"`).
اسکریپت idempotent است؛ اجرای مجدد کارت‌های موجود را آپدیت می‌کند.

**کارت‌های نمونه (۱۰ عدد):**

| CardID | نام | دسته | ندر | Ability |
|---|---|---|---|---|
| `CARD_ANIMAL_LION_001` | Lion | Animals | Rare | Boost |
| `CARD_ANIMAL_EAGLE_001` | Eagle | Animals | Uncommon | Dodge |
| `CARD_DINOSAUR_TREX_001` | T-Rex | Dinosaurs | Epic | CriticalStrike |
| `CARD_FANTASY_DRAGON_001` | Dragon | Fantasy | Legendary | DoubleAttack |
| `CARD_ROBOT_001` | Robot | Robots | Rare | Shield |
| `CARD_PERSIA_WARRIOR_001` | Persian Warrior | AncientPersia | Rare | Counter |
| `CARD_EGYPT_PHARAOH_001` | Egyptian Pharaoh | AncientEgypt | Epic | Curse |
| `CARD_GREECE_HERO_001` | Greek Hero | AncientGreece | Epic | Heal |
| `CARD_SPACE_MONSTER_001` | Space Monster | Space | Legendary | Poison |
| `CARD_FANTASY_GOLEM_001` | Magic Golem | Magic | Rare | Mirror |

آمار Lion/T-Rex/Dragon دقیقاً مطابق Spec؛ بقیه مقادیر تستی معقول.
**داده‌ها در C++ Hard-Code نشده‌اند** — کل جدول در اسکریپت پایتون است و بعداً
قابل حذف/جایگزینی است. (اسکریپت یک‌بارمصرف است؛ خروجی آن Asset های واقعی است.)

> اگر Python plugin فعال نبود: Edit → Plugins → *Python Editor Script Plugin* →
> Restart، یا `.uproject` ویرایش شده را ذخیره کنید و ادیتور را دوباره باز کنید.

---

## ۹) چگونه یک دسته (Category) جدید بسازم؟

1. در `CardTypes.h` فقط **آخر** enum `ECardCategory` یک مورد اضافه کنید:
   ```cpp
   AncientIndia UMETA(DisplayName = "Ancient India"),
   ```
2. `Content/Cards/AncientIndia/` بسازید (برای آرت).
3. کارت‌های جدید را بسازید — دکمه انتخاب Category خودکار گزینه جدید را نشان می‌دهد.
4. (اختیاری) در `create_sample_cards.py` ایندکس جدید را به `CATEGORY_INDEX` اضافه کنید.

هیچ سیستم دیگری (Battle/Deck/UI) نیاز به تغییر ندارد.

## ۱۰) چگونه یک ندر (Rarity) جدید بسازم؟

دقیقاً مثل Category: فقط آخر `ECardRarity` اضافه کنید، مثلاً `Ancient` یا `Prismatic`.
مثلاً: `Prismatic UMETA(DisplayName = "Prismatic"),` ← ایندکس ۶.
سیستم‌های بصری/احتمالیِ آینده بر اساس همین مقدار کار می‌کنند.

## ۱۱) چگونه تصویر کارت را Assign کنم؟

1. تصویر را در پوشه دسته مربوطه (`Content/Cards/Animals/...`) وارد کنید
   (PNG توصیه می‌شود؛ فرمت‌های ASTC/ETC2 هنگام Build برای Android).
2. در Data Asset، فیلد **Visual → Card Image** را با drag-and-drop پر کنید.
3. **نکته حافظه:** این یک **Soft Reference** است — تا زمانی که سیستم UI (مرحله آینده)
   به‌صورت explicit آن را `LoadSynchronous()` نکند، تکسچر در حافظه بارگذاری **نمی‌شود.**
   `CardFrame` هم همین‌طور (فریم‌های ندر در آینده).

## ۱۲) چگونه صدا/ویس/انیمیشن را Assign کنم؟

- **Audio → Card Sound / Card Voice:** `TSoftObjectPtr<USoundBase>` — هر SoundCue
  یا SoundWave را assign کنید (پوشه `Content/Sounds`).
- **Animation → Card Animation:** `TSoftObjectPtr<UAnimationAsset>` (مونتیج/انیمیشن).
- همه soft هستند و در Startup بار نمی‌شوند. **Sound Manager هنوز وجود ندارد**
  — فقط دیتا آماده است.

## ۱۳) چگونه Ability را Assign کنم؟

1. **Ability → Ability Type** را انتخاب کنید (مثلاً `Boost`).
2. **Ability Value** عدد بدهید (مثلاً `10`).
3. **Ability Description** متن شرح بنویسید (مثلاً *"Increases the selected stat."*).
4. Validate بزنید — اگر Type ≠ None ولی Value ≤ 0 باشد هشدار می‌گیرید.

⚠️ رفتار Ability در مرحله آینده پیاده می‌شود؛ الان صرفاً دیتا.

---

## ۱۴) اتصال به سیستم‌های آینده

| سیستم آینده | چگونه وصل می‌شود |
|---|---|
| **Card Database** | `UAssetManager` تایپ `Card` را برای `UCardDataAsset` ثبت می‌کند و با `FPrimaryAssetId("Card", CardID)` یا Asset Registry scan بارگذاری می‌کند — از همین امروز سازگار است. |
| **Collection** | هر آیتم بازیکن فقط `CardID` (و UniqueInstanceID/Level/XP خودش) نگه می‌دارد → به این Asset ارجاع می‌دهد. |
| **Deck Builder** | روی Collection + محدودیت‌ها (اندازه Deck، یکتایی) کار می‌کند — همه‌چیز از همین Asset می‌خواند. |
| **Battle** | آمار (`Stats`) و داده Ability را از Asset می‌خواند؛ **هیچ مقایسه‌ای هنوز کد نشده.** |
| **AI** | دسترسی به همان Database برای ارزیابی کارت‌ها. |
| **Packs** | احتمالات بر اساس `Rarity` — واژگانش آماده است. |
| **UI/UMG** | Widget ها فقط `UCardDataAsset` را می‌خوانند (BlueprintReadOnly) — هیچ Hard-Code ممنوع. |

## ۱۵) پشتیبانی از ۱,۰۰۰+ (و ۱۰,۰۰۰+) کارت

- **Data Asset سبک:** هر کارت یک UObject کوچک (متن/enum/عدد + soft ptr) — بدون
  تکسچر در حافظه، بدون Tick، بدون Actor.
- **Asset Registry:** با `AssetRegistrySearchable` روی `CardID` + اسکن تایپ `Card`
  می‌توان بدون بارگذاریِ همه Asset ها، فهرست/فیلتر ساخت (پایه Collection آینده).
- **Soft references:** تصویر/صدا/انیمیشن فقط هنگام نیاز لود می‌شوند
  (۱۰,۰۰۰ کارت × تکسچر = بارگذاری درخواستی، نه هم‌زمان).
- **فایل‌سیستم:** پوشه‌بندی `Data/Cards` و `Cards/<Category>` برای مرتب‌سازی فیزیکی.
- **افزودنی بودن:** Enum ها فقط append می‌شوند؛ تگ‌ها آزادند؛ استراکچرها extend می‌شوند —
  **بازنویسی معماری لازم نیست.**

## ۱۶) ملاحظات حافظه موبایل (Android)

- هیچ `TSoftObjectPtr` در constructor لود نمی‌شود؛ لود فقط با تصمیم صریح سیستم مصرف‌کننده.
- بدون Tick و بدون Actor برای داده کارت.
- `DefaultTouchInterface=None` Stage 1 و UI-محور بودن بازی → بدون هزینه input اضافه.
- تگ‌ها و enum ها فقط چند بایت در هر Asset.
- سیستم‌های بعدی باید از `FStreamableManager`/AssetManager برای لود گروهی استفاده کنند
  (پیشنهاد: لود تصویر کارت فقط هنگام نمایش در دست یا Collection).

---

## ۱۷) Build / تست

```bat
"<UE_5.8>\Engine\Build\BatchFiles\Build.bat" FantasyCardBattleEditor Win64 Development -Project="<مسیر>\FantasyCardBattle.uproject" -WaitMutex
```

1. کامپایل C++ (مراحل Stage 1) — بعد از اولین اجرا، ادیتور درباره ۲ پلاگین جدید
   «Restart» می‌خواهد → ری‌استارت کنید.
2. `Tools → Execute Python Script…` → `Tools/Python/create_sample_cards.py`.
3. به `Content/Data/Cards` بروید — ۱۰ کارت باید ظاهر شوند.
4. یک کارت را انتخاب کنید → دکمه **Validate Card Data Now** → در Output Log
   (فیلتر `LogCardGame`) پیام‌های اعتبارسنجی را ببینید.
5. Stage 1 را تست کنید: باز کردن `MainMenu` و `Alt+P` باید مثل قبل لاگ‌های
   `LogCardGame` را بدهد (سازگاری Stage 1 حفظ شده).

**در این محیط Unreal Engine نصب نیست** → کامپایل واقعی باید روی ماشین شما انجام شود
(جزئیات در گزارش پایانی).
