# Android TV and Mobile interface modes

The standard Android APK contains both interfaces. Automatic mode selects TV on Android TV / Google TV devices and Mobile on phones and tablets. Detection uses Android TV UI mode and television/Leanback features; it does not use screen shape or connected controllers. Choose **Settings > Appearance > Interface mode** to override Automatic with TV or Mobile. Both modes support the settings control by touch and by remote.

Existing saved TV (`0`) and Mobile (`1`) choices retain their meaning; new installations default to Automatic (`2`). The selection is stored as `interfaceModoLocal` in this device's settings. Account synchronization and profile changes do not replace it. Switching mode preserves the installed package, account, settings, and library. It updates the native canvas and Android window in place; it does not restart the Activity or create a second SDL thread.

TV keeps the original 1920×1080 logical canvas and landscape policy. Mobile follows the drawable aspect ratio with a logical short edge of 1080, supports portrait/landscape browsing, protects keyboard input around cutouts, and switches full-screen playback to landscape. Returning to TV restores its window, buffer, and keyboard policy.

The application ID remains `space.nuvio.nativelegacy`, and release builds use the existing release signing configuration. Updates signed with the same key preserve the installed app and its data. Debug builds use a debug signature and cannot replace a release-signed installation.
