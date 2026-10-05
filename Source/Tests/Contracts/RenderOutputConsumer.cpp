#include "Hyperion/Renderer/RenderOutput.h"

int main()
{
	const Hyperion::FImageOutputWindow Window(Hyperion::EImageOutputWindow::Main);
	return Window.Kind() == Hyperion::EImageOutputWindow::Main ? 0 : 1;
}
