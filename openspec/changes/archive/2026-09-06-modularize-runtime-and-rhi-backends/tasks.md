## 1. Modules

- [x] 1.1 Move owned files into concept modules with Public/Private ownership and split foundation dependencies.
- [x] 1.2 Introduce module CMake definitions and update VS grouping, include/style boundaries and developer guidance.

## 2. Backend contracts

- [x] 2.1 Add abstract device/swapchain/backend interfaces, capabilities, polymorphic resource payloads and an explicit factory registry.
- [x] 2.2 Extract D3D12 device, resources and swapchain with headless creation and safe shared GPU state.
- [x] 2.3 Migrate Viewer, configuration, Render Graph and plugins to abstract interfaces and backend-selected shader targets.

## 3. Validation

- [x] 3.1 Add independent fake-backend contract tests and D3D12 ownership/lifetime/capability tests.
- [x] 3.2 Pass boundary/style checks and the full Visual Studio Debug/Release CPU and GPU suites.
- [x] 3.3 Record verification, synchronize specs and archive the completed change.
