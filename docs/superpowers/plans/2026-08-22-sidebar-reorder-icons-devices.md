# Astrea Explorer Sidebar Reorder, Symbolic Icons, and Device Rendering Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close the favorite reorder, semantic icon tint, and device-row rendering defects in the native Astrea Explorer runtime without changing the persisted settings format or unrelated Explorer behavior.

**Architecture:** Add a focused native `QAbstractListModel` with a drag transaction for effective sidebar favorites, expose it through `AppStateFacade`, and render it from a nested non-interactive QML `ListView` with a pointer-following proxy. Extend the existing native icon service/provider with explicit full-color and symbolic render modes, then correct the device `QVariantList` bindings to use `modelData` while preserving the existing Rust/native device contract.

**Tech Stack:** Qt 6.11 C++, `QAbstractListModel`, Qt Quick/QML, `QIcon`/`QImage`, Qt Test, Python `unittest`, CMake, Rust backend consumed without modification.

## Global Constraints

- Preserve all unrelated user changes in the Orbit working tree.
- Keep the existing favorite JSON format compatible with both object-array and legacy string-array entries.
- Persist the finalized favorite order once when a drag completes; do not write settings on pointer movement.
- Keep filesystem URL drops separate from internal favorite reorder state.
- Use symbolic/tinted rendering only for semantic sidebar/portal icons; keep file-content icons full-color.
- Do not modify `Core/bridge/apps/explorer/src/devices.rs` unless validation proves an independent backend defect.
- Reuse `build-closure-release` and install the verified runtime to the existing `AstreaNative` prefix.
- Do not reset, clean, revert, overwrite, or stage unrelated files.

---

### Task 1: Add the ordered favorites model with transaction semantics

**Files:**
- Create: `Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/native/src/models/sidebar_favorites_model.h`
- Create: `Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/native/src/models/sidebar_favorites_model.cpp`
- Create: `Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/native/tests/tst_sidebar_favorites_model.cpp`
- Modify: `Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/native/CMakeLists.txt`

**Interfaces:**
- Produces `Astrea::Explorer::Native::Models::SidebarFavoritesModel`.
- Roles: `PathRole`, `LabelRole`, and `IconRole`, mapped to `path`, `label`, and `icon`.
- Public methods:
  - `void setItems(const QVariantList &items)`
  - `QVariantList items() const`
  - `int indexOfPath(const QString &path) const`
  - `bool beginDrag(const QString &path)`
  - `bool moveFavorite(const QString &path, int finalIndex)`
  - `void commitDrag()`
  - `void cancelDrag()`
  - `bool dragActive() const`

- [ ] **Step 1: Write failing model tests.**

  Cover object-array and legacy string-array loading, role values, upward and
  downward moves, same-index no-op behavior, `rowsMoved` notification, and
  cancel rollback. Use a helper `items({"one", "two", "three"})` that creates
  normalized maps with stable labels/icons.

  The downward test must start with `[one, two, three]`, move `one` to final
  index `2`, and assert `[two, three, one]`; this catches the Qt
  `destinationChild` off-by-one error.

- [ ] **Step 2: Run the focused test target and confirm it fails.**

  Run:

  ```bash
  rtk run cmake --build /home/agony/GitHub/Orbit/Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/build-closure-release --target tst_sidebar_favorites_model -j2
  ```

  Expected: configuration/build failure because the model and target do not
  exist yet.

- [ ] **Step 3: Implement the model.**

  Store rows as `QVector<QVariantMap>`. `setItems()` normalizes path/label/icon
  fields and uses `beginResetModel()`/`endResetModel()`. `moveFavorite()` calls
  `beginMoveRows(QModelIndex(), from, from, QModelIndex(), finalIndex + 1)` for
  downward moves and `finalIndex` for upward moves, then uses
  `QVector::move(from, finalIndex)`. `beginDrag()` snapshots the rows and
  records the dragged normalized path. `commitDrag()` clears the snapshot;
  `cancelDrag()` restores the snapshot with one model reset.

- [ ] **Step 4: Register the target and run the tests.**

  Add the model source to a focused static library or the existing native
  navigation target, add `tst_sidebar_favorites_model`, and register it with
  `add_test(NAME sidebar_favorites_model COMMAND tst_sidebar_favorites_model)`.

  Run:

  ```bash
  rtk run cmake --build /home/agony/GitHub/Orbit/Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/build-closure-release --target tst_sidebar_favorites_model -j2
  rtk run ctest --test-dir /home/agony/GitHub/Orbit/Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/build-closure-release -R sidebar_favorites_model --output-on-failure
  ```

