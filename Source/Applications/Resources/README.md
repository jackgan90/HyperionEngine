# Hyperion application icon

`Hyperion.png` is the original transparent artwork. `Hyperion.ico` contains
16, 20, 24, 32, 40, 48, 64, 96, 128 and 256 pixel versions for Windows DPI scales.
Both Viewer and Editor compile `Hyperion.rc` into their executable. SDL's Windows
window class selects the first embedded icon; Explorer and the taskbar also use
that resource. No runtime image file or content mount is required.

The artwork was created with the built-in imagegen tool using this prompt:

> Use case: logo-brand. Create a polished original application icon for HyperionEngine, a next-generation real-time rendering engine. Single centered bold geometric H monogram sculpted from luminous faceted metal/glass, precise angular architecture and a restrained ray-traced sheen, sophisticated futuristic technology aesthetic. Strong cyan/ice-blue highlights with a subtle violet secondary reflection on dark graphite facets. Make the H immediately readable at 16 and 32 pixels: thick simple strokes, generous negative spaces, minimal facets, crisp silhouette. Straight-on view, balanced square composition, symbol occupying 85 percent of canvas. Genuine transparent background outside the isolated emblem. No words, no caption, no frame, no mockup, no extra objects, no fine circuitry, no star particles, no diffuse glow outside the silhouette. Production app icon, 1024x1024.

To repackage the source artwork after an intentional replacement, use Python with Pillow:

```python
from PIL import Image

with Image.open("Source/Applications/Resources/Hyperion.png") as image:
    image.save("Source/Applications/Resources/Hyperion.ico",
               sizes=[(size, size) for size in (16, 20, 24, 32, 40, 48, 64, 96, 128, 256)])
```

Pillow is only needed to regenerate the ICO, not to build or run the applications.
