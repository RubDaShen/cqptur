# cqptur

Screenshots at your screen's real resolution, even with Windows display scaling turned on.

## Why cqptur

- **No shrunken screenshots.** With display scaling on (125%, 150%, ...), many screenshot
  tools save a smaller image than your screen: at 150%, a 2560 × 1600 screen comes out around
  1707 × 1067. cqptur keeps every pixel, on every monitor, even when your monitors use
  different scaling.
- **Light.** A single file of about 470 KB with no installer. While it waits in the
  background it uses only a few megabytes of memory.
- **Private.** No internet access and no third-party code.

## Getting cqptur

cqptur runs on Windows 10 (version 1607 or later) and Windows 11, 64-bit.

For now, you build it from this repository:

1. Install [Visual Studio](https://visualstudio.microsoft.com/) 2022 or newer (the free
   Community edition works) with the **Desktop development with C++** workload.
2. Download this repository (**Code → Download ZIP**) and unzip it, or clone it.
3. Double-click `build.cmd`. When it finishes, `cqptur.exe` is in the `build\release` folder.
4. Move `cqptur.exe` somewhere permanent, for example `%LOCALAPPDATA%\Programs\cqptur\`.

## First-time setup

1. **Run `cqptur.exe`.** A notification confirms that cqptur is running, and its icon appears
   in the notification area next to the clock.
   Windows 11 may tuck new icons behind the **^** arrow. To keep cqptur's icon visible, go to
   *Settings → Personalization → Taskbar → Other system tray icons* and turn on cqptur.
2. **Optional:** right-click the cqptur icon and choose **Start with Windows**, so cqptur is
   ready every time you sign in.

## Taking a screenshot

Start a capture in any of these ways:

- Press **Ctrl + Print Screen**. You can switch to **Shift + Print Screen** or
  **Ctrl + Shift + Print Screen** from the [tray menu](#the-tray-menu).
- Click the cqptur icon.
- Run `cqptur.exe` again, for example from a Start menu or taskbar shortcut.

Print Screen on its own stays with Windows: Windows 11 doesn't let apps running in the
background take that key over. It keeps doing whatever Windows does with it on your PC
(opening Snipping Tool, or copying the screen to the clipboard).

The screen freezes and dims. Then:

| Do this | To get |
|---|---|
| Drag across an area | That area. The size shown while you drag is the real size in pixels. |
| Click once | The whole monitor you clicked on |
| Press Esc, or right-click | Nothing: the capture is cancelled |

## Where your screenshots go

- **The clipboard.** Paste the screenshot anywhere with **Ctrl + V**.
- **Your Screenshots folder** (`Pictures\Screenshots`), as a PNG named
  `Screenshot YYYY-MM-DD HHMMSS.png`.

A notification shows the screenshot's size. Click it to open the image.

## The tray menu

Right-click the cqptur icon:

| Item | What it does |
|---|---|
| **Capture** | Takes a screenshot. Shows the shortcut next to it while that shortcut belongs to cqptur. |
| **Shortcut** | Chooses the capture shortcut: *Ctrl+Print Screen* (the default), *Shift+Print Screen* or *Ctrl+Shift+Print Screen*. cqptur remembers your choice. |
| **Start with Windows** | Starts cqptur automatically when you sign in. |
| **Exit** | Closes cqptur. |

## Troubleshooting

- **Print Screen opens Snipping Tool (or does nothing).** cqptur's shortcut is
  **Ctrl + Print Screen** (or the one you chose); Print Screen alone belongs to Windows.
- **The shortcut does nothing.** Another app (another screenshot tool, for example) may be
  using it. If so, cqptur's "cqptur is running" notification says so and its menu shows
  **Capture** without a shortcut. Choose a different one under **Shortcut**, or close that
  app and then right-click the cqptur icon so it picks the shortcut up again.
- **I can't find the cqptur icon.** Click the **^** arrow next to the clock, or make the icon
  always visible as described in step 1 of [First-time setup](#first-time-setup).
- **cqptur stopped starting with Windows after I moved it.** Right-click the icon and choose
  **Start with Windows** again, so Windows knows the new location.

## Removing cqptur

1. Right-click the cqptur icon. If **Start with Windows** is checked, click it to turn it off.
2. If you changed the shortcut, set **Shortcut** back to **Ctrl+Print Screen**. cqptur only
   stores a shortcut that isn't the default, so this clears its last setting.
3. Choose **Exit**.
4. Delete `cqptur.exe`.

cqptur keeps no other files or settings. The screenshots you took stay in your Screenshots folder.

## License

[MIT](LICENSE)
