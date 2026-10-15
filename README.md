# G-Force (2009) FPS Unlocker

Unlock FPS and switch between borderless and fullscreen. Only the running game is patched; game files stay unchanged. Supported `GForce.exe` SHA-256: `637130107CAD738CC9F52A0F688B2A3BF7CA981B22616B4A005CA4EEBD59B012`.

## Usage

1. Start G-Force.

2. Press **Win+X** and select **Terminal (Administrator)**.

3. Go to the game directory (use your own path if different):

   ```powershell
   cd "D:\Games\SteamLibrary\steamapps\common\G-Force"
   ```

4. Copy `g-force-fps-unlock.exe` into that directory, next to `GForce.exe`.

5. Run:

   ```powershell
   .\g-force-fps-unlock.exe
   ```

6. Choose a mode and follow the prompts. For example (available FPS values depend on your display):

   ```text
   G-Force FPS Unlock

   GForce.exe PID=21540
   Mode: patched fullscreen (800 x 600 @ 59 FPS, 59 Hz)

   [ 1] Borderless Fullscreen @ native FPS (recommended)
   [ 2] Fullscreen @ 56 FPS
   [ 3] Fullscreen @ 60 FPS
   [ 4] Fullscreen @ 72 FPS
   [ 5] Fullscreen @ 75 FPS
   [ 6] Fullscreen @ 100 FPS
   [ 7] Fullscreen @ 119 FPS
   [ 8] Fullscreen @ 120 FPS
   [ 9] Fullscreen @ 144 FPS
   [10] Fullscreen @ 165 FPS
   [ 0] Exit

   > 1

   Apply settings now?

   [ 1] Yes (recommended)
   [ 0] No

   > 1
   Game resolution differs from desktop; the image may be scaled.

   Switched from fullscreen@59 to borderless@165.
   Game reported a successful video reset.
   ```

## Download

Download `g-force-fps-unlock.exe` from the [latest release](https://github.com/siva1danil/G-Force-FPS-Unlock/releases/latest).

## Compilation

From the repository root, using MinGW GCC:

```powershell
gcc src\main.c -o g-force-fps-unlock.exe
```
