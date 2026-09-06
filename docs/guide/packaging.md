# Packaging Applications

`huxerui package` builds an enabled platform and publishes its distributable output under `dist/<platform>`.
Release is the default profile for packaging; an explicit `--profile` selects another configured profile.

```bash
huxerui package windows
huxerui package macos,linux --profile release
```

Packaging uses the same application target, resources, libraries, and platform shell as `build` and `run`.
Intermediate files stay under `.huxerui/package`, and the CLI replaces a platform's published directory only after the new artifacts are complete.

Desktop outputs are platform-native:

- Windows produces one self-contained Burn setup executable containing the application MSI and a HuxerUI installer interface.
- macOS produces one DMG containing the application bundle and an Applications link.
- Linux produces one AppImage.
- Android publishes the generated APK, iOS publishes its built application bundle, and Web publishes the generated deployment files.

Normal `build` and `run` do not build installer targets or require packaging tools.
Windows downloads the pinned WiX v5 package dependencies only during `package` and verifies their SHA-256 hashes.
Windows setup generation currently supports x64 applications.
Running the restored WiX tool requires `Microsoft.NETCore.App` 6.0 or newer; HuxerUI reports this package-only prerequisite without requiring a system-wide WiX installation.
Linux requires `appimagetool`, `patchelf`, and binutils on `PATH` for `package`; macOS uses the system `otool`, `install_name_tool`, `lipo`, `codesign`, and `hdiutil`.

## Application payloads

CMake install rules are the only source of application files placed into desktop packages.
The generated platform shell installs the application executable or bundle and its final HuxerUI resource package, then collects non-system dynamic dependencies recursively during `cmake --install`.
Deployment scans the original binaries, copies dependencies, fixes loading paths in the staged copies, and verifies that the staged application no longer resolves non-system dependencies outside the package.
Each package operation uses a fresh staging directory.

Libraries loaded through `LoadLibrary`, `dlopen`, or an equivalent runtime mechanism may not appear in the binary dependency graph.
Declare those additional roots with `huxerui_add_runtime_dependencies`; the helper installs them and collects their dependencies:

```cmake
huxerui_add_runtime_dependencies(my_app
        TARGETS image_codec
        FILES "${vendor_runtime_file}"
        SEARCH_DIRECTORIES "${vendor_runtime_directory}"
)
```

`TARGETS` accepts shared-library and module targets, including imported targets; application builds also build any declared local targets.
`FILES` accepts prebuilt dynamic libraries and, on macOS, complete `.framework` or `.bundle` directories.
Relative file and search paths are resolved against the declaring source directory, and generator expressions select the active build configuration.
Conditional file and search expressions that evaluate to an empty value are skipped, and imported targets retain the visibility of their declaring directory.
Search hints include the application directory, declared targets, and directly discoverable linked CMake targets; provide `SEARCH_DIRECTORIES` for other locations, including target paths hidden behind conditional link expressions.
Search directories are packaging inputs, not paths retained by the installed application.

The optional `DESTINATION` selects a subdirectory relative to the Windows application directory, macOS bundle's `Contents`, or Linux AppDir's `usr` directory.
Without it, Windows libraries go beside the executable, macOS libraries and frameworks go into `Frameworks` (bundles into `PlugIns`), and Linux libraries go into `lib`.
Dependencies of explicitly placed modules still use the platform's default library directory.
The application remains responsible for locating and loading its explicitly declared modules.

Install data and configuration with the existing application component:

```cmake
get_target_property(app_install_component my_app HUXERUI_APPLICATION_INSTALL_COMPONENT)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/vendor/codec-settings.json"
        DESTINATION config
        COMPONENT "${app_install_component}"
)
```

Use an appropriate destination and platform condition for each target system.
Missing required dependencies, conflicting payloads, architecture mismatches, broken links, loading-path errors, and invalid final signatures fail packaging before `dist` is replaced.
Native binaries installed with application-owned rules also participate in the final dependency validation.
System runtimes cannot be explicitly bundled through the helper or application-owned installation rules.
The CLI wraps the validated installation tree; CMake owns dependency deployment.

## Application icons

Application icons belong to the generated platform shell and use each platform's native format.
New applications start with the HuxerUI brand mark as an editable placeholder.
Replace the generated file in place; no CMake option or HuxerUI resource registration is required:

- Windows uses `platform/windows/app.ico` for the application executable, installer executable, Burn setup, Start menu and desktop shortcuts, and the installed-app entry.
- macOS uses `platform/macos/AppIcon.icns` for the application bundle displayed by Finder, Dock, and the application switcher.
- Linux uses `platform/linux/package/<target>.svg` for the AppImage and its desktop entry.
- Android uses the legacy density icons and adaptive icon resources under `platform/android/app/src/main/res`.
- iOS uses the complete `platform/ios/App/Assets.xcassets/AppIcon.appiconset` catalog.
- Web uses `platform/web/favicon.svg` for browser tabs and `apple-touch-icon.png` for home-screen bookmarks.

The Windows installer reuses the application icon, and the macOS DMG retains its standard volume icon.
HuxerUI does not convert a common source image into platform icon formats during application builds.

## Desktop system requirements

