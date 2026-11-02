# Iraq Theater Command (OIFsim)

Desktop builds and over-the-air updates for **Iraq Theater Command**, a strategy game of the 2003 Iraq campaign and the occupation that followed.

## Download

Get the latest build from [Releases](https://github.com/kkaigle/OIFsim/releases/latest):

- **Mac (Apple silicon):** `Iraq-Theater-Command-mac-arm64.zip`. Unzip it, move the app to Applications, then right-click it and choose Open the first time, because the app is not notarized.
- **Windows (64-bit):** `Iraq-Theater-Command-windows-x64.zip`. Unzip it and run `Iraq Theater Command.exe`.

## Updates

Version 1.61.0 and later check [`ota/update.json`](ota/update.json) in this repository. When a newer game version is pushed, the app downloads it in the background (about 1 MB), checks its SHA-256, and offers to restart into it. Campaigns are saved and backed up first. If an update fails to start, the app rolls back to the previous version.

## Publishing an update

From the desktop project:

```
ITC_OTA_REPO_DIR=/path/to/OIFsim node release.js
```

That writes `ota/update.json` and `ota/game-<ver>.html` into this repo. Commit and push to `main`, and every installed copy picks it up within a few hours (or right away from Campaign › Check for updates).

When the desktop app version in `package.json` changes, also create a GitHub release with the new Mac and Windows zips attached. Installed apps then show a "New desktop app" download button.
