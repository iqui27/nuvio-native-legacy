# Android TV and Mobile interface modes

The standard Android APK contains both interfaces. It starts in TV mode; choose **Settings > Appearance > Interface mode > Mobile** to use phone layouts and direct touch scrolling. Choose TV again to restore the remote-oriented layout. Both modes support the settings control by touch and by remote.

The selection is stored as `interfaceModoLocal` in this device's settings. Account synchronization and profile changes do not replace it. Switching mode preserves the installed package, account, settings, and library. It updates the native canvas and Android window in place; it does not restart the Activity or create a second SDL thread.

TV keeps the original 1920×1080 logical canvas and landscape policy. Mobile follows the drawable aspect ratio with a logical short edge of 1080, supports portrait/landscape browsing, protects keyboard input around cutouts, and switches full-screen playback to landscape. Returning to TV restores its window, buffer, and keyboard policy.

The application ID remains `space.nuvio.nativelegacy`, and release builds use the existing release signing configuration. Updates signed with the same key preserve the installed app and its data. Debug builds use a debug signature and cannot replace a release-signed installation.

