import io
import os
import sys
import tempfile
import types
import zipfile
from pathlib import Path
from urllib.request import urlretrieve

import cv2
import numpy as np
import torch
from fastapi import FastAPI, HTTPException, Query, Request
from fastapi.responses import Response, JSONResponse
from PIL import Image

# basicsr expects the older torchvision module path. Newer torchvision builds
# moved it under _functional_tensor, so alias it before importing basicsr.
try:
    import torchvision.transforms.functional_tensor as _tv_functional_tensor
except ModuleNotFoundError:
    try:
        import torchvision.transforms._functional_tensor as _tv_functional_tensor
        sys.modules["torchvision.transforms.functional_tensor"] = _tv_functional_tensor
    except ModuleNotFoundError:
        import torchvision.transforms.functional as _tv_functional

        _tv_functional_tensor = types.ModuleType("torchvision.transforms.functional_tensor")
        _tv_functional_tensor.rgb_to_grayscale = _tv_functional.rgb_to_grayscale
        sys.modules["torchvision.transforms.functional_tensor"] = _tv_functional_tensor

from basicsr.archs.rrdbnet_arch import RRDBNet
from realesrgan import RealESRGANer


MODEL_URLS = {
    "RealESRGAN_x4plus": "https://github.com/xinntao/Real-ESRGAN/releases/download/v0.1.0/RealESRGAN_x4plus.pth",
}

SUPPORTED_METHODS = {"realesrgan"}
SUPPORTED_SCALES = {2, 4}

app = FastAPI(title="PNG Upscale Service", version="0.1.0")
_UPSAMPLER = None
_UPSAMPLER_MODEL = None


def model_cache_dir() -> Path:
    path = Path(os.environ.get("MODEL_CACHE_DIR", "/models"))
    path.mkdir(parents=True, exist_ok=True)
    return path


def model_name() -> str:
    return os.environ.get("REALESRGAN_MODEL", "RealESRGAN_x4plus")


def ensure_model_weights() -> Path:
    name = model_name()
    if name not in MODEL_URLS:
        raise RuntimeError(f"Unsupported model '{name}'")

    weights = model_cache_dir() / f"{name}.pth"
    if not weights.exists():
        urlretrieve(MODEL_URLS[name], weights)
    return weights


def build_upsampler() -> RealESRGANer:
    name = model_name()
    if name != "RealESRGAN_x4plus":
        raise RuntimeError(f"Unsupported model '{name}'")

    weights = ensure_model_weights()
    tile = int(os.environ.get("REALESRGAN_TILE", "0"))
    half = torch.cuda.is_available() and os.environ.get("REALESRGAN_HALF", "1") != "0"
    model = RRDBNet(num_in_ch=3, num_out_ch=3, num_feat=64,
                    num_block=23, num_grow_ch=32, scale=4)
    return RealESRGANer(
        scale=4,
        model_path=str(weights),
        model=model,
        tile=tile,
        tile_pad=10,
        pre_pad=0,
        half=half,
        gpu_id=0 if torch.cuda.is_available() else None,
    )


def get_upsampler() -> RealESRGANer:
    global _UPSAMPLER, _UPSAMPLER_MODEL
    current_model = model_name()
    if _UPSAMPLER is None or _UPSAMPLER_MODEL != current_model:
        _UPSAMPLER = build_upsampler()
        _UPSAMPLER_MODEL = current_model
    return _UPSAMPLER


def upscale_png_bytes(payload: bytes, scale: int, method: str) -> bytes:
    if method not in SUPPORTED_METHODS:
        raise HTTPException(status_code=400, detail=f"Unsupported method '{method}'")
    if scale not in SUPPORTED_SCALES:
        raise HTTPException(status_code=400, detail="Scale must be 2 or 4")

    image = Image.open(io.BytesIO(payload)).convert("RGBA")
    bgra = cv2.cvtColor(np.array(image), cv2.COLOR_RGBA2BGRA)
    upsampler = get_upsampler()
    enhanced, _ = upsampler.enhance(bgra, outscale=scale)
    rgba = cv2.cvtColor(enhanced, cv2.COLOR_BGRA2RGBA)

    out = io.BytesIO()
    Image.fromarray(rgba).save(out, format="PNG")
    return out.getvalue()


def upscale_archive_bytes(payload: bytes, scale: int, method: str) -> bytes:
    if method not in SUPPORTED_METHODS:
        raise HTTPException(status_code=400, detail=f"Unsupported method '{method}'")
    if scale not in SUPPORTED_SCALES:
        raise HTTPException(status_code=400, detail="Scale must be 2 or 4")

    with tempfile.TemporaryDirectory() as tmp:
        in_dir = Path(tmp) / "in"
        out_dir = Path(tmp) / "out"
        in_dir.mkdir(parents=True, exist_ok=True)
        out_dir.mkdir(parents=True, exist_ok=True)

        with zipfile.ZipFile(io.BytesIO(payload), "r") as zf:
            zf.extractall(in_dir)

        for src in in_dir.rglob("*.png"):
            rel = src.relative_to(in_dir)
            dst = out_dir / rel
            dst.parent.mkdir(parents=True, exist_ok=True)
            dst.write_bytes(upscale_png_bytes(src.read_bytes(), scale, method))

        out = io.BytesIO()
        with zipfile.ZipFile(out, "w", compression=zipfile.ZIP_DEFLATED) as zf:
            for src in out_dir.rglob("*.png"):
                zf.write(src, src.relative_to(out_dir))
        return out.getvalue()


@app.get("/health")
def health() -> JSONResponse:
    return JSONResponse(
        {
            "ok": True,
            "cuda": torch.cuda.is_available(),
            "device": torch.cuda.get_device_name(0) if torch.cuda.is_available() else "cpu",
            "model": model_name(),
            "methods": sorted(SUPPORTED_METHODS),
            "scales": sorted(SUPPORTED_SCALES),
        }
    )


@app.post("/upscale/image")
async def upscale_image(
    request: Request,
    method: str = Query("realesrgan"),
    scale: int = Query(2),
) -> Response:
    payload = await request.body()
    if not payload:
        raise HTTPException(status_code=400, detail="Request body is empty")
    return Response(upscale_png_bytes(payload, scale, method), media_type="image/png")


@app.post("/upscale/archive")
async def upscale_archive(
    request: Request,
    method: str = Query("realesrgan"),
    scale: int = Query(2),
) -> Response:
    payload = await request.body()
    if not payload:
        raise HTTPException(status_code=400, detail="Request body is empty")
    return Response(upscale_archive_bytes(payload, scale, method), media_type="application/zip")