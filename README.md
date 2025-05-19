# BetterDiscordAutoUpdate
Performs the automatic installation of [BetterDiscord](https://github.com/BetterDiscord/BetterDiscord) after each Discord updates.

## OS
Supported OS:
- Windows

## Known Issues
- After a discord update, discord starts without BD installed and you need to manually close and reopen discord to re-install BD once.

## Installation\Usage
1. Download `BetterDiscordAutoUpdate.exe` from the [latest release](https://github.com/nicola02nb/BetterDiscordAutoUpdate/releases/latest) page.
2. Double-click to open `BetterDiscordAutoUpdate.exe` (A terminal should open).
3. If the execution went well, you're done.
4. Now BetterDiscord will install automatically after each Discord update.

### Update
1. Go to your %LOCALAPPDATA%\Disocrd folder
    1. If there is Update.moved.exe, you shuld first delete the older Update.exe
    2. Then rename Update.moved.exe to Update.exe
2. Proceed with a normal [Installation](#installationusage)

## How it works
`BetterDiscordAutoUpdate.exe` is a `.exe` file for `Windows` that renames the original `Update.exe` to `Update.moved.exe` and self-copies into the Discord folder as `Update.exe` to catch all executions of that file and perform checks and actions to keep BetterDiscord installed.

### Removing Auto Update
1. Go to your Discord folder at `%LOCALAPPDATA%\Discord`.
2. Delete `Update.exe`.
3. Rename `Update.moved.exe` to `Update.exe`.
4. Done.

### Installation for `PTB` or `Canary`
The `BetterDiscordAutoUpdate.exe` should be run in a terminal by adding a launch argument:

#### PTB:
```ps1
.\BetterDiscordAutoUpdate.exe ptb
```

#### Canary:
```ps1
.\BetterDiscordAutoUpdate.exe canary
```

## Build
BetterDiscordAutoUpdate has been written in `C++17` using `Visual Studio Community` with the `Desktop development with C++` workload.
