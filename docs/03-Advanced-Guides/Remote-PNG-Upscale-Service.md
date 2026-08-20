# Remote PNG Upscale Service

DS1Edit can optionally use a remote PNG upscaling service for export actions.

## DS1Edit Configuration

Add these keys to `Ds1edit.ini`:

```ini
upscale_enabled     = YES
upscale_service_url = http://10.0.10.30:8084
```

When configured, the following export actions can prompt for `None`, `2x`, or `4x`:

- Export area assets
- Export folder assets
- Export folder assets by type
- Export all assets

If `2x` or `4x` is selected, DS1Edit:

1. exports native PNGs to a temporary staging directory
2. sends a ZIP archive of those PNGs to the configured service
3. writes only the upscaled PNGs to the selected output folder
4. asks before overwrite if the target folder is not empty
5. asks before falling back to the built-in local upscaler if the remote call fails

## Service Source

**The reference service is no longer part of this repository.** It was removed as
scaffolding rather than something users were expected to run.

The editor still supports the remote methods (`--upscale-method=realesrgan`,
`ultrasharp`, `nmkd-superscale`, `anime-6b`, `apisr`, `sd-x4`, `sdxl-refine`), so
you need to point `upscale_service_url` at a service of your own that implements
the contract described above: it takes a PNG, returns an upscaled PNG, and knows
nothing about DT1, DC6, DCC or any DS1Edit-specific format.

If you only want upscaling without running anything, the local methods need no
service at all: `nn`, `scale2x`, `xbrz`, `bilinear`, `bicubic`, `lanczos`.