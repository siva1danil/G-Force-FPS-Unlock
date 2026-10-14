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

   [ 1] Switch to Borderless Fullscreen @ desktop FPS (recommended)
   [ 2] Switch to Fullscreen @ 59 FPS
   [ 3] Switch to Fullscreen @ 60 FPS
   [ 4] Switch to Fullscreen @ 120 FPS
   [ 5] Switch to Fullscreen @ 144 FPS
   [ 6] Switch to Fullscreen @ 165 FPS
   [ 0] Exit

   > 1

   Apply settings now?

   [ 1] Yes (recommended)
   [ 0] No

   > 1

   Run FPS/TOPMOST watch to prevent game changing the settings?

   [ 1] Yes
   [ 0] No (recommended unless there are problems)

   > 0

   Switched from fullscreen@60 to borderless@165.
   Watch disabled - consider using it if FPS/TOPMOST gets changed during gameplay.
   Video mode applied automatically; no resolution change needed.
   ```

## Download

Download `g-force-fps-unlock.exe` from the [latest release](https://github.com/siva1danil/G-Force-FPS-Unlock/releases/latest).

## Compilation

From the repository root, using MinGW GCC:

```powershell
gcc src\main.c -o g-force-fps-unlock.exe
```
