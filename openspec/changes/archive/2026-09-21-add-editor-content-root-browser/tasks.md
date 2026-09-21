## 1. Content session and filesystem

- [x] 1.1 Add directory entries and exclusive validated mount replacement, preserving path/link validation.
- [x] 1.2 Add assets-owned root candidate preparation and commit with drained asset/catalog state.
- [x] 1.3 Add reusable renderer/GUI retirement at the content boundary with safe GPU ownership.

## 2. Editor interaction

- [x] 2.1 Add native directory picker, persistent root/recent history and startup precedence.
- [x] 2.2 Implement cancellable directory browsing and native-type scene discovery with internal visibility rules.
- [x] 2.3 Implement split tree/grid, menu actions, refresh and typed asset double-click.
- [x] 2.4 Implement save/discard/cancel root transitions and clear old document/selection/preview state.

## 3. Verification and delivery

- [x] 3.1 Add focused IO/assets, preferences/browser and A/B/A editor transition coverage.
- [x] 3.2 Build and run affected tests, graphical acceptance, plugin absence checks and style/boundary validation.
- [x] 3.3 Update current documentation and record verification, leaving all work uncommitted.

## 4. Display path follow-up

- [x] 4.1 Display root-relative paths in the Open Scene list/input and All in the Content Browser root/breadcrumb, preserving internal package paths; verify existing opening/navigation behavior.

## 5. Independent audit follow-up

- [x] 5.1 Unify title-bar dismissal with Cancel during save-and-switch, preserve the admitted old-root save, and cover dismissal before save completion.
- [x] 5.2 Honor Discard when application close overlaps a root-switch prompt; verify the resulting clean close in Debug and Release.
