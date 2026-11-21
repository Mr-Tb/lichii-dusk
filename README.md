<!--
# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# SPDX-FileCopyrightText: 2018 yuzu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later
-->

# Lichii for The Duskbloods (Network Test)

# ⚠️ Launcher not included: [download it here](https://github.com/sikoGD/playDB-CNT/releases/latest) / المشغّل غير مرفق: [حمّله من هنا](https://github.com/sikoGD/playDB-CNT/releases/latest)

<p align="center"><b>Choose your language / اختر اللغة</b></p>

<a id="english"></a>
<details name="lang" open>
<summary><b>English: open the guide</b></summary>

**This is a fork of [zortos293/Lichii](https://github.com/zortos293/Lichii) (nx2, newest upstream after build 341), tuned to run *The Duskbloods Network Test* (program `0400D1D02399A000`) on Windows PCs.**
It pairs with the [playDB-CNT](https://github.com/sikoGD/playDB-CNT) launcher by sikoGD, which provides the local network-test servers the game needs to get in-game. The launcher is a separate download: see the two boxes below.

> Not affiliated with Nintendo, FromSoftware or the upstream projects. No games, keys or firmware are included. Use only files you have legally dumped yourself.

# ⚠️ The launcher is NOT in the zip: download it separately

### ➡️ [Download `Play.Duskbloods.exe` from the playDB-CNT releases](https://github.com/sikoGD/playDB-CNT/releases/latest)

Put `Play.Duskbloods.exe` in the same folder as `lichii.exe`, then follow [Launcher setup](#launcher-setup).

# 🛡️ Windows Defender may call the launcher a virus: it is a false positive

- **What you may see:** Windows Defender blocks or deletes `Play.Duskbloods.exe` with a name like `Trojan:Win32/Wacatac.B!ml`.
- **Why it happens:** the launcher is a Python program packed into a single exe with PyInstaller. When it starts, it unpacks itself and then runs, and the exe is not digitally signed. Real malware is often packed the same way, so Defender's automatic guesswork flags it. The `!ml` at the end means "machine learning": a guess based on how the file looks, not a match with any known virus.
- **Why we are sure it is a false positive:** every launcher version we scanned (1.3.2, 1.4.0 and 1.5.0) gets the same guess, and on some days the very same file scans clean. The launcher's source code is public in [its repository](https://github.com/sikoGD/playDB-CNT). `lichii.exe` and everything else in our zip scan clean. That is why the launcher is no longer bundled: it made our whole zip look infected.
- **How to keep it:** open **Windows Security > Virus & threat protection > Protection history**, select the launcher entry, then **Actions > Allow** (or **Restore**). Do this only for the file you downloaded from the official playDB-CNT releases page.

## What's new in v4

- **Rebased on the newest zortos293/Lichii nx2** (after build 341; v3 was build 177): hundreds of upstream fixes, and fewer shader freezes (a draw whose shader is still building waits at most 8 ms, then shows a frame later).
- **Much faster lighting:** the game's lighting shader, the heaviest part of each frame, now reads its light data directly instead of searching every buffer for it. Our own fix, for every GPU. At 2x on an RTX 3060: **30.4 → 39.4 fps**.
- **Shorter GPU waits:** the texture cache writes back at most one GPU-drawn image per frame with a full GPU wait; the rest wait a frame. **39.5 → 43.3 fps** in game, better 1% lows, and less texture churn when memory runs low.
- **Online fix** for the newer upstream base: its IPv6 connections now reach the launcher's servers (error 2321-4992 otherwise).
- **The launcher is now a separate download.** Windows Defender kept flagging it by mistake, and with it our whole zip (see the warning at the top).
- **For bug reports:** when the game closes, the log ends with an `Nx2Summary run` line (average fps, 1% and 0.1% lows), and the GPU's busy share is logged once a second.

## Performance

Measured by the maintainer in actual play (hub, heaviest area and matches):

| | |
|---|---|
| CPU | Intel Core i5-12400F |
| GPU | NVIDIA GeForce RTX 3060 (12 GB) |
| RAM | 16 GB |
| v4 at **2x**, DLSS Quality | **~43 fps** in game (1% low ~22 fps) |
| Same PC and settings before v4's fixes | ~30 fps |
| v3 at 1x with FSR 3.1 (for reference) | ~47 fps match median |

At 2x the GPU is the limit (busy ~90% of each frame), so a stronger GPU helps directly.

## Resolution and upscaling (pick by monitor)

The **Resolution** setting decides quality and fps; the DLSS mode barely does, so keep DLSS on **Quality**. The game itself draws a 1108x624 picture: the resolution setting multiplies it, then FSR 3.1 or DLSS upscales it to your screen.

| Monitor | Resolution | Renders | Upscale to screen | GPU cost vs 1x | Use when |
|---|---|---|---|---|---|
| 1080p | 1x | 1108x624 | 1.73x | 1x | Weaker GPUs (RTX 3050/2060 class), laptops |
| 1080p | **1.5x (recommended)** | 1662x936 | 1.15x | ~2.25x | RTX 3060 class and up |
| 1080p | 2x | 2216x1248 | scaled down | 4x | Strong GPUs; small gain over 1.5x |
| 1440p | 1x | 1108x624 | 2.31x | 1x | Weak GPUs (soft image) |
| 1440p | **1.5x (recommended)** | 1662x936 | 1.54x | ~2.25x | RTX 3060 class |
| 1440p | 2x | 2216x1248 | 1.15x | 4x | RTX 4070 / 5070 Ti class and up |

AMD and Intel cards use FSR 3.1: same guidance, a bit softer. Avoid 2x on mid-range cards.

## What this fork changes

| Area | Fix |
|---|---|
| Online check | Works with the playDB-CNT launcher: host redirect (IPv4 and IPv6), loopback-only networking, the launcher's CA in the game's trust store, a BCAT file-name check (error 2122-0001) |
| Rendering | TAA black stripes (native stack spills), the fog volume turning white/NaN (per-layer 3D clears), white waves at the bottom of the screen (NN-upscaler arena clear), two missing Ampere shader instructions |
| Performance | Faster storage-buffer range lookups, clean buffer-sync skipping, a sleeping (not spinning) GPU front thread |
| v2: upstream build 117 | The game's neural-network (HMMA) shaders run on the GPU's matrix units, NaN-safe float math, and 100+ other upstream fixes |
| v3: upstream build 177 | FSR 3.1 in place of the game's DLSS pass, optional NVIDIA DLSS with live switching, fixes for the save freeze (2153-0002), crackling sound, Bot Match crashes, hub laser lines and flicker, and pixelated HUD above 1x |
| v4: newest upstream | Direct light-data reads in the lighting shader (+30% fps at 2x), one waited texture write-back per frame (+10%), the IPv6 online fix |

**Tested on:** an **NVIDIA RTX 3060** GPU with an **Intel Core i5-12400F** CPU. Other NVIDIA GPUs should behave the same; AMD and Intel GPUs are untested. Use a current GPU driver.

## Download

**[Download v4 (Lichii-Duskbloods.zip)](https://github.com/Mr-Tb/lichii-dusk/releases/tag/duskbloods-v4)**

Older builds: [all releases](https://github.com/Mr-Tb/lichii-dusk/releases).

## Installation

You need:
- Windows 10/11 x64 and a Vulkan 1.3 GPU (NVIDIA recommended, see above)
- Your own `prod.keys` and `title.keys`
- Your own Switch 2 firmware dump (tested with 23.0.1)
- Your own dump of *The Duskbloods Network Test*
- The **[playDB-CNT launcher](https://github.com/sikoGD/playDB-CNT)** (`Play.Duskbloods.exe`), **a separate download** from [its releases](https://github.com/sikoGD/playDB-CNT/releases/latest). See [Launcher setup](#launcher-setup) below.

Steps:

1. **Extract** `Lichii-Duskbloods.zip` to a folder with a short path, e.g. `C:\Games\Lichii-Duskbloods\`. Keep the `user` folder next to `lichii.exe`: it makes the emulator portable and already holds the tuned settings.
2. **Keys:** copy `prod.keys` and `title.keys` into `user\keys\`. Or start `lichii.exe` and use **Tools > Install Decryption Keys**.
3. **Firmware and first start:** start `lichii.exe`, then **Tools > Install Firmware > From Folder** (or **From ZIP**) and pick your firmware dump. Close the emulator afterwards. This first start also writes the emulator's full settings, which the launcher needs.
4. **Launcher:** [download `Play.Duskbloods.exe`](https://github.com/sikoGD/playDB-CNT/releases/latest), put it next to `lichii.exe`, and set it up as described in [Launcher setup](#launcher-setup) below.
5. **Play:** press **Start game** in `Play.Duskbloods.exe`. Always start the game through the launcher: started any other way, the game has no servers and stops at an online connection message.

**Updating from v3:** extract v4 into a new folder and copy your old `user` folder over it (keys, firmware, saves). The first minutes stutter while shaders rebuild for the new version.

## Launcher setup

Download `Play.Duskbloods.exe` from [the playDB-CNT releases](https://github.com/sikoGD/playDB-CNT/releases/latest) and put it next to `lichii.exe`. If Windows Defender blocks it, see the false-positive note at the top. You set everything in its window. It saves your choices to `launcher_settings.json` by itself when you press **Start game**, so do not edit that file by hand. No admin rights, hosts file or certificate steps are needed.

1. Run `Play.Duskbloods.exe`.
2. **Setup tab > Game:** press **Folder** and pick the unpacked `THE DUSKBLOODS NETWORK TEST VER` folder (the one with `base\exefs\main`), or **Browse** for a `.nup` file. It must say *The Duskbloods network test found*. A game folder in or next to the Lichii folder is found automatically.
3. **Setup tab > Emulator:** choose `Lichii - <folder name>` (this `lichii.exe`; it is selected automatically when the launcher sits next to it). No red *No Duskbloods online support* note may show. Do not press any **Patch** button: this build has the online support built in.
4. **Online tab:** keep **Just me** (the default). **Host a room** and **Join a room** are online tests with friends, and their matches do not start yet.
5. **Mods tab** (launcher 1.5.0 and newer): keep **Bot Match** and **CPU player names** on; untick the experimental mods for normal play.
6. Press **Start game** and keep the launcher open while you play.

*Just me* needs no firewall change: everything stays on 127.0.0.1. To host or join a room, allow the launcher and `lichii.exe` through Windows Firewall when Windows asks.

## NVIDIA DLSS (optional)

For NVIDIA RTX cards only. Without it, the emulator uses AMD FSR 3.1, which works on every GPU.

1. **Get NVIDIA's files** (not included in the zip): open **Emulation > Configure > Graphics**, tick **NVIDIA DLSS** and press **Download from NVIDIA** in its panel. Or copy `sl.interposer.dll`, `sl.common.dll`, `sl.dlss.dll` and `nvngx_dlss.dll` from the NVIDIA Streamline SDK 2.14.16 into `user\streamline\`.
2. **Turn it on:** tick **NVIDIA DLSS** and restart the emulator once. The first time, DLSS starts with the next launch.
3. **Switch live:** in game, click the upscaler button in the status bar (or press **F7**) to cycle between FSR3 and the DLSS modes: Auto, Quality, Balanced, Performance, Ultra Performance. Right-click the button to pick one. **Quality** is recommended; set sharpness with the resolution (see above).

If DLSS cannot start, the emulator falls back to FSR 3.1.

## Recommended settings (already set in the zip)

| Setting | Value | Why |
|---|---|---|
| Graphics > Resolution | **1x** | Runs on the most PCs; raise it with the table above |
| Graphics > Anti-aliasing | **None** | FSR 3.1 / DLSS already handle it |
| Graphics > Advanced > GPU Mode | **Fast** | Big CPU saving |
| Extras > Asynchronous shader compilation | **On** | Fewer shader stutters |
| Graphics > VSync | **Mailbox** | Presenting does not block rendering |
| Async presentation | **Off** | Faster in this game |
| Window adapting filter | Bilinear | Cheapest |
| NVIDIA DLSS | Off | Turn it on for an RTX card, see above |
| Error applet | HLE | The LLE applet crashes the emulator |
| Docked mode | On | |

## Known issues

- Brightness flicker or texture pop-in can appear when the PC runs low on memory (RAM plus page file). Close browsers and other heavy programs before playing.
- The first launch stutters while shaders rebuild for the new version.
- AMD and Intel GPUs are untested.
- Windows Defender may flag the launcher by mistake. It is a false positive: see the warning at the top of this guide.
- DLSS needs an RTX card and a one-time download of NVIDIA's files.

## Troubleshooting

- **Stuck at the title menu, or the game says it must be online:** check these in order.
  1. Start the game with **Start game** in `Play.Duskbloods.exe`. Started from `lichii.exe`, its game list or your own shortcut, the game has no servers.
  2. First start right after extracting? Close the game and press **Start game** again. Still stuck: start `lichii.exe` alone once, close it, then use the launcher.
  3. In `lichii.exe`, open **Emulation > Configure > Network** and turn **off** "Redirect host names to a server you run" (Host redirect). When it is on, it replaces the launcher's servers.
  4. In the launcher's **Online** tab, choose **Just me**.
  5. Look for red lines in the launcher's log: missing keys, *Port 18443 is still in use*, or *No Duskbloods online support*.
  6. Edited `launcher_settings.json` by hand? Delete it (it is next to `Play.Duskbloods.exe`) and choose the game and emulator again in the window. A broken file is ignored without a warning.
  7. Still stuck: press **Open logs** in the launcher and send the newest folder in `user\duskbloods_test\runs\` (`emulator.log`, `server.log`, `frontdoor.log`).
- **Error 2321-4992 at the title screen:** the launcher is starting an older Lichii or another emulator. Pick this build's `lichii.exe` in the Setup tab.
- **Stuck on "checking save data" or network errors:** start through `Play.Duskbloods.exe`, not `lichii.exe` directly.
- **Error 2153-0513 at boot:** a save got corrupted, e.g. by force-closing during a save. Move the game's save folder under `user\nand\user\save\` aside and start again.
- **Black screen after loading:** close and relaunch. A rare hang has been seen once.
- **Black screen on first boot after updating:** delete the shader cache in `user\cache\shader\`.
- **Flickering or broken geometry:** delete the line `EDEN_NX2_FRAMES_AHEAD=1` in `nx2_switches.txt` and restart.
- **Lights look wrong (black, flashing or missing):** remove the `#` before `EDEN_NX2_DUSK_LOADED_REBASE=0` in `nx2_switches.txt` (the old, slower light loading) and report it.
- **No DLSS option or button:** it needs an RTX GPU and the four NVIDIA files in `user\streamline\`.
- **Missing DLL errors:** extract the whole zip. Do not run `lichii.exe` from inside the archive.
- **Reporting a problem:** close the game, then send `user\log\eden_log.txt` (check it for anything private first).

## Building from source

Branch [`dusk-v4`](https://github.com/Mr-Tb/lichii-dusk/tree/dusk-v4) (v3: [`dusk-v3r`](https://github.com/Mr-Tb/lichii-dusk/tree/dusk-v3r), v2: [`dusk-v2`](https://github.com/Mr-Tb/lichii-dusk/tree/dusk-v2), v1: [`dusk-frizti`](https://github.com/Mr-Tb/lichii-dusk/tree/dusk-frizti)). Build with MSYS2 UCRT64 + clang, exactly as upstream Lichii.

## Credits and license

Built on [zortos293/Lichii](https://github.com/zortos293/Lichii), [Frizti/Lichii](https://github.com/Frizti/Lichii). The launcher and network-test servers are [playDB-CNT](https://github.com/sikoGD/playDB-CNT) by sikoGD. Licensed under GPL-3.0-or-later (see [LICENSE.txt](LICENSE.txt)). NVIDIA DLSS runs through NVIDIA Streamline (MIT license); NVIDIA's files are downloaded by the user, not shipped.

Special thanks to **Siko** ([sikoGD](https://github.com/sikoGD)), author of the [playDB-CNT](https://github.com/sikoGD/playDB-CNT) launcher. This release owes a lot to his Duskbloods build v30 and its follow-ups. From his work we took how to feed the game's real inputs to the temporal upscaler: the motion-vector scale and jitter conventions, tracking the game's depth and velocity render targets, and keeping the upscaler history when the render region changes size. That is the basis of the sharp FSR 3.1 image. His v32 also pointed the way to the save-freeze (2153-0002) fix, and its pause logging is ported here.

v4 also uses ideas from [Fritchii](https://github.com/Frizti/Fritchii) (the one-waited-write-back cap, the run summary and the Force maximum clocks guard) and from an open upstream pull request, [zortos293/Lichii#559](https://github.com/zortos293/Lichii/pull/559) (report reads that wait only for their own reports). Thanks to JalenJaceX2 for PR #2.

</details>

<a id="arabic"></a>
<details name="lang">
<summary><b>العربية: افتح الدليل</b></summary>

<div dir="rtl">

**هذا المشروع نسخة معدّلة (fork) من [zortos293/Lichii](https://github.com/zortos293/Lichii) (nx2، أحدث نسخة بعد الإصدار 341)، ومحسّنة لتشغيل *The Duskbloods Network Test* (رقم البرنامج `0400D1D02399A000`) على أجهزة Windows.**
يعمل مع المشغّل [playDB-CNT](https://github.com/sikoGD/playDB-CNT) من sikoGD، والذي يوفّر خوادم الشبكة المحلية التي تحتاجها اللعبة للدخول. المشغّل يُحمَّل بشكل منفصل: انظر المربّعين بالأسفل.

> المشروع غير تابع لـ Nintendo أو FromSoftware أو المشاريع الأصلية. لا يتضمن أي ألعاب أو مفاتيح أو firmware. استخدم فقط ملفات استخرجتها بنفسك بطريقة قانونية.

# ⚠️ المشغّل غير موجود في الملف المضغوط: حمّله بشكل منفصل

### ➡️ [حمّل `Play.Duskbloods.exe` من صفحة إصدارات playDB-CNT](https://github.com/sikoGD/playDB-CNT/releases/latest)

ضع `Play.Duskbloods.exe` في نفس مجلد `lichii.exe`، ثم اتبع قسم [إعداد المشغّل](#إعداد-المشغّل).

# 🛡️ قد يعتبر Windows Defender المشغّل فيروسًا: هذا إنذار كاذب

- **ما قد تراه:** يحظر Windows Defender الملف `Play.Duskbloods.exe` أو يحذفه باسم مثل `Trojan:Win32/Wacatac.B!ml`.
- **لماذا يحدث هذا:** المشغّل برنامج مكتوب بلغة Python ومحزوم في ملف exe واحد بأداة PyInstaller. عند تشغيله يفكّ نفسه ثم يعمل، والملف غير موقّع رقميًا. كثير من البرامج الضارة تُحزم بنفس الطريقة، لذلك يشتبه به التخمين الآلي في Defender. الجزء `!ml` في آخر الاسم يعني "تعلّم آلي": أي تخمين مبني على شكل الملف، وليس تطابقًا مع فيروس معروف.
- **لماذا نحن متأكدون أنه إنذار كاذب:** كل إصدارات المشغّل التي فحصناها (1.3.2 و1.4.0 و1.5.0) تحصل على نفس التخمين، وفي بعض الأيام ينجح فحص الملف نفسه دون أي تحذير. الكود المصدري للمشغّل منشور في [مستودعه](https://github.com/sikoGD/playDB-CNT). الملف `lichii.exe` وباقي محتويات ملفنا المضغوط نظيفة في الفحص، ولهذا لم نعد نضع المشغّل داخله: كان يجعل الملف المضغوط كله يبدو مصابًا.
- **كيف تحتفظ به:** افتح **Windows Security > Virus & threat protection > Protection history**، واختر سجل المشغّل، ثم **Actions > Allow** (أو **Restore**). افعل ذلك فقط للملف الذي حمّلته من صفحة إصدارات playDB-CNT الرسمية.

## الجديد في الإصدار الرابع

- **مبني على أحدث نسخة nx2 من zortos293/Lichii** (بعد الإصدار 341، والإصدار الثالث كان مبنيًا على 177): مئات الإصلاحات من المشروع الأصلي، وتجمّد أقل أثناء تجهيز الـ shaders (الرسم الذي لم يجهز الـ shader الخاص به ينتظر 8 ميلي ثانية كحد أقصى، ثم يظهر بعد إطار واحد).
- **إضاءة أسرع بكثير:** الـ shader الخاص بالإضاءة، وهو أثقل جزء في كل إطار، صار يقرأ بيانات الأضواء مباشرة بدل البحث عنها في كل ذاكرة مربوطة. هذا إصلاحنا الخاص ويفيد كل كروت الشاشة. على RTX 3060 بدقة 2x: **من 30.4 إلى 39.4 إطار/ثانية**.
- **انتظار أقل لكرت الشاشة:** ذاكرة الصور تعيد كتابة صورة واحدة على الأكثر في كل إطار مع انتظار كامل لكرت الشاشة، والباقي ينتظر الإطار التالي. **من 39.5 إلى 43.3 إطار/ثانية** داخل اللعب، وأدنى إطارات (1% low) أفضل، وتبديل أقل للصور عندما تقلّ الذاكرة.
- **إصلاح الاتصال** للنسخة الأصلية الجديدة: اتصالات IPv6 صارت تصل إلى خوادم المشغّل (وإلا يظهر الخطأ 2321-4992).
- **المشغّل صار يُحمَّل بشكل منفصل.** كان Windows Defender يصنّفه كتهديد بالخطأ، ومعه ملفنا المضغوط كله (انظر التحذير في الأعلى).
- **لتقارير المشاكل:** عند إغلاق اللعبة ينتهي السجلّ بسطر `Nx2Summary run` (متوسط الإطارات وأدنى 1% و0.1%)، ونسبة انشغال كرت الشاشة تُسجَّل كل ثانية.

## الأداء

قياسات صاحب المشروع أثناء اللعب الفعلي (الـ hub وأثقل منطقة والمباريات):

| | |
|---|---|
| المعالج | Intel Core i5-12400F |
| كرت الشاشة | NVIDIA GeForce RTX 3060 (12 GB) |
| الذاكرة | 16 GB |
| الإصدار الرابع بدقة **2x** مع DLSS Quality | **حوالي 43 إطار/ثانية** داخل اللعب (أدنى 1% حوالي 22) |
| نفس الجهاز والإعدادات قبل إصلاحات الإصدار الرابع | حوالي 30 إطار/ثانية |
| الإصدار الثالث بدقة 1x مع FSR 3.1 (للمقارنة) | حوالي 47 إطار/ثانية وسيط المباريات |

بدقة 2x يكون كرت الشاشة هو الحدّ (مشغول حوالي 90% من كل إطار)، لذلك كرت أقوى يرفع الأداء مباشرة.

## الدقة ورفع الدقة (اختر حسب شاشتك)

إعداد **Resolution** هو الذي يحدد الجودة والأداء، أما وضع DLSS فتأثيره بسيط، لذلك اترك DLSS على **Quality**. اللعبة نفسها ترسم صورة بحجم 1108x624، وإعداد الدقة يضاعفها، ثم يرفعها FSR 3.1 أو DLSS إلى حجم شاشتك.

| الشاشة | الدقة | حجم الرسم | الرفع إلى الشاشة | تكلفة كرت الشاشة مقارنة بـ 1x | متى تستخدمها |
|---|---|---|---|---|---|
| 1080p | 1x | 1108x624 | 1.73x | 1x | كروت أضعف (فئة RTX 3050/2060) واللابتوبات |
| 1080p | **1.5x (مقترح)** | 1662x936 | 1.15x | حوالي 2.25x | فئة RTX 3060 وأعلى |
| 1080p | 2x | 2216x1248 | تصغير | 4x | كروت قوية، والفرق عن 1.5x بسيط |
| 1440p | 1x | 1108x624 | 2.31x | 1x | كروت ضعيفة (صورة ناعمة) |
| 1440p | **1.5x (مقترح)** | 1662x936 | 1.54x | حوالي 2.25x | فئة RTX 3060 |
| 1440p | 2x | 2216x1248 | 1.15x | 4x | فئة RTX 4070 / 5070 Ti وأعلى |

كروت AMD وIntel تستخدم FSR 3.1: نفس الإرشادات مع صورة أنعم قليلًا. تجنّب 2x على الكروت المتوسطة.

## ما الذي تغيّر في هذه النسخة

| المجال | الإصلاح |
|---|---|
| فحص الاتصال | يعمل مع مشغّل playDB-CNT: إعادة توجيه الخوادم (IPv4 وIPv6)، شبكة محلية فقط، إضافة شهادة المشغّل إلى شهادات اللعبة الموثوقة، وإصلاح خطأ BCAT ‏(2122-0001) |
| الرسوميات | الخطوط السوداء (TAA)، الضباب الذي يتحوّل للأبيض، الموجات البيضاء أسفل الشاشة، وتعليمتان مفقودتان في shaders ‏Ampere |
| الأداء | بحث أسرع في نطاقات الذاكرة، تجاوز مزامنة البيانات غير المتغيّرة، وخيط GPU أمامي ينام بدل الانشغال بالانتظار |
| الإصدار الثاني: تحديث 117 | shaders الشبكة العصبية (HMMA) في اللعبة تعمل على وحدات المصفوفات في كرت الشاشة، حسابات أدق مع قيم NaN، وأكثر من 100 إصلاح آخر من المشروع الأصلي |
| الإصدار الثالث: تحديث 177 | FSR 3.1 بدل مرحلة DLSS الخاصة باللعبة، و NVIDIA DLSS اختياري مع التبديل المباشر، وإصلاح تجمّد الحفظ (2153-0002) وطقطقة الصوت وانهيار Bot Match وخطوط الليزر والوميض في الـ hub وتكسّر الواجهة فوق 1x |
| الإصدار الرابع: أحدث نسخة أصلية | قراءة بيانات الأضواء مباشرة في shader الإضاءة (+30% إطارات بدقة 2x)، إعادة كتابة صورة واحدة مع انتظار في كل إطار (+10%)، وإصلاح اتصال IPv6 |

**تمّت التجربة على:** كرت شاشة **NVIDIA RTX 3060** مع معالج **Intel Core i5-12400F**. يُفترض أن تعمل كروت NVIDIA الأخرى بنفس الطريقة، أما كروت AMD وIntel فلم تُجرَّب. استخدم أحدث تعريف لكرت الشاشة.

## التحميل

**[حمّل الإصدار الرابع (Lichii-Duskbloods.zip)](https://github.com/Mr-Tb/lichii-dusk/releases/tag/duskbloods-v4)**

الإصدارات الأقدم: [كل الإصدارات](https://github.com/Mr-Tb/lichii-dusk/releases).

## التثبيت

تحتاج إلى:
- Windows 10/11 بنظام 64-bit وكرت شاشة يدعم Vulkan 1.3 (يُنصح بـ NVIDIA)
- ملفّي `prod.keys` و`title.keys` الخاصين بك
- نسخة firmware خاصة بك من Switch 2 (تمّت التجربة على 23.0.1)
- نسختك من *The Duskbloods Network Test*
- **[مشغّل playDB-CNT](https://github.com/sikoGD/playDB-CNT)** ‏(`Play.Duskbloods.exe`)، **يُحمَّل بشكل منفصل** من [صفحة إصداراته](https://github.com/sikoGD/playDB-CNT/releases/latest). طريقة إعداده في قسم [إعداد المشغّل](#إعداد-المشغّل) بالأسفل

الخطوات:

1. **فكّ الضغط** عن `Lichii-Duskbloods.zip` في مجلد بمسار قصير، مثل `C:\Games\Lichii-Duskbloods\`. أبقِ مجلد `user` بجانب `lichii.exe`: فهو يجعل المحاكي محمولًا ويحتوي على الإعدادات المحسّنة.
2. **المفاتيح:** انسخ `prod.keys` و`title.keys` إلى `user\keys\`، أو شغّل `lichii.exe` واختر **Tools > Install Decryption Keys**.
3. **الـ Firmware وأول تشغيل:** شغّل `lichii.exe`، ثم اختر **Tools > Install Firmware > From Folder** (أو **From ZIP**) وحدّد ملفات الـ firmware، ثم أغلق المحاكي. هذا التشغيل الأول يكتب أيضًا كل إعدادات المحاكي، والمشغّل يحتاج إليها.
4. **المشغّل:** [حمّل `Play.Duskbloods.exe`](https://github.com/sikoGD/playDB-CNT/releases/latest) وضعه بجانب `lichii.exe`، ثم جهّزه كما في قسم [إعداد المشغّل](#إعداد-المشغّل) بالأسفل.
5. **اللعب:** اضغط **Start game** في `Play.Duskbloods.exe`. شغّل اللعبة دائمًا من المشغّل، فإذا شغّلتها بأي طريقة أخرى لن تجد أي خوادم، وستتوقف عند رسالة تطلب الاتصال بالإنترنت.

**التحديث من الإصدار الثالث:** فكّ ضغط الإصدار الرابع في مجلد جديد، ثم انسخ مجلد `user` القديم فوقه (المفاتيح والـ firmware وملفات الحفظ). في الدقائق الأولى قد يحدث تقطيع أثناء إعادة بناء الـ shaders للإصدار الجديد.

## إعداد المشغّل

حمّل المشغّل `Play.Duskbloods.exe` من [صفحة إصدارات playDB-CNT](https://github.com/sikoGD/playDB-CNT/releases/latest) وضعه بجانب `lichii.exe` (وإذا حظره Windows Defender فانظر ملاحظة الإنذار الكاذب في الأعلى)، وكل إعداداته تتم من نافذته. هو يحفظ اختياراتك بنفسه في `launcher_settings.json` عند الضغط على **Start game**، فلا تعدّل هذا الملف يدويًا. لا تحتاج إلى صلاحيات المسؤول (admin)، ولا إلى تعديل ملف hosts أو تثبيت أي شهادات.

1. شغّل `Play.Duskbloods.exe`.
2. **تبويب Setup > Game:** اضغط **Folder** واختر مجلد اللعبة `THE DUSKBLOODS NETWORK TEST VER` (الذي يحتوي على `base\exefs\main`)، أو اضغط **Browse** لاختيار ملف `.nup`. يجب أن تظهر العبارة *The Duskbloods network test found*. إذا كان مجلد اللعبة داخل مجلد Lichii أو بجانبه، يجده المشغّل تلقائيًا.
3. **تبويب Setup > Emulator:** اختر `Lichii - <اسم المجلد>` (أي `lichii.exe` هذا، ويُختار تلقائيًا إذا كان المشغّل بجانبه). يجب ألّا تظهر الملاحظة الحمراء *No Duskbloods online support*. لا تضغط أي زر **Patch**، فهذه النسخة تدعم الاتصال أصلًا.
4. **تبويب Online:** اترك **Just me** (الخيار الافتراضي). أما **Host a room** و**Join a room** فهما لتجارب اللعب مع الأصدقاء، والمباريات فيهما لا تبدأ بعد.
5. **تبويب Mods** (في المشغّل 1.5.0 وما بعده): أبقِ **Bot Match** و**CPU player names** مفعّلين، وألغِ تفعيل الإضافات التجريبية في اللعب العادي.
6. اضغط **Start game**، وأبقِ المشغّل مفتوحًا أثناء اللعب.

وضع *Just me* لا يحتاج أي تغيير في جدار الحماية (Firewall)، فكل شيء يبقى على 127.0.0.1. لاستضافة غرفة أو الانضمام إليها، اسمح للمشغّل ولـ `lichii.exe` بالمرور عبر جدار حماية Windows عندما يطلب Windows ذلك.

## NVIDIA DLSS (اختياري)

لكروت NVIDIA RTX فقط. بدونه يستخدم المحاكي AMD FSR 3.1، وهو يعمل على كل كروت الشاشة.

1. **حمّل ملفات NVIDIA** (غير موجودة في الملف المضغوط): افتح **Emulation > Configure > Graphics**، فعّل **NVIDIA DLSS** واضغط **Download from NVIDIA** في اللوحة الخاصة به. أو انسخ بنفسك الملفات `sl.interposer.dll` و`sl.common.dll` و`sl.dlss.dll` و`nvngx_dlss.dll` من NVIDIA Streamline SDK 2.14.16 إلى المجلد `user\streamline\`.
2. **فعّله:** ضع علامة على **NVIDIA DLSS** وأعد تشغيل المحاكي مرة واحدة. في المرة الأولى يبدأ DLSS مع التشغيل التالي.
3. **بدّل أثناء اللعب:** اضغط زر الـ upscaler في شريط الحالة أسفل النافذة (أو **F7**) للتنقّل بين FSR3 وأوضاع DLSS: Auto وQuality وBalanced وPerformance وUltra Performance. اضغط بالزر الأيمن لاختيار وضع معيّن. ننصح بـ **Quality**، وحدّد وضوح الصورة من إعداد الدقة (راجع الأعلى).

إذا لم يستطع DLSS العمل، يعود المحاكي تلقائيًا إلى FSR 3.1.

## الإعدادات المقترحة (مضبوطة مسبقًا في الملف المضغوط)

| الإعداد | القيمة | السبب |
|---|---|---|
| Graphics > Resolution | **1x** | يعمل على أكبر عدد من الأجهزة، وارفعه حسب الجدول أعلاه |
| Graphics > Anti-aliasing | **None** | FSR 3.1 أو DLSS يتكفّلان بذلك |
| Graphics > Advanced > GPU Mode | **Fast** | توفير كبير في استهلاك المعالج |
| Extras > Asynchronous shader compilation | **On** | تقطيع أقل |
| Graphics > VSync | **Mailbox** | العرض لا يعطّل الرسم |
| Async presentation | **Off** | أسرع في هذه اللعبة |
| Window adapting filter | Bilinear | الأخفّ |
| NVIDIA DLSS | Off | فعّله إذا كان لديك كرت RTX (راجع الأعلى) |
| Error applet | HLE | نسخة LLE تُسقط المحاكي |
| Docked mode | On | |

## مشاكل معروفة

- قد يظهر وميض في الإضاءة أو ظهور متأخر للصور عندما تقلّ ذاكرة الجهاز (الرام مع ملف الصفحات). أغلق المتصفحات والبرامج الثقيلة قبل اللعب.
- أول تشغيل فيه تقطيع أثناء إعادة بناء الـ shaders للإصدار الجديد.
- كروت AMD وIntel لم تُجرَّب.
- قد يصنّف Windows Defender المشغّل كتهديد بالخطأ. هذا إنذار كاذب: انظر التحذير في أعلى هذا الدليل.
- DLSS يحتاج كرت RTX وتحميل ملفات NVIDIA مرة واحدة.

## حل المشاكل

- **التوقف عند قائمة البداية، أو تقول اللعبة إنها تحتاج اتصالًا بالإنترنت:** تحقّق من هذه النقاط بالترتيب.
  1. شغّل اللعبة بزر **Start game** في `Play.Duskbloods.exe`. إذا شغّلتها من `lichii.exe` أو من قائمة ألعابه أو من اختصار خاص بك، فلن تجد اللعبة أي خوادم.
  2. أول تشغيل بعد فكّ الضغط مباشرة؟ أغلق اللعبة واضغط **Start game** مرة أخرى. إذا استمرت المشكلة، شغّل `lichii.exe` وحده مرة واحدة ثم أغلقه، ثم استخدم المشغّل.
  3. في `lichii.exe` افتح **Emulation > Configure > Network** وأوقف خيار "Redirect host names to a server you run" ‏(Host redirect). عندما يكون مفعّلًا، يحلّ محلّ خوادم المشغّل.
  4. في تبويب **Online** في المشغّل، اختر **Just me**.
  5. ابحث عن أسطر حمراء في سجلّ المشغّل: مفاتيح ناقصة، أو *Port 18443 is still in use*، أو *No Duskbloods online support*.
  6. عدّلت `launcher_settings.json` يدويًا؟ احذفه (هو بجانب `Play.Duskbloods.exe`) واختر اللعبة والمحاكي من جديد في النافذة، فالمشغّل يتجاهل الملف التالف دون أي تنبيه.
  7. ما زالت المشكلة قائمة؟ اضغط **Open logs** في المشغّل وأرسل أحدث مجلد داخل `user\duskbloods_test\runs\` (`emulator.log` و`server.log` و`frontdoor.log`).
- **الخطأ 2321-4992 عند شاشة البداية:** المشغّل يشغّل نسخة Lichii أقدم أو محاكيًا آخر. اختر `lichii.exe` الخاص بهذه النسخة في تبويب Setup.
- **التوقف عند "checking save data" أو أخطاء الشبكة:** شغّل اللعبة من `Play.Duskbloods.exe` وليس من `lichii.exe` مباشرة.
- **الخطأ 2153-0513 عند التشغيل:** ملف الحفظ تلف، مثلًا بسبب إغلاق اللعبة بالقوة أثناء الحفظ. انقل مجلد حفظ اللعبة داخل `user\nand\user\save\` إلى مكان آخر وأعد التشغيل.
- **شاشة سوداء بعد التحميل:** أغلق اللعبة وأعد تشغيلها (حدث تعليق نادر مرة واحدة).
- **شاشة سوداء أول تشغيل بعد التحديث:** احذف ذاكرة الـ shaders في `user\cache\shader\`.
- **وميض أو أشكال مشوّهة:** احذف السطر `EDEN_NX2_FRAMES_AHEAD=1` من الملف `nx2_switches.txt` ثم أعد تشغيل المحاكي.
- **الإضاءة تبدو خاطئة (سوداء أو تومض أو مفقودة):** احذف علامة `#` قبل السطر `EDEN_NX2_DUSK_LOADED_REBASE=0` في الملف `nx2_switches.txt` (طريقة تحميل الأضواء القديمة الأبطأ)، وأخبرنا بالمشكلة.
- **لا يظهر خيار أو زر DLSS:** يحتاج كرت RTX والملفات الأربعة من NVIDIA داخل `user\streamline\`.
- **رسائل عن ملفات DLL مفقودة:** فكّ الضغط عن الملف كاملًا، ولا تشغّل `lichii.exe` من داخل الملف المضغوط.
- **للإبلاغ عن مشكلة:** أغلق اللعبة، ثم أرسل الملف `user\log\eden_log.txt` (تأكد أولًا أنه لا يحتوي على شيء خاص).

## البناء من الكود المصدري

الفرع [`dusk-v4`](https://github.com/Mr-Tb/lichii-dusk/tree/dusk-v4) (الإصدار الثالث: [`dusk-v3r`](https://github.com/Mr-Tb/lichii-dusk/tree/dusk-v3r)، الإصدار الثاني: [`dusk-v2`](https://github.com/Mr-Tb/lichii-dusk/tree/dusk-v2)، الإصدار الأول: [`dusk-frizti`](https://github.com/Mr-Tb/lichii-dusk/tree/dusk-frizti)). يُبنى باستخدام MSYS2 UCRT64 + clang مثل Lichii الأصلي تمامًا.

## الشكر والترخيص

مبني على [zortos293/Lichii](https://github.com/zortos293/Lichii) و[Frizti/Lichii](https://github.com/Frizti/Lichii). المشغّل وخوادم الشبكة من [playDB-CNT](https://github.com/sikoGD/playDB-CNT) لـ sikoGD. الترخيص: GPL-3.0-or-later (راجع [LICENSE.txt](LICENSE.txt)). يعمل NVIDIA DLSS عبر NVIDIA Streamline (ترخيص MIT)، وملفات NVIDIA يحمّلها المستخدم بنفسه ولا تُوزَّع مع المحاكي.

شكر خاص لـ **Siko** ‏([sikoGD](https://github.com/sikoGD))، صاحب المشغّل [playDB-CNT](https://github.com/sikoGD/playDB-CNT). هذا الإصدار مدين كثيرًا لنسخته v30 من Duskbloods ولتحديثيها v31 وv32. أخذنا من عمله طريقة تمرير مدخلات اللعبة الحقيقية إلى أداة رفع الدقة الزمنية (temporal upscaler): مقياس متجهات الحركة واصطلاحات الـ jitter، وتتبّع أهداف الرسم الخاصة بالعمق والسرعة في اللعبة، والاحتفاظ بسجلّ أداة رفع الدقة عندما يتغيّر حجم منطقة الرسم. هذا هو أساس صورة FSR 3.1 الحادّة. كما أرشدتنا نسخته v32 إلى طريق إصلاح تجمّد الحفظ (2153-0002)، ونقلنا منها تسجيل فترات التوقّف.

يستخدم الإصدار الرابع أيضًا أفكارًا من [Fritchii](https://github.com/Frizti/Fritchii) (حدّ إعادة الكتابة مع الانتظار، وملخّص الجلسة، وحماية خيار Force maximum clocks)، ومن طلب دمج مفتوح في المشروع الأصلي، [zortos293/Lichii#559](https://github.com/zortos293/Lichii/pull/559) (قراءة نتائج الاستعلامات مع انتظار نتائجها فقط). شكرًا لـ JalenJaceX2 على طلب الدمج رقم 2.

</div>

</details>