- [ ] **Step 5: Commit the isolated model work.**

  ```bash
  git add Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/native/src/models/sidebar_favorites_model.h Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/native/src/models/sidebar_favorites_model.cpp Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/native/tests/tst_sidebar_favorites_model.cpp Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/native/CMakeLists.txt
  git commit -m "feat: add ordered sidebar favorites model"
  ```

### Task 2: Make `AppStateFacade` the single persistence boundary

**Files:**
- Modify: `Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/native/src/controllers/app_state_facade.h`
- Modify: `Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/native/src/controllers/app_state_facade.cpp`
- Modify: `Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/AppState.qml`
- Modify: `Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/compatibility/NativeAppStateAdapter.qml`
- Modify: `Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/compatibility/LegacyAppStateAdapter.qml`
- Test: `Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/native/tests/tst_app_state_facade.cpp`

**Interfaces:**
- Exposes `QAbstractItemModel *sidebarFavoritesModel() const` through
  `Q_PROPERTY(QAbstractItemModel* sidebarFavoritesModel READ sidebarFavoritesModel CONSTANT)`.
- Exposes `beginSidebarFavoriteDrag`, `previewSidebarFavoriteMove`,
  `commitSidebarFavoriteDrag`, and `cancelSidebarFavoriteDrag` as invokables.
- Existing `sidebarFavorites`, JSON properties, pin/remove methods, and
  `moveSidebarFavorite` remain source-compatible.

- [ ] **Step 1: Add failing facade persistence tests.**

  Add tests that load an object-array and legacy string-array, begin a drag,
  preview a move without changing the settings file, commit it and verify the
  exact final order is persisted, then begin another drag and cancel it while
  asserting the settings file and model both retain the prior committed order.
  Add a `QSignalSpy` for `sidebarFavoritesChanged` and verify the transaction
  does not persist on preview.

- [ ] **Step 2: Run the focused facade test and confirm the new assertions fail.**

  ```bash
  rtk run cmake --build /home/agony/GitHub/Orbit/Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/build-closure-release --target tst_app_state_facade -j2
  rtk run ctest --test-dir /home/agony/GitHub/Orbit/Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/build-closure-release -R app_state_facade --output-on-failure
  ```

- [ ] **Step 3: Integrate the model into the facade.**

  Construct the model as a child of `AppStateFacade` after settings are loaded.
  Extract the current effective-favorites parsing/default-merging behavior into
  one helper used to initialize or refresh the model. Make `sidebarFavorites()`
  return `model.items()`. Make JSON setters refresh the model after updating
  settings. Serialize `model.items()` through one private persistence helper
  for finalized drag commits and legacy immediate moves. Keep default hidden
  paths and legacy string entries compatible.

- [ ] **Step 4: Add the QML compatibility surface and rerun tests.**

  Add `sidebarFavoritesModel` to `AppState.qml` and the native adapter. The
  legacy adapter supplies the existing array as a fallback so the new QML
  surface remains loadable without the native facade. Add adapter wrappers for
  all four transaction calls. Run the focused C++ test and the Python QML test
  module.

- [ ] **Step 5: Commit the facade integration.**

  ```bash
  git add Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/native/src/controllers/app_state_facade.h Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/native/src/controllers/app_state_facade.cpp Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/AppState.qml Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/compatibility/NativeAppStateAdapter.qml Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/compatibility/LegacyAppStateAdapter.qml Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/native/tests/tst_app_state_facade.cpp
  git commit -m "feat: expose transactional sidebar favorites"
  ```

### Task 3: Replace translated favorite delegates with a real drag proxy

**Files:**
- Modify: `Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/components/layout/Sidebar.qml`
- Modify: `Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/tests/test_explorer_qml.py`

**Interfaces:**
- Consumes `AppState.sidebarFavoritesModel` and the four transaction methods
  from Task 2.
- Produces a non-interactive nested `ListView`, proxy state, insertion marker,
  and separate filesystem drop behavior.

- [ ] **Step 1: Add failing QML regression assertions.**

  Assert that the favorites block contains `ListView`,
  `interactive: false`, `model: AppState.sidebarFavoritesModel`,
  `DragHandler` with `target: null`, a proxy/insertion marker, and the four
  transaction calls. Assert the old unconditional `if (drop.accepted) return`
  branch and favorite MIME handling are absent from the filesystem `DropArea`.

- [ ] **Step 2: Run the QML regression module and confirm the assertions fail.**

  ```bash
  rtk run python3 -m unittest discover -s Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/tests -p 'test_explorer_qml.py'
  ```

