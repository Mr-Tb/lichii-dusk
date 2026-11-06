<!--
# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# SPDX-FileCopyrightText: 2018 yuzu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later
-->

<p align="center">
  <b>English</b> | <a href="README.ar.md">العربية</a>
</p>

# Lichii for The Duskbloods (Network Test)

**This is a fork of [Frizti/Lichii](https://github.com/Frizti/Lichii) (`frizti-v2`), tuned to run *The Duskbloods Network Test* (program `0400D1D02399A000`) on Windows PCs with NVIDIA and Intel GPUs.**
It pairs with the [playDB-CNT](https://github.com/sikoGD/playDB-CNT) launcher by sikoGD, which provides the local network-test servers the game needs to get in-game.

> Not affiliated with Nintendo, FromSoftware or the upstream projects. No games, keys or firmware are included. Use only files you have legally dumped yourself.

## Performance

Measured by the maintainer in actual play:

| | |
|---|---|
| CPU | Intel Core i5-12400F |
| GPU | NVIDIA GeForce RTX 3060 (12 GB), driver 616.56 |
| RAM | 16 GB |
| Average | **~40 fps** |
| Peak | **~50 fps** |
| Heaviest area | **36+ fps** (13-16 fps before this fork's optimizations) |

The emulator is mostly CPU-bound in this game, so a faster CPU helps more than a faster GPU.

## What this fork changes

| Area | Fix |
|---|---|
| Online check | Works with the playDB-CNT launcher: host redirect, loopback-only networking, the launcher's CA in the game's trust store, a BCAT file-name check (error 2122-0001) |
| Rendering | TAA black stripes (native stack spills), the fog volume turning white/NaN (per-layer 3D clears), white waves at the bottom of the screen (NN-upscaler arena clear), two missing Ampere shader instructions |
| Performance | Faster storage-buffer range lookups, clean buffer-sync skipping, and a sleeping (not spinning) GPU front thread |

**GPUs:** tested on **NVIDIA (RTX 3060)**. The fixes are vendor-neutral Vulkan, so **Intel Arc / Intel iGPUs and AMD should work, but are untested**. Please report your results. Use a current GPU driver.

## Download

**[Download the latest release (Lichii-Duskbloods.zip)](https://github.com/Mr-Tb/lichii-dusk/releases/latest)**

## Installation

You need:
- Windows 10/11 x64 and a Vulkan 1.3 GPU (NVIDIA recommended, see above)
- Your own `prod.keys` and `title.keys`
- Your own Switch 2 firmware dump (tested with 23.0.1)
- Your own dump of *The Duskbloods Network Test*
- The **[playDB-CNT launcher](https://github.com/sikoGD/playDB-CNT)** (`Play.Duskbloods.exe`). Follow its README for its own setup.

Steps:

1. **Extract** `Lichii-Duskbloods.zip` to a folder with a short path, e.g. `C:\Games\Lichii-Duskbloods\`. Keep the `user` folder next to `lichii.exe`: it makes the emulator portable and already holds the tuned settings.
2. **Keys:** copy `prod.keys` and `title.keys` into `user\keys\`. Or start `lichii.exe` and use **Tools > Install Decryption Keys**.
3. **Firmware:** start `lichii.exe`, then **Tools > Install Firmware > From Folder** (or **From ZIP**) and pick your firmware dump. Close the emulator afterwards.
4. **Launcher:** set up [playDB-CNT](https://github.com/sikoGD/playDB-CNT) as its README describes. Then edit its `launcher_settings.json` so `emulator` points to this `lichii.exe`:
   ```json
   {
     "emulator": "C:\\Games\\Lichii-Duskbloods\\lichii.exe",
     "game": "C:\\Games\\THE DUSKBLOODS NETWORK TEST VER"
   }
   ```
   Use double backslashes. `game` is your game dump folder.
5. **Play:** run `Play.Duskbloods.exe` and press **Play**. Always start the game through the launcher: started directly, the online check fails.

The first minutes stutter while shaders compile; this improves with each session.

## Recommended settings (already set in the zip)

| Setting | Value | Why |
|---|---|---|
| Graphics > Advanced > GPU Mode | **Fast** | Big CPU saving |
| Extras > Asynchronous shader compilation | **On** | Fewer shader stutters |
| Graphics > VSync | **Mailbox** | Presenting does not block rendering |
| Async presentation | **Off** | Faster in this game |
| Window adapting filter | Bilinear | Cheapest |
| Error applet | HLE | The LLE applet crashes the emulator |
| Docked mode | On | |

## Troubleshooting

- **Stuck on "checking save data" or network errors:** start through `Play.Duskbloods.exe`, not `lichii.exe` directly.
- **Error 2153-0513 at boot:** a save got corrupted, e.g. by force-closing during a save. Move the game's save folder under `user\nand\user\save\` aside and start again.
- **Black screen after loading:** close and relaunch. A rare hang has been seen once.
- **Black screen on first boot after updating:** delete the shader cache in `user\shader\`.
- **Missing DLL errors:** extract the whole zip. Do not run `lichii.exe` from inside the archive.

## Building from source

Branch [`dusk-frizti`](https://github.com/Mr-Tb/lichii-dusk/tree/dusk-frizti). Build with MSYS2 UCRT64 + clang, exactly as upstream Lichii.

## Credits and license

Built on [Frizti/Lichii](https://github.com/Frizti/Lichii), [zortos293/Lichii](https://github.com/zortos293/Lichii), [Eglantine](https://github.com/Djihads80/Eglantine), Eden, yuzu and Sudachi. The launcher and network-test servers are [playDB-CNT](https://github.com/sikoGD/playDB-CNT) by sikoGD. Licensed under GPL-3.0-or-later (see [LICENSE.txt](LICENSE.txt)).
