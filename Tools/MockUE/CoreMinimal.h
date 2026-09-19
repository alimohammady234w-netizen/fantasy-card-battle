// Copyright (c) Fantasy Card Battle. All rights reserved.
//
// Tools/MockUE/CoreMinimal.h - a deliberately tiny subset of Unreal's base types.
//
// WHY THIS EXISTS
//   The rules engine, the card database and the AI are written so that they only need int32/FString/
//   FName/TArray/TMap/FRandomStream. That shim lets the exact same shipped .cpp files be compiled with
//   plain g++ so we can (a) run unit tests, (b) run thousands of headless AI-vs-AI matches for balance,
//   without a 40 GB engine install. Nothing here is compiled into the game: the real build uses
//   Runtime/Core/Public/CoreMinimal.h from the engine.
//
// If a header under Source/FantasyCardBattle/Public starts failing to compile here, that is a *signal*:
// the file gained a dependency it should not have.

#pragma once

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#define FANTASYCARDBATTLE_API
#define USTRUCT(...)
#define UENUM(...)
#define UMETA(...)
#define UPROPERTY(...)
#define UFUNCTION(...)
#define UClass
#define GENERATED_BODY()
#define GENERATED_USTRUCT_BODY()
#define GENERATED_UCLASS_BODY()
#define TEXT(x) x

typedef char TCHAR;
typedef std::int32_t int32;
typedef std::uint32_t uint32;
typedef std::int16_t int16;
typedef std::uint16_t uint16;
typedef std::int8_t int8;
typedef std::uint8_t uint8;
typedef std::int64_t int64;
typedef std::uint64_t uint64;

static constexpr int32 INDEX_NONE = -1;
static constexpr float KINDA_SMALL_NUMBER = 1e-4f;
static constexpr double SMALL_NUMBER = 1e-8;

enum class ESearchCase : uint32
{
	IgnoreCase = 0,
	CaseSensitive = 1
};

template <typename T>
inline typename std::remove_reference<T>::type&& MoveTemp(T&& Value) { return static_cast<typename std::remove_reference<T>::type&&>(Value); }

#define check(Condition) do { if (!(Condition)) { std::abort(); } } while (false)
#define checkf(Condition, ...) do { if (!(Condition)) { std::abort(); } } while (false)
#define ensure(Condition) (Condition)
#define ensureMsgf(Condition, ...) (Condition)
#define verify(Condition) (void)(Condition)

template <typename T> struct TNumericLimits;
template <> struct TNumericLimits<int32>
{
	static int32 Min() { return std::numeric_limits<int32>::min(); }
	static int32 Max() { return std::numeric_limits<int32>::max(); }
};
template <> struct TNumericLimits<int64>
{
	static int64 Min() { return std::numeric_limits<int64>::min(); }
	static int64 Max() { return std::numeric_limits<int64>::max(); }
};
template <> struct TNumericLimits<uint32>
{
	static uint32 Min() { return 0u; }
	static uint32 Max() { return std::numeric_limits<uint32>::max(); }
};
template <> struct TNumericLimits<float>
{
	static float Min() { return std::numeric_limits<float>::lowest(); }
	static float Max() { return std::numeric_limits<float>::max(); }
};
template <> struct TNumericLimits<uint8>
{
	static uint8 Min() { return 0u; }
	static uint8 Max() { return 255u; }
};

struct FMath
{
	template <typename T> static T Min(T A, T B) { return A < B ? A : B; }
	template <typename T> static T Max(T A, T B) { return A > B ? A : B; }
	template <typename T> static T Clamp(T Value, T Low, T High) { return Value < Low ? Low : (Value > High ? High : Value); }
	static float Abs(float Value) { return std::fabs(Value); }
	static int32 Abs(int32 Value) { return Value < 0 ? -Value : Value; }
	static float Sqrt(float Value) { return std::sqrt(Value < 0.f ? 0.f : Value); }
	static float FloorToFloat(float Value) { return std::floor(Value); }
	static int32 FloorToInt32(float Value) { return static_cast<int32>(std::floor(Value)); }
	static int32 RoundToInt32(float Value) { return static_cast<int32>(std::lround(Value)); }
};

