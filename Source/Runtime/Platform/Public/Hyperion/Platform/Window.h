#pragma once
#include <cstdint>
#include <memory>
#include <span>
#include <string>

namespace Hyperion
{
struct FSize
{
	std::uint32_t Width{};
	std::uint32_t Height{};
};

struct FNativeSurface
{
	void* Handle{};
};
enum class EKey
{
	None,
	Tab,
	Left,
	Right,
	Up,
	Down,
	PageUp,
	PageDown,
	Home,
	End,
	Insert,
	Delete,
	Backspace,
	Space,
	Enter,
	Escape,
	A,
	C,
	V,
	X,
	Y,
	Z,
	W,
	S,
	D,
	Q,
	E,
	F
};
enum class EEventType
{
	Quit,
	Resize,
	MouseMove,
	MouseButton,
	MouseWheel,
	Key,
	Text,
	Focus
};

namespace InputButtons
{
inline constexpr std::uint32_t Left = 0;
inline constexpr std::uint32_t Right = 1;
inline constexpr std::uint32_t Middle = 2;
inline constexpr std::uint32_t Extra1 = 3;
inline constexpr std::uint32_t Extra2 = 4;
inline constexpr std::uint32_t Count = 5;
} // namespace InputButtons

namespace InputModifiers
{
inline constexpr std::uint32_t None = 0;
inline constexpr std::uint32_t Control = 1;
inline constexpr std::uint32_t Shift = 2;
inline constexpr std::uint32_t Alt = 4;
inline constexpr std::uint32_t Super = 8;
} // namespace InputModifiers

inline bool HasInputModifier(std::uint32_t InMask, std::uint32_t InModifier)
{
	return (InMask & InModifier) != 0;
}

struct FInputEvent
{
	EEventType Type{};
	float X{};
	float Y{};
	std::uint32_t Button{};
	std::uint32_t Modifiers{};
	EKey Key{};
	bool bDown{};
	std::string Text;
	bool bRepeat{};
};

enum class EMouseCursor
{
	Hidden,
	Arrow,
	TextInput,
	ResizeAll,
	ResizeVertical,
	ResizeHorizontal,
	ResizeDiagonalNE,
	ResizeDiagonalNW,
	Hand,
	NotAllowed
};

class FWindow
{
public:
	FWindow(std::string InTitle, FSize InSize, bool bInHidden = false);
	~FWindow();
	FWindow(const FWindow&) = delete;
	FWindow& operator=(const FWindow&) = delete;
	void Poll();
	void SetMouseCursor(EMouseCursor InCursor);
	bool ShouldClose() const;
	bool Minimized() const;
	FSize PixelSize() const;
	FSize LogicalSize() const;
	FNativeSurface Surface() const;
	std::span<const FInputEvent> Events() const;
	void Resize(FSize InSize);
	void Minimize();
	void Restore();
	void Raise();
	// Associates non-modal top-level windows. Destroy owned windows before their owner.
	void SetOwner(FWindow* InOwner);
	void RequestClose();
	void CancelClose();
	std::string Clipboard() const;
	void SetClipboard(const std::string& InText);
	bool SupportsTypedClipboard() const;
	// Owner-thread only. Empty means absent; access failures throw. Payloads are bounded UTF-8 byte strings.
	std::string TypedClipboard(const std::string& InFormat) const;
	void SetTypedClipboard(const std::string& InFormat, const std::string& InData, const std::string& InText);
	// Requests a black native title bar with light text; returns false when unsupported.
	bool SetDarkTitleBar(bool bInEnabled);

private:
	struct FImpl;
	std::unique_ptr<FImpl> Impl;
};
} // namespace Hyperion
