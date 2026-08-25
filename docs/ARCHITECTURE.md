# Orbit architecture

Orbit has one active production application: Explorer. The source tree is
organized by ownership rather than by the historical AstreaOS filesystem
image.

## Ownership

- QML owns presentation, layout, delegates, animation, and transient visual
  state.
- C++/Qt owns application lifecycle, the QML engine, state, navigation,
  selection, settings, controllers, filesystem orchestration, recent state,
  and portal integration.
- `apps/explorer/backend` owns the Rust filesystem backend and its existing
  worker and JSON contracts.
- `services/launch` owns `astrea-launch`.
- `services/filechooser-portal` owns the D-Bus file chooser portal.

Shared UI is exposed as real QML modules:

```qml
import Astrea.Components 1.0 as UI
import Astrea.Files 1.0 as AstreaFiles
import Astrea.I18n 1.0 as AstreaI18n
```

The public wrapper files in `Astrea.Components` remain part of the module API;
categorized implementations sit below them.

## Dependency direction

Explorer QML depends on the shared QML modules and the native `Astrea.Explorer`
projection. C++ owns the engine and native services, and invokes the Rust
components through their existing executable protocols. Rust services do not
depend on QML or C++ implementation details.

There are no production source-tree compatibility symlinks. All shared QML is
copied into the development runtime staging area and installed as real files.

## Source and installed layouts

Development output is staged below the CMake build tree at
`build/<configuration>/apps/explorer/runtime/Astrea`. Canonical QML modules
are available under `Astrea/`; compatibility runtime paths remain available
under `Core/components`, `Features/Files`, and `System/i18n` so existing
installed consumers keep their contract.

Installed output is rooted at `${prefix}/share/Astrea` and contains the same
runtime layout. The Explorer binary resolves roots in this order:

1. explicit `ASTREA_ROOT`;
2. `${prefix}/share/Astrea` derived from the executable;
3. the configured development staging root;
4. `${HOME}/.local/share/Astrea`;
5. a diagnostic failure.

The clean-install gate removes both explicit and development overrides, which
proves that installed-prefix resolution is sufficient.

## Legacy boundary

`old/` is preserved source only. It is not included by the root CMake project,
Cargo workspace, runtime staging, or installation. Generated compiler and
Python cache output is not stored there.