// ---------------------------------------------------------------------------
// FString / FName
// ---------------------------------------------------------------------------

struct FString
{
	std::string S;

	FString() = default;
	FString(const TCHAR* Text) : S(Text ? Text : "") {}
	FString(const std::string& Text) : S(Text) {}
	FString(int32 Count, const TCHAR InChar) : S(static_cast<size_t>(Count), InChar) {}

	int32 Len() const { return static_cast<int32>(S.size()); }
	bool IsEmpty() const { return S.empty(); }
	const TCHAR* operator*() const { return S.c_str(); }
	TCHAR operator[](int32 Index) const { return S[static_cast<size_t>(Index)]; }
	TCHAR& operator[](int32 Index) { return S[static_cast<size_t>(Index)]; }

	FString Mid(int32 Start) const { return FString(S.substr(static_cast<size_t>(FMath::Max(0, Start)))); }
	FString Mid(int32 Start, int32 Count) const { return FString(S.substr(static_cast<size_t>(FMath::Max(0, Start)), static_cast<size_t>(FMath::Max(0, Count)))); }
	FString Left(int32 Count) const { return FString(S.substr(0, static_cast<size_t>(FMath::Clamp(Count, 0, Len())))); }
	FString Right(int32 Count) const { const int32 N = FMath::Clamp(Count, 0, Len()); return FString(S.substr(static_cast<size_t>(Len() - N), static_cast<size_t>(N))); }

	void Reset() { S.clear(); }
	void Clear() { S.clear(); }
	void Reserve(int32) {}
	void AppendChar(TCHAR Char) { S.push_back(Char); }
	void Append(const FString& Other) { S += Other.S; }

	FString& operator+=(const FString& Other) { S += Other.S; return *this; }
	FString& operator+=(const TCHAR* Other) { if (Other) { S += Other; } return *this; }
	FString operator+(const FString& Other) const { return FString(S + Other.S); }
	FString operator+(const TCHAR* Other) const { return FString(S + (Other ? Other : "")); }

	bool operator==(const FString& Other) const { return S == Other.S; }
	bool operator!=(const FString& Other) const { return S != Other.S; }
	bool operator==(const TCHAR* Other) const { return S == std::string(Other ? Other : ""); }
	bool operator!=(const TCHAR* Other) const { return !(*this == Other); }

	bool Equals(const FString& Other, ESearchCase Case = ESearchCase::CaseSensitive) const
	{
		if (S.size() != Other.S.size()) { return false; }
		if (Case == ESearchCase::CaseSensitive) { return S == Other.S; }
		for (size_t Index = 0; Index < S.size(); ++Index)
		{
			if (std::tolower(static_cast<unsigned char>(S[Index])) != std::tolower(static_cast<unsigned char>(Other.S[Index])))
			{
				return false;
			}
		}
		return true;
	}

	/** UE: FString::FromInt, used by the number formatting helpers. */
	static FString FromInt(int32 InValue) { return FString(std::to_string(InValue)); }

	FString ToString() const { return *this; }

	/** UE: lexicographic comparison returning <0 / 0 / >0. */
	int32 Compare(const FString& Other, ESearchCase Case = ESearchCase::CaseSensitive) const
	{
		if (Case == ESearchCase::IgnoreCase)
		{
			std::string A = S, B = Other.S;
			std::transform(A.begin(), A.end(), A.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
			std::transform(B.begin(), B.end(), B.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
			return A < B ? -1 : (B < A ? 1 : 0);
		}
		return S < Other.S ? -1 : (Other.S < S ? 1 : 0);
	}

	static FString Printf(const TCHAR* Format, ...)
	{
		va_list Args;
		va_start(Args, Format);
		char Buffer[4096];
		const int32 Written = std::vsnprintf(Buffer, sizeof(Buffer), Format, Args);
		va_end(Args);
		if (Written < 0) { return FString(); }
		if (static_cast<size_t>(Written) < sizeof(Buffer)) { return FString(std::string(Buffer, static_cast<size_t>(Written))); }

		std::vector<char> Big(static_cast<size_t>(Written) + 1);
		va_start(Args, Format);
		std::vsnprintf(Big.data(), Big.size(), Format, Args);
		va_end(Args);
		return FString(std::string(Big.data(), static_cast<size_t>(Written)));
	}

};

struct FName
{
	std::string S;

	FName() {}
	FName(const TCHAR* Text) : S(Text ? Text : "") {}
	FName(const FString& Text) : S(Text.S) {}

	bool IsNone() const { return S.empty(); }
	FString ToString() const { return FString(S); }
	const TCHAR* operator*() const { return S.c_str(); }
	bool operator==(const FName& Other) const { return S == Other.S; }
	bool operator!=(const FName& Other) const { return S != Other.S; }
};

/** Matches UE: NAME_None is an empty FName. */
static const FName NAME_None;

template <typename T>
struct TArray
{
	std::vector<T> Items;

	TArray() = default;
	TArray(std::initializer_list<T> Init) : Items(Init) {}
	TArray& operator=(std::initializer_list<T> Init) { Items.assign(Init.begin(), Init.end()); return *this; }
	TArray(const TArray&) = default;
	TArray& operator=(const TArray&) = default;
	TArray(TArray&&) = default;
	TArray& operator=(TArray&&) = default;

	int32 Num() const { return static_cast<int32>(Items.size()); }
	bool IsEmpty() const { return Items.empty(); }
	int32 Max() const { return static_cast<int32>(Items.capacity()); }
	void Reserve(int32 Amount) { Items.reserve(static_cast<size_t>(Amount)); }

	T& operator[](int32 Index) { return Items[static_cast<size_t>(Index)]; }
	const T& operator[](int32 Index) const { return Items[static_cast<size_t>(Index)]; }
	bool IsValidIndex(int32 Index) const { return Index >= 0 && Index < Num(); }

	T& Last() { return Items.back(); }
	const T& Last() const { return Items.back(); }
	T& First() { return Items.front(); }

	void Add(const T& Value) { Items.push_back(Value); }
	void Add(T&& Value) { Items.push_back(MoveTemp(Value)); }
	int32 AddUnique(const T& Value)
	{
		for (int32 Index = 0; Index < Num(); ++Index)
		{
			if (Items[static_cast<size_t>(Index)] == Value) { return Index; }
		}
		Items.push_back(Value);
		return Num() - 1;
	}
	void Append(const TArray& Other) { Items.insert(Items.end(), Other.Items.begin(), Other.Items.end()); }

	bool Contains(const T& Value) const
	{
		for (const T& Item : Items) { if (Item == Value) { return true; } }
		return false;
	}

	int32 IndexOf(const T& Value) const
	{
		for (int32 Index = 0; Index < Num(); ++Index)
		{
			if (Items[static_cast<size_t>(Index)] == Value) { return Index; }
		}
		return INDEX_NONE;
	}

	void RemoveAt(int32 Index) { Items.erase(Items.begin() + Index); }
	bool Remove(const T& Value)
	{
		const int32 Index = IndexOf(Value);
		if (Index == INDEX_NONE) { return false; }
		RemoveAt(Index);
		return true;
	}
	void Reset() { Items.clear(); }

	/** UE: fill the array with N copies of Value. */
	void Init(const T& Value, int32 Count)
	{
		Items.assign(static_cast<size_t>(Count), Value);
	}

	/** UE: in-place sort with a predicate (std::sort under the hood, same complexity class). */
	template <typename Predicate>
	void Sort(Predicate Pred)
	{
		std::sort(Items.begin(), Items.end(), [&](const T& A, const T& B) { return Pred(A, B); });
	}

	void Sort()
	{
		std::sort(Items.begin(), Items.end());
	}
	void Empty() { Items.clear(); }

	void SetNum(int32 Count)
	{
		Items.resize(static_cast<size_t>(FMath::Max(0, Count)));
	}
	void SetNumZeroed(int32 Count)
	{
		Items.clear();
		Items.assign(static_cast<size_t>(FMath::Max(0, Count)), T());
	}
	void SetNumUninitialized(int32 Count) { Items.resize(static_cast<size_t>(FMath::Max(0, Count))); }

	void Insert(const T& Value, int32 Index) { Items.insert(Items.begin() + Index, Value); }
	void Swap(int32 A, int32 B) { std::swap(Items[static_cast<size_t>(A)], Items[static_cast<size_t>(B)]); }

	typename std::vector<T>::iterator begin() { return Items.begin(); }
	typename std::vector<T>::iterator end() { return Items.end(); }
	typename std::vector<T>::const_iterator begin() const { return Items.begin(); }
	typename std::vector<T>::const_iterator end() const { return Items.end(); }

	bool operator==(const TArray& Other) const { return Items == Other.Items; }
};

template <typename K, typename V>
struct TPair
{
	K Key;
	V Value;
	TPair() = default;
	TPair(const K& InKey, const V& InValue) : Key(InKey), Value(InValue) {}
};

template <typename K, typename V>
struct TMap
{
	std::vector<TPair<K, V>> Pairs;

	int32 Num() const { return static_cast<int32>(Pairs.size()); }
	bool IsEmpty() const { return Pairs.empty(); }
	void Reset() { Pairs.clear(); }

	V* Find(const K& Key)
	{
		for (TPair<K, V>& Pair : Pairs) { if (Pair.Key == Key) { return &Pair.Value; } }
		return nullptr;
	}
	const V* Find(const K& Key) const
	{
		for (const TPair<K, V>& Pair : Pairs) { if (Pair.Key == Key) { return &Pair.Value; } }
		return nullptr;
	}
	bool Contains(const K& Key) const { return Find(Key) != nullptr; }
	V& FindOrAdd(const K& Key, const V& DefaultValue)
	{
		if (V* Existing = Find(Key)) { return *Existing; }
		Pairs.push_back(TPair<K, V>(Key, DefaultValue));
		return Pairs.back().Value;
	}

	V& FindOrAdd(const K& Key)
	{
		if (V* Existing = Find(Key)) { return *Existing; }
		Pairs.push_back(TPair<K, V>(Key, V()));
		return Pairs.back().Value;
	}
	void Add(const K& Key, const V& Value)
	{
		if (V* Existing = Find(Key)) { *Existing = Value; return; }
		Pairs.push_back(TPair<K, V>(Key, Value));
	}
	void Remove(const K& Key)
	{
		for (size_t Index = 0; Index < Pairs.size(); ++Index)
		{
			if (Pairs[Index].Key == Key) { Pairs.erase(Pairs.begin() + Index); return; }
		}
	}

	typename std::vector<TPair<K, V>>::iterator begin() { return Pairs.begin(); }
	typename std::vector<TPair<K, V>>::iterator end() { return Pairs.end(); }
	typename std::vector<TPair<K, V>>::const_iterator begin() const { return Pairs.begin(); }
	typename std::vector<TPair<K, V>>::const_iterator end() const { return Pairs.end(); }

	TMap& operator=(const TMap&) = default;
	TMap() = default;
	TMap(const TMap&) = default;
};

struct FRandomStream
{
	uint32 State = 0x1u;

	FRandomStream() = default;
	explicit FRandomStream(int32 Seed) { State = static_cast<uint32>(Seed) * 2654435761u + 1u; if (State == 0u) { State = 1u; } }

	uint32 Next()
	{
		// xorshift32: cheap, deterministic, good enough for shuffling a deck in tests.
		State ^= State << 13; State ^= State >> 17; State ^= State << 5;
		return State;
	}
	int32 RandRange(int32 Low, int32 High)
	{
		if (High <= Low) { return Low; }
		const uint32 Range = static_cast<uint32>(High - Low) + 1u;
		return Low + static_cast<int32>(Next() % Range);
	}
	float FRand() { return static_cast<float>(Next() & 0xffffffu) / 16777216.0f; }
};

/** Same call shape as UE's helper, used by the tools only. */
inline bool FCBSimLoadFileToString(const FString& Path, FString& OutString)
{
	std::FILE* Handle = std::fopen(*Path, "rb");
	if (!Handle) { return false; }
	std::string Text;
	char Buffer[8192];
	size_t Read = 0;
	while ((Read = std::fread(Buffer, 1, sizeof(Buffer), Handle)) > 0)
	{
		Text.append(Buffer, Read);
	}
	std::fclose(Handle);
	OutString = FString(Text);
	return true;
}

inline void FCBSimPrint(const FString& Text) { std::fputs(*Text, stdout); std::fputc('\n', stdout); std::fflush(stdout); }
