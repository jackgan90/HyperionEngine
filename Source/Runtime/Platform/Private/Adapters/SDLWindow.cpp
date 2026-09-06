#include <Hyperion/Platform/Window.h>
#include <SDL3/SDL.h>
#include <stdexcept>
#include <thread>
#include <vector>

namespace Hyperion
{
namespace
{
EKey Translate(SDL_Keycode InKey)
{
	switch (InKey)
	{
		case SDLK_TAB:
			return EKey::Tab;
		case SDLK_LEFT:
			return EKey::Left;
		case SDLK_RIGHT:
			return EKey::Right;
		case SDLK_UP:
			return EKey::Up;
		case SDLK_DOWN:
			return EKey::Down;
		case SDLK_PAGEUP:
			return EKey::PageUp;
		case SDLK_PAGEDOWN:
			return EKey::PageDown;
		case SDLK_HOME:
			return EKey::Home;
		case SDLK_END:
			return EKey::End;
		case SDLK_INSERT:
			return EKey::Insert;
		case SDLK_DELETE:
			return EKey::Delete;
		case SDLK_BACKSPACE:
			return EKey::Backspace;
		case SDLK_SPACE:
			return EKey::Space;
		case SDLK_RETURN:
			return EKey::Enter;
		case SDLK_ESCAPE:
			return EKey::Escape;
		case SDLK_A:
			return EKey::A;
		case SDLK_C:
			return EKey::C;
		case SDLK_V:
			return EKey::V;
		case SDLK_X:
			return EKey::X;
		case SDLK_Y:
			return EKey::Y;
		case SDLK_Z:
			return EKey::Z;
		default:
			return EKey::None;
	}
}

void Check(bool InOk)
{
	if (!InOk)
	{
		throw std::runtime_error(std::string("SDL: ") + SDL_GetError());
	}
}

struct FSDLSession
{
	FSDLSession()
	{
		Check(SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS));
	}

	~FSDLSession()
	{
		SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
	}
};
} // namespace

struct FWindow::FImpl
{
	// Balanced subsystem references keep surviving windows alive.
	FSDLSession Session;
	SDL_Window* Window{};
	FNativeSurface Surface;
	std::thread::id Owner = std::this_thread::get_id();
	std::vector<FInputEvent> Events;
	bool Close = false;

	~FImpl()
	{
		if (Window)
		{
			SDL_DestroyWindow(Window);
		}
	}

	void RequireOwner() const
	{
		if (std::this_thread::get_id() != Owner)
		{
			throw std::logic_error("Window operation requires Main thread");
		}
	}
};

