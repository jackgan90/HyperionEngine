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
	Z
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

struct FInputEvent
{
	EEventType Type{};
	float X{};
	float Y{};
	std::uint32_t Button{};
	std::uint32_t Modifiers{};
	EKey Key{};
	bool Down{};
	std::string Text;
};

class FWindow
{
public:
	FWindow(std::string InTitle, FSize InSize, bool InHidden = false);
	~FWindow();
	FWindow(const FWindow&) = delete;
	FWindow& operator=(const FWindow&) = delete;
	void Poll();
	bool ShouldClose() const;
	bool Minimized() const;
	FSize PixelSize() const;
	FSize LogicalSize() const;
	FNativeSurface Surface() const;
	std::span<const FInputEvent> Events() const;
	void Resize(FSize InSize);
	void Minimize();
	void Restore();
	void RequestClose();
	std::string Clipboard() const;
	void SetClipboard(const std::string& InText);

private:
	struct FImpl;
	std::unique_ptr<FImpl> Impl;
};
} // namespace Hyperion
