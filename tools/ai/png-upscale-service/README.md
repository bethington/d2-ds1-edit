# PNG Upscale Service

Generic PNG upscaling service intended for DS1Edit and other local-network tools.

## Features

- Generic API, not DS1Edit-specific
- `POST /upscale/archive?method=<method>&scale=2|4` with `ZIP in / ZIP out`
- `POST /upscale/image?method=<method>&scale=2|4` with `PNG in / PNG out`
- Method support (all 4x ESRGAN/RRDBNet models, downscaled by the client when scale=2):
  - `realesrgan` — Real-ESRGAN x4 plus (default; photo-trained)
  - `ultrasharp` — 4x-UltraSharp (community photo/general purpose)
  - `nmkd-superscale` — 4x_NMKD-Superscale-SP (community general purpose, sharper detail)
  - `anime-6b` — RealESRGAN x4 plus anime 6B (smaller arch tuned for line art)
- GPU-ready via Docker Compose on NVIDIA hosts
- Per-method weight cache: each model is downloaded on first use and kept resident

## Quick Start

```bash
cp .env.example .env
docker compose up -d --build
curl http://localhost:8084/health
```

## Environment

- `PNG_UPSCALE_PORT=8084`
- `REALESRGAN_TILE=0` — tile size for memory-bounded GPUs (0 = no tiling)
- `REALESRGAN_HALF=1` — fp16 inference when CUDA is available
- `MODEL_CACHE_DIR=/models` — where downloaded weights are stored (volume-mounted)

## DS1Edit Integration

Set these in `Ds1edit.ini`:

```ini
upscale_enabled     = YES
upscale_service_url = http://10.0.10.30:8084
```

When configured, export actions can offer `None`, `2x`, or `4x`. DS1Edit exports native PNGs to a temporary staging area, sends them to this service as a ZIP archive, and writes the returned ZIP contents to the selected output folder.

## API Examples

Single image:

```bash
curl -X POST "http://localhost:8084/upscale/image?method=realesrgan&scale=2" \
  -H "Content-Type: image/png" \
  --data-binary @input.png \
  -o output.png
```

Batch ZIP:

```bash
curl -X POST "http://localhost:8084/upscale/archive?method=realesrgan&scale=4" \
  -H "Content-Type: application/zip" \
  --data-binary @input.zip \
  -o output.zip
```

## Notes

- The service expects PNG input only.
- `scale=2` and `scale=4` are supported. All registered models are 4x; for
  `scale=2` the upsampler downscales the 4x output to match.
- The first request for each method downloads its weights into `./models`.
  Subsequent requests reuse the cached `.pth`.
- `GET /health` reports the available methods, scales, and which methods
  have been loaded into GPU memory in this process.