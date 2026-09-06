## Why
The framework needs an actual Windows application with responsive event handling and well-defined thread ownership before GPU presentation can be integrated.
## What Changes
- Wrap SDL3 windows, normalized input events, clipboard and window lifecycle.
- Introduce the Viewer entry point, configuration loading and bounded smoke-run options.
- Connect the application loop to the existing dedicated execution domains.
## Capabilities
### New Capabilities
- `application-platform`: desktop window, input and application lifetime.
### Modified Capabilities
None.
## Impact
Adds SDL3 as a private static dependency and a Windows viewer executable with a timed integration run.