- [ ] **Step 3: Implement the model-backed Favorites view.**

  Replace only the Favorites `Repeater` with a `ListView` whose delegate is
  `SidebarItem`. Set `interactive: false`, `height: contentHeight`, and
  `displaced`/`move` transitions using 120 ms `Easing.OutCubic`. Set the
  delegate's `DragHandler.target` to `null`; use handler centroid coordinates
  mapped into the list to calculate the final index. Begin the transaction on
  activation, preview model moves only when the index changes, show a clipped
  proxy with the original label/icon, and show a one-pixel insertion marker.

- [ ] **Step 4: Implement commit/cancel and separate filesystem drops.**

  Commit on normal handler release, cancel on handler cancellation or any
  invalidated drag state, and always hide the proxy/marker in a cleanup helper.
  Keep `DropArea` for `text/uri-list`/filesystem handling only; disable it
  during internal favorite dragging and route valid drops through
  `root.handleDroppedUrls(drop, sbItem.path)`.

- [ ] **Step 5: Run source tests and inspect the QML diff.**

  ```bash
  rtk run python3 -m unittest discover -s Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/tests -p 'test_explorer_qml.py'
  rtk run git diff --check
  ```

- [ ] **Step 6: Commit the QML reorder interaction.**

  ```bash
  git add Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/components/layout/Sidebar.qml Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/tests/test_explorer_qml.py
  git commit -m "fix: make sidebar favorite dragging reorderable"
  ```

### Task 4: Add explicit full-color and symbolic icon rendering modes

**Files:**
- Modify: `Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/native/src/services/icon_theme_service.h`
- Modify: `Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/native/src/services/icon_theme_service.cpp`
- Modify: `Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/native/src/runtime/astrea_icon_image_provider.cpp`
- Modify: `Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/native/tests/tst_icon_theme_service.cpp`
- Modify: `Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/native/tests/tst_icon_image_provider.cpp`

**Interfaces:**
- Add `enum class IconRenderMode { FullColor, Symbolic }`.
- Extend `renderIcon`, `iconSourceForNames`, and cache-key generation with a
  defaulted render mode so current full-color callers remain source-compatible.
- Add `symbolicIconSourceForNames(const QStringList&, int)` for semantic calls.

- [ ] **Step 1: Add failing color-mode tests.**

  Render the existing test theme's colored icon once in `FullColor` mode and
  once in `Symbolic` mode. Assert the full-color center retains the fixture
  RGB value and symbolic nontransparent pixels have equal RGB channels with
  preserved alpha. Add a provider URL test containing `mode=symbolic` and
  assert the returned image is monochrome.

- [ ] **Step 2: Run the focused icon/provider tests and confirm failure.**

  ```bash
  rtk run cmake --build /home/agony/GitHub/Orbit/Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/build-closure-release --target tst_icon_theme_service tst_icon_image_provider -j2
  rtk run ctest --test-dir /home/agony/GitHub/Orbit/Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/build-closure-release -R 'icon_theme_service|icon_image_provider' --output-on-failure
  ```

- [ ] **Step 3: Implement mode-aware provider URLs and cache keys.**

  Encode `mode=symbolic` for semantic URLs. Parse the query with `QUrlQuery`
  in `AstreaIconImageProvider`; default to `FullColor`. Add render mode to the
  cache key. For `Symbolic`, convert the resolved QIcon image to an
  alpha-preserving monochrome image with white RGB channels; preserve the
  built-in fallback path when no theme icon resolves.

- [ ] **Step 4: Route facade semantic calls to symbolic mode.**

  Change `AppStateFacade::themedIconSource()` to call
  `symbolicIconSourceForNames()`. Keep `fileIconSource()` on `FullColor` even
  when it receives a semantic candidate override from a file view.

- [ ] **Step 5: Run focused tests and commit.**

  ```bash
  rtk run cmake --build /home/agony/GitHub/Orbit/Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/build-closure-release --target tst_icon_theme_service tst_icon_image_provider -j2
  rtk run ctest --test-dir /home/agony/GitHub/Orbit/Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/build-closure-release -R 'icon_theme_service|icon_image_provider' --output-on-failure
  ```

  ```bash
  git add Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/native/src/services/icon_theme_service.h Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/native/src/services/icon_theme_service.cpp Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/native/src/runtime/astrea_icon_image_provider.cpp Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/native/tests/tst_icon_theme_service.cpp Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/native/tests/tst_icon_image_provider.cpp
  git commit -m "fix: render semantic icons symbolically"
  ```

### Task 5: Correct semantic routes and device delegate data

