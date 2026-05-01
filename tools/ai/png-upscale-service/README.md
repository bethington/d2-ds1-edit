# PNG Upscale Service

Generic PNG upscaling service intended for DS1Edit and other local-network tools.

## Features

- Generic API, not DS1Edit-specific
- `POST /upscale/archive?method=realesrgan&scale=2|4` with `ZIP in / ZIP out`
- `POST /upscale/image?method=realesrgan&scale=2|4` with `PNG in / PNG out`
- Initial method support: `realesrgan`
- GPU-ready via Docker Compose on NVIDIA hosts

## Quick Start

```bash
cp .env.example .env
docker compose up -d --build
curl http://localhost:8084/health
```

## Environment

- `PNG_UPSCALE_PORT=8084`
- `REALESRGAN_MODEL=RealESRGAN_x4plus`
- `REALESRGAN_TILE=0`
- `REALESRGAN_HALF=1`
- `MODEL_CACHE_DIR=/models`

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
- `scale=2` and `scale=4` are supported.
- The first request downloads the Real-ESRGAN model weights into `./models`.