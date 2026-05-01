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

The generic Docker service lives in [tools/ai/png-upscale-service](../../tools/ai/png-upscale-service).

It is intentionally generic so other local-network projects can reuse it. The API is PNG-based and does not know anything about DT1, DC6, DCC, or DS1Edit-specific asset formats.