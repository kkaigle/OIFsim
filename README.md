# Iraq Theater Command (OIFsim)

Desktop builds and over-the-air updates for **Iraq Theater Command**, a strategy game of the 2003 Iraq campaign and the occupation that followed.

## Download

Get the latest build from [Releases](https://github.com/kkaigle/OIFsim/releases/latest):

- **Mac (Apple silicon):** `Iraq-Theater-Command-mac-arm64.zip`. Unzip it, move the app to Applications, then right-click it and choose Open the first time, because the app is not notarized.
- **Windows (64-bit):** `Iraq-Theater-Command-windows-x64.zip`. Unzip it and run `Iraq Theater Command.exe`.

## Updates

Version 1.61.0 and later check this repository's latest release for `update.json`. When a newer game version is published, the app downloads it in the background (about 1 MB), checks its SHA-256, and offers to restart into it. Campaigns are saved and backed up first. If an update fails to start, the app rolls back to the previous version.

## Publishing a release

From the desktop project, run `node release.js`. It writes `release/update.json` and `release/game-<ver>.html`. Then create a release with those files attached:

```
gh release create v<ver> release/* -R kkaigle/OIFsim --title "v<ver>" --notes "..."
```

Attach the Mac and Windows zips as well whenever the desktop app version in `package.json` changes.