FWindow::FWindow(std::string InTitle, FSize InSize, bool InHidden) : Impl(std::make_unique<FImpl>())
{
	Impl->Window =
	    SDL_CreateWindow(InTitle.c_str(), static_cast<int>(InSize.Width), static_cast<int>(InSize.Height),
	                     SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | (InHidden ? SDL_WINDOW_HIDDEN : 0));
	if (!Impl->Window)
	{
		const std::string Error = SDL_GetError();
		throw std::runtime_error(Error);
	}
#ifdef _WIN32
	Impl->Surface.Handle =
	    SDL_GetPointerProperty(SDL_GetWindowProperties(Impl->Window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
#endif
	SDL_StartTextInput(Impl->Window);
}

FWindow::~FWindow() = default;

void FWindow::Poll()
{
	Impl->RequireOwner();
	Impl->Events.clear();
	SDL_Event Native;
	while (SDL_PollEvent(&Native))
	{
		FInputEvent Event;
		switch (Native.type)
		{
			case SDL_EVENT_QUIT:
			case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
				Event.Type = EEventType::Quit;
				Impl->Close = true;
				break;
			case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
				Event.Type = EEventType::Resize;
				Event.X = static_cast<float>(Native.window.data1);
				Event.Y = static_cast<float>(Native.window.data2);
				break;
			case SDL_EVENT_MOUSE_MOTION:
				Event.Type = EEventType::MouseMove;
				Event.X = Native.motion.x;
				Event.Y = Native.motion.y;
				break;
			case SDL_EVENT_MOUSE_BUTTON_DOWN:
			case SDL_EVENT_MOUSE_BUTTON_UP:
				Event.Type = EEventType::MouseButton;
				Event.Down = Native.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
				Event.Button = Native.button.button == SDL_BUTTON_LEFT     ? 0
				               : Native.button.button == SDL_BUTTON_RIGHT  ? 1
				               : Native.button.button == SDL_BUTTON_MIDDLE ? 2
				               : Native.button.button == SDL_BUTTON_X1     ? 3
				                                                           : 4;
				break;
			case SDL_EVENT_MOUSE_WHEEL:
				Event.Type = EEventType::MouseWheel;
				Event.X = Native.wheel.x;
				Event.Y = Native.wheel.y;
				if (Native.wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
				{
					Event.X = -Event.X;
					Event.Y = -Event.Y;
				}
				break;
			case SDL_EVENT_KEY_DOWN:
			case SDL_EVENT_KEY_UP:
				Event.Type = EEventType::Key;
				Event.Down = Native.type == SDL_EVENT_KEY_DOWN;
				Event.Key = Translate(Native.key.key);
				Event.Modifiers =
				    ((Native.key.mod & SDL_KMOD_CTRL) ? 1u : 0u) | ((Native.key.mod & SDL_KMOD_SHIFT) ? 2u : 0u) |
				    ((Native.key.mod & SDL_KMOD_ALT) ? 4u : 0u) | ((Native.key.mod & SDL_KMOD_GUI) ? 8u : 0u);
				break;
			case SDL_EVENT_TEXT_INPUT:
				Event.Type = EEventType::Text;
				Event.Text = Native.text.text;
				break;
			case SDL_EVENT_WINDOW_FOCUS_GAINED:
			case SDL_EVENT_WINDOW_FOCUS_LOST:
				Event.Type = EEventType::Focus;
				Event.Down = Native.type == SDL_EVENT_WINDOW_FOCUS_GAINED;
				break;
			default:
				continue;
		}
		Impl->Events.push_back(std::move(Event));
	}
}

bool FWindow::ShouldClose() const
{
	Impl->RequireOwner();
	return Impl->Close;
}

bool FWindow::Minimized() const
{
	Impl->RequireOwner();
	return (SDL_GetWindowFlags(Impl->Window) & SDL_WINDOW_MINIMIZED) != 0;
}

FSize FWindow::PixelSize() const
{
	Impl->RequireOwner();
	int W{};
	int H{};
	Check(SDL_GetWindowSizeInPixels(Impl->Window, &W, &H));
	return {static_cast<std::uint32_t>(W), static_cast<std::uint32_t>(H)};
}

FSize FWindow::LogicalSize() const
{
	Impl->RequireOwner();
	int W{};
	int H{};
	Check(SDL_GetWindowSize(Impl->Window, &W, &H));
	return {static_cast<std::uint32_t>(W), static_cast<std::uint32_t>(H)};
}

FNativeSurface FWindow::Surface() const
{
	return Impl->Surface;
}

std::span<const FInputEvent> FWindow::Events() const
{
	Impl->RequireOwner();
	return Impl->Events;
}

void FWindow::Resize(FSize InSize)
{
	Impl->RequireOwner();
	Check(SDL_SetWindowSize(Impl->Window, static_cast<int>(InSize.Width), static_cast<int>(InSize.Height)));
}

void FWindow::Minimize()
{
	Impl->RequireOwner();
	Check(SDL_MinimizeWindow(Impl->Window));
}

void FWindow::Restore()
{
	Impl->RequireOwner();
	Check(SDL_RestoreWindow(Impl->Window));
}

void FWindow::RequestClose()
{
	Impl->RequireOwner();
	Impl->Close = true;
}

std::string FWindow::Clipboard() const
{
	Impl->RequireOwner();
	char* Text = SDL_GetClipboardText();
	std::string Result = Text ? Text : "";
	SDL_free(Text);
	return Result;
}

void FWindow::SetClipboard(const std::string& InText)
{
	Impl->RequireOwner();
	Check(SDL_SetClipboardText(InText.c_str()));
}
} // namespace Hyperion
