# Lean Beeftext icon asset manifest

The Lean Beeftext icon artwork is frozen. Generated assets are deterministic derivatives of the three approved source PNGs: resizing, transparent padding, and grayscale conversion only. No generated asset changes, redraws, or reinterprets the mascot.

## Approved sources

| File | Canvas | SHA-256 | Purpose |
| --- | --- | --- | --- |
| `Sources/LeanBeeftextMaster.png` | 1254×1254 RGBA | `4ce2f5fe7090dcc0bf31a2068f1a8b7302a7cec95feec90293c438a5932831c2` | Full framed application artwork for 20px and larger. |
| `Sources/LeanBeeftextTray32Source.png` | 1536×1024 RGBA | `0a6036a6d053ea531ef3cdc63cbfcaa7c7a06686b28602a19f955e13df69e3ac` | Bull-only tray artwork for the 32×32 tray asset. |
| `Sources/LeanBeeftextTray16Source.png` | 1254×1254 RGBA | `9c2666333ccd7e50339beb08bcde5ceb0f7423b1866e313c8c4c7255b6b4caef` | Dedicated optical bull-only artwork used for every true 16×16 frame. |

## Deterministic transforms

`Scripts/GenerateLeanIconAssets.sh` generates the bundle with ImageMagick. Each output is rendered directly from its designated approved source with the Lanczos filter, metadata stripped, an explicit RGBA PNG color type, and deterministic PNG compression settings.

- Full application PNGs resize the square master canvas directly to 20, 24, 32, 48, 64, 128, and 256 pixels.
- The 32px tray source is trimmed to its nontransparent artwork, proportionally fitted into a 32×32 transparent canvas, and centered without cropping or distortion.
- The 16px tray PNG resizes the dedicated square optical source directly. It is not derived from either the full framed icon or the 32px tray source.
- Paused assets convert the corresponding sized color PNG to grayscale while retaining its alpha channel. No red or error accent is added.

The script was validated with ImageMagick 7.1.1-43 Q16. `SHA256SUMS.txt` records the approved sources and every generated PNG and ICO.

## Output and resource mapping

| Application surface | Asset |
| --- | --- |
| Windows executable and Windows shell | `LeanBeeftextApp.ico` |
| Qt application/window icon | `LeanBeeftextApp.ico` |
| Enabled system tray icon | `LeanBeeftextTray.ico` |
| Paused system tray icon | `LeanBeeftextTrayPaused.ico` |
| About dialog | `App/LeanBeeftextApp-128.png` |
| Picker window | `App/LeanBeeftextApp-32.png` |

`LeanBeeftextApp.ico` and `LeanBeeftextAppPaused.ico` contain 16, 20, 24, 32, 48, 64, 128, and 256px frames. Their true 16×16 frame deliberately uses the dedicated bull-only optical artwork; all frames from 20×20 upward use the full framed master. Tray ICOs contain only the dedicated optical 16×16 and simplified 32×32 bull frames.