**Files:**
- Modify: `Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/components/layout/Sidebar.qml`
- Modify: `Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/native/tests/tst_device_controller.cpp`
- Modify: `Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/tests/test_explorer_qml.py`

**Interfaces:**
- Consumes the unchanged `DeviceEntry` fields decoded by
  `RustBackendClient` and exposed by `AppStateFacade::deviceModel()`.

- [ ] **Step 1: Add failing binding/typed-device assertions.**

  Extend the typed-device test to compare title, subtitle, and icon after a
  backend completion. Extend the QML test to require `modelData.id`,
  `modelData.title`, `modelData.subtitle`, `modelData.icon`, and the safe
  removable/internal fallback, while rejecting the old `model.icon` and
  `model.title` bindings in the device delegate.

- [ ] **Step 2: Run the focused tests and confirm the QML assertion fails.**

  ```bash
  rtk run cmake --build /home/agony/GitHub/Orbit/Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/build-closure-release --target tst_device_controller -j2
  rtk run ctest --test-dir /home/agony/GitHub/Orbit/Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/build-closure-release -R device_controller --output-on-failure
  rtk run python3 -m unittest discover -s Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/tests -p 'test_explorer_qml.py'
  ```

- [ ] **Step 3: Fix device bindings without changing Rust.**

  Replace each device delegate `model.*` access with `modelData.*`. Bind the
  backend `icon`, `title`, and `subtitle`; use `drive-removable-media` or
  `drive-harddisk` only when `modelData.icon` is empty. Render the existing
  subtitle in the current two-line device row style.

- [ ] **Step 4: Run focused tests and commit.**

  ```bash
  rtk run cmake --build /home/agony/GitHub/Orbit/Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/build-closure-release --target tst_device_controller -j2
  rtk run ctest --test-dir /home/agony/GitHub/Orbit/Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/build-closure-release -R device_controller --output-on-failure
  rtk run python3 -m unittest discover -s Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/tests -p 'test_explorer_qml.py'
  ```

  ```bash
  git add Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/components/layout/Sidebar.qml Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/native/tests/tst_device_controller.cpp Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/tests/test_explorer_qml.py
  git commit -m "fix: render native sidebar device data"
  ```

### Task 6: Build, install, run full validation, and visually review

**Files:**
- Build: `/home/agony/GitHub/Orbit/Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/build-closure-release`
- Install prefix: `/home/agony/.local/share/AstreaNative`
- Runtime launcher: `/home/agony/.config/hypr/bindings/keybindings.lua`

- [ ] **Step 1: Build and install the exact `Super+E` runtime.**

  ```bash
  rtk run cmake --build /home/agony/GitHub/Orbit/Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/build-closure-release --target astrea-explorer -j2
  rtk run cmake --install /home/agony/GitHub/Orbit/Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/build-closure-release --prefix /home/agony/.local/share/AstreaNative
  ```

- [ ] **Step 2: Run the full native and QML suites.**

  ```bash
  rtk run ctest --test-dir /home/agony/GitHub/Orbit/Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/build-closure-release --output-on-failure
  rtk run python3 -m unittest discover -s Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/tests -p 'test_explorer_qml.py'
  rtk run git diff --check
  ```

- [ ] **Step 3: Run the normal installed-root offscreen self-test.**

  ```bash
  rtk run env QT_QPA_PLATFORM=offscreen ASTREA_ROOT=/home/agony/.local/share/AstreaNative/share/Astrea ASTREA_EXPLORER_START_PATH=/home/agony timeout 20s /home/agony/.local/share/AstreaNative/bin/astrea-explorer --self-test
  ```

  Expected: exit code `0`, loading the installed `Apps/Explorer/Main.qml`, with
  no missing-runtime or QML-warning failure.

- [ ] **Step 4: Perform visual review of the installed app.**

  Launch through the existing `Super+E` binding. Verify in dark mode that
  semantic sidebar icons are light/tintable, file icons remain full-color,
  device rows show title/subtitle/icon, favorite rows displace around the
  insertion marker, cancellation restores order, commit survives restart,
  sidebar scrolling does not corrupt the target, and filesystem URL drops
  still navigate/copy as before. Capture a screenshot if the desktop capture
  tooling is available; otherwise record the exact runtime/self-test evidence
  and the manual checks performed.

- [ ] **Step 5: Verify scope and hand off.**

  ```bash
  rtk run git status --short
  rtk run git log -6 --oneline
  ```

  Confirm no unrelated pre-existing file is staged or reverted, list every
  focused/full test result, and report any visual limitation explicitly.