Windows deployment targets the default Windows 10 version 1607 or later platform and keeps Windows system DLLs, API sets, and UCRT system-owned.
System DLLs are identified by the deployment module's explicit Windows library list; a third-party DLL is not excluded merely because it resides in `System32` or `SysWOW64`.
The optional Windows 7 backend configuration does not have a packaging redistribution policy and is rejected by `package`.
Required Release VC++ runtime DLLs are deployed beside both the application and the independently staged installer interface, so target machines do not need a separate VC++ Redistributable installation.
The runtime files must come from the matching toolchain's redistributable directories; missing files, older runtime versions, and Debug CRT dependencies fail packaging.
HuxerUI uses CMake's detected `MSVC_REDIST_DIR` and falls back to the active Visual Studio compiler or developer environment when CMake does not yet recognize a newer toolset.
Set `MSVC_REDIST_DIR` explicitly when the toolchain stores its redistributables in a separate location; an explicit directory remains authoritative and an invalid one fails packaging.
Application publishers own updates to these app-local runtime files.
WiX and its .NET runtime are build-machine tools, not installed-application prerequisites.

macOS keeps Apple system frameworks and `/usr/lib` libraries system-owned, including libraries supplied through the dyld shared cache.
Non-system dylibs and frameworks are copied into the application bundle with their runtime resources and symbolic links.
Deployment rewrites load commands and runpaths to use bundle-relative locations, checks required architectures and declared minimum macOS versions, and applies ad-hoc signatures from nested code outward after modification.
Developer ID signing and notarization belong to the application's release pipeline and must follow dependency deployment; the default ad-hoc signature does not provide Gatekeeper distribution approval.

Linux keeps glibc, the ELF loader, the system C++ runtime, graphics drivers, and the distribution-provided GTK 4.14+, libepoxy, Pango, Cairo, GIO, and libsoup 3 runtime stack system-owned.
The desktop stack's resolved distribution-library dependency closure is excluded; other application libraries are bundled even when installed in a system search directory.
Custom desktop-stack builds outside distribution library directories are rejected.
The target machine needs the corresponding runtime packages, not their development packages or the HuxerUI SDK.
Bundled ELF files use `$ORIGIN`-relative RUNPATH entries, and versioned library link chains are preserved.
Deployment prints the packaged binaries' `GLIBC`, `GLIBCXX`, and `CXXABI` requirements; build and test release packages on the intended minimum environment.
The official SDK's glibc 2.35 requirement alone does not establish an application's minimum system requirements or provide GTK through the AppImage.

## Custom Windows installer interface

New Windows application shells contain an editable installer application under `platform/windows/package`:

```text
platform/windows/package/
  Bundle.wxs.in
  Package.wxs.in
  resources/
    strings/
      default.properties
  src/
    app.cpp
    main.cpp
```

Edit `src/app.cpp` and `resources` with ordinary HuxerUI components and assets.
The generated interface resolves its text through the ordinary HuxerUI resource system: `default.properties` is the required fallback, and locale catalogs such as `zh.properties`, `ja.properties`, or `pt-BR.properties` follow the same language-tag selection and fallback rules as application resources.
Windows supplies localized text for the native folder picker, while messages returned by the installation engine remain engine-owned text.
Edit the WiX sources for product metadata or MSI behavior that belongs to the Windows package.
Keep installation mechanics in Burn instead of reimplementing file copying, elevation, rollback, repair, or uninstall in the interface.
The generated interface displays the expanded default installation directory, accepts an absolute path, opens the Windows folder picker from its Browse button, and lets the user choose whether to create a desktop shortcut.
The Start menu shortcut is always installed so the application remains discoverable through the normal Windows application surface.
Taskbar pinning does not belong to setup: an application that supports it must request the operation from its foreground interface and let Windows obtain user confirmation.

The Windows-only `<huxerui/windows/installer.h>` API exposes one root-owned session:

```cpp
#include <huxerui/huxerui.h>
#include <huxerui/windows/installer.h>

using namespace huxerui;
using namespace huxerui::windows;

View InstallerPage() {
  const InstallerHandle installer = UseInstaller();
  const TaskScope tasks = UseTaskScope();
  const InstallerStatus status = installer.Status();
  if (status.phase == InstallerPhase::Ready && status.product == InstallerProductState::Absent) {
    return Button("Choose destination and install").OnClick([installer, status, tasks] {
      tasks.Launch([installer, status]() -> Task<void> {
        const std::optional<std::filesystem::path> selected =
            co_await installer.ChooseDestinationAsync(status.default_destination);
        if (selected) {
          installer.Install({
              .destination = *selected,
              .create_desktop_shortcut = status.default_create_desktop_shortcut,
          });
        }
      });
    });
  }
  return ProgressBar(status.progress);
}

const Application application{
    InstallerPage,
    {.root_hooks = {InstallInstallerSession}},
};
```

`InstallerHandle` starts install, repair, and uninstall operations, requests cooperative cancellation, and answers the current identified prompt.
`InstallerInstallOptions` overrides the authored destination or desktop-shortcut choice for one install request; leaving either field unset preserves the matching Burn variable.
`ChooseDestinationAsync()` opens the Windows folder picker from a Task launched by the component's `TaskScope` and returns no value when the user dismisses it.
`InstallerStatus` is the single observable status value for phase, detected product state, expanded defaults, action, progress, current package, prompt, failure, and restart requirement.
Do not use this API in the ordinary application executable or introduce another installer state store beside it.

Signing, notarization, store submission, and platform-specific release credentials remain application and release-pipeline responsibilities.
