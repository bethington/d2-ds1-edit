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


# Method registry. Each method maps to a model file + RRDBNet arch params.
# All entries below are 4x ESRGAN-architecture models that load via
# basicsr's RRDBNet. The "anime-6b" variant uses num_block=6 instead of
# the default 23, which is required to match its weight shapes.
METHODS = {
    "realesrgan": {
        "model_name": "RealESRGAN_x4plus",
        "url": "https://github.com/xinntao/Real-ESRGAN/releases/download/v0.1.0/RealESRGAN_x4plus.pth",
        "num_block": 23,
        "scale": 4,
    },
    "ultrasharp": {
        "model_name": "4x-UltraSharp",
        "url": "https://huggingface.co/lokCX/4x-Ultrasharp/resolve/main/4x-UltraSharp.pth",
        "num_block": 23,
        "scale": 4,
    },
    "nmkd-superscale": {
        "model_name": "4x_NMKD-Superscale-SP_178000_G",
        "url": "https://huggingface.co/uwg/upscaler/resolve/main/ESRGAN/4x_NMKD-Superscale-SP_178000_G.pth",
        "num_block": 23,
        "scale": 4,
    },
    "anime-6b": {
        "model_name": "RealESRGAN_x4plus_anime_6B",
        "url": "https://github.com/xinntao/Real-ESRGAN/releases/download/v0.2.2.4/RealESRGAN_x4plus_anime_6B.pth",
        "num_block": 6,
        "scale": 4,
    },
    "apisr": {
        "model_name": "4x_APISR_RRDB_GAN_generator",
        "url": "https://huggingface.co/HikariDawn/APISR/resolve/main/4x_APISR_RRDB_GAN_generator.pth",
        "num_block": 6,
        "scale": 4,
    },
}

SUPPORTED_METHODS = (
    set(METHODS.keys())
    | {"sd-x4", "sdxl-refine"}
    | {"bilinear", "bicubic", "lanczos", "xbrz"}
)
SUPPORTED_SCALES = {2, 4}

# Default text prompt for the SD x4 upscaler. Diffusion-based upscalers
# need a prompt to bias the synthesized detail. This default targets
# fantasy game character art; callers may override per-request via the
# `prompt` query string.
SD_DEFAULT_PROMPT = (
    "fantasy game character, intricate detail, sharp lines, "
    "clean rendering, detailed clothing and armor"
)

# How many diffusion denoising steps. More = slower + (sometimes) more
# detail. 20 is the diffusers default and gives a reasonable detail/time
# tradeoff on the 3090.
SD_NUM_STEPS = 20

# SDXL img2img refine prompt + strength. Low strength means the model
# only refines textures over the NN-upscaled init image -- it doesn't
# redraw content, so the original D2 silhouette and colour palette are
# preserved while plausible painted-illustration detail is added.
SDXL_DEFAULT_PROMPT = (
    "diablo 2 game sprite, dark fantasy painted illustration, "
    "gothic, hand-painted, detailed, sharp"
)
SDXL_NEGATIVE_PROMPT = (
    "blurry, low quality, photographic, modern, cartoon, anime, "
    "3d render, plastic"
)
SDXL_STRENGTH = 0.35
SDXL_NUM_STEPS = 30

app = FastAPI(title="PNG Upscale Service", version="0.4.0")
_UPSAMPLER_CACHE: dict = {}
_SD_PIPELINE = None
_SDXL_PIPELINE = None


def model_cache_dir() -> Path:
    path = Path(os.environ.get("MODEL_CACHE_DIR", "/models"))
    path.mkdir(parents=True, exist_ok=True)
    return path


def ensure_model_weights(method: str) -> Path:
    spec = METHODS[method]
    weights = model_cache_dir() / f"{spec['model_name']}.pth"
    if not weights.exists():
        urlretrieve(spec["url"], weights)
    _normalize_checkpoint(weights)
    return weights


def _normalize_checkpoint(path: Path) -> None:
    """Make the .pth at `path` loadable by basicsr's RRDBNet with strict=True.

    Two transforms, applied once and cached via a sibling .normalized marker:

      1. Wrap a bare state dict under {'params_ema': ...}. RealESRGANer
         expects either 'params_ema' or 'params' as the top-level key.
      2. Translate old-arch ESRGAN key names (model.0.weight,
         model.1.sub.N.RDB.X.conv.Y.0.weight, ...) into basicsr's RRDBNet
         names (conv_first.weight, body.N.rdbX.convY.weight, ...). 4x-
         UltraSharp and 4x_NMKD-Superscale-SP both ship in the old layout.
    """
    marker = path.with_suffix(path.suffix + ".normalized")
    if marker.exists():
        return
    try:
        loaded = torch.load(str(path), map_location="cpu", weights_only=True)
    except Exception:
        loaded = torch.load(str(path), map_location="cpu")
    if not isinstance(loaded, dict):
        return

    inner_key = None
    if "params_ema" in loaded:
        inner_key = "params_ema"
    elif "params" in loaded:
        inner_key = "params"
    elif "model_state_dict" in loaded:
        # APISR checkpoints (4x_APISR_RRDB_GAN_generator.pth) wrap the
        # state dict under this key. Unwrap and rewrap as params_ema.
        inner_key = "model_state_dict"
    inner = loaded[inner_key] if inner_key is not None else loaded

    converted = _convert_old_esrgan_keys(inner)
    if converted is not None:
        inner = converted

    torch.save({"params_ema": inner}, str(path))
    marker.touch()


def _convert_old_esrgan_keys(state_dict):
    """Return a new state dict with old-arch ESRGAN keys mapped to basicsr's
    RRDBNet keys, or None if state_dict is already in the new layout.

    Old layout (ESRGAN):                New layout (basicsr RRDBNet):
      model.0.weight                    conv_first.weight
      model.1.sub.{N}.RDB{X}.conv{Y}.0  body.{N}.rdb{X}.conv{Y}
      model.1.sub.23                    conv_body
      model.3                           conv_up1
      model.6                           conv_up2
      model.8                           conv_hr
      model.10                          conv_last
    """
    import re
    if not any(k.startswith("model.") for k in state_dict.keys()):
        return None
    # 23 blocks (sub.0..sub.22), then sub.23 is conv_body. The N=23
    # prefix and the explicit fixed offsets (3/6/8/10) match the standard
    # ESRGAN RRDB_PSNR generator topology.
    rdb_re = re.compile(
        r"^model\.1\.sub\.(\d+)\.RDB(\d+)\.conv(\d+)\.0\.(weight|bias)$"
    )
    fixed = {
        "model.0.weight": "conv_first.weight",
        "model.0.bias": "conv_first.bias",
        "model.1.sub.23.weight": "conv_body.weight",
        "model.1.sub.23.bias": "conv_body.bias",
        "model.3.weight": "conv_up1.weight",
        "model.3.bias": "conv_up1.bias",
        "model.6.weight": "conv_up2.weight",
        "model.6.bias": "conv_up2.bias",
        "model.8.weight": "conv_hr.weight",
        "model.8.bias": "conv_hr.bias",
        "model.10.weight": "conv_last.weight",
        "model.10.bias": "conv_last.bias",
    }
    out = {}
    for k, v in state_dict.items():
        if k in fixed:
            out[fixed[k]] = v
            continue
        m = rdb_re.match(k)
        if m is not None:
            sub, rdb, conv, kind = m.group(1), m.group(2), m.group(3), m.group(4)
            out[f"body.{sub}.rdb{rdb}.conv{conv}.{kind}"] = v
            continue
        out[k] = v
    return out


def build_upsampler(method: str) -> RealESRGANer:
    spec = METHODS[method]
    weights = ensure_model_weights(method)
    tile = int(os.environ.get("REALESRGAN_TILE", "0"))
    half = torch.cuda.is_available() and os.environ.get("REALESRGAN_HALF", "1") != "0"
    model = RRDBNet(
        num_in_ch=3,
        num_out_ch=3,
        num_feat=64,
        num_block=spec["num_block"],
        num_grow_ch=32,
        scale=spec["scale"],
    )
    return RealESRGANer(
        scale=spec["scale"],
        model_path=str(weights),
        model=model,
        tile=tile,
        tile_pad=10,
        pre_pad=0,
        half=half,
        gpu_id=0 if torch.cuda.is_available() else None,
    )


def get_upsampler(method: str) -> RealESRGANer:
    cached = _UPSAMPLER_CACHE.get(method)
    if cached is None:
        cached = build_upsampler(method)
        _UPSAMPLER_CACHE[method] = cached
    return cached


def get_sd_pipeline():
    """Lazy-load the Stable Diffusion x4 Upscaler pipeline.

    The model is ~3.5 GB and pulls a CLIP text encoder + VAE on top of
    the UNet. We keep it resident after first use. fp16 + attention
    slicing keeps VRAM around 10 GB on the 3090, well below the 24 GB
    cap, so concurrent ESRGAN models can stay loaded too.
    """
    global _SD_PIPELINE
    if _SD_PIPELINE is not None:
        return _SD_PIPELINE
    from diffusers import StableDiffusionUpscalePipeline
    cache = os.environ.get("HF_HOME", "/models/hf")
    Path(cache).mkdir(parents=True, exist_ok=True)
    dtype = torch.float16 if torch.cuda.is_available() else torch.float32
    pipe = StableDiffusionUpscalePipeline.from_pretrained(
        "stabilityai/stable-diffusion-x4-upscaler",
        torch_dtype=dtype,
        cache_dir=cache,
    )
    pipe = pipe.to("cuda" if torch.cuda.is_available() else "cpu")
    pipe.enable_attention_slicing()
    pipe.set_progress_bar_config(disable=True)
    _SD_PIPELINE = pipe
    return _SD_PIPELINE


def get_sdxl_pipeline():
    """Lazy-load SDXL img2img refiner pipeline. ~6.5 GB on disk, ~10 GB
    VRAM with fp16. We re-use the diffusers HF cache at /models/hf so
    the weights survive container rebuilds."""
    global _SDXL_PIPELINE
    if _SDXL_PIPELINE is not None:
        return _SDXL_PIPELINE
    from diffusers import StableDiffusionXLImg2ImgPipeline
    cache = os.environ.get("HF_HOME", "/models/hf")
    Path(cache).mkdir(parents=True, exist_ok=True)
    dtype = torch.float16 if torch.cuda.is_available() else torch.float32
    pipe = StableDiffusionXLImg2ImgPipeline.from_pretrained(
        "stabilityai/stable-diffusion-xl-base-1.0",
        torch_dtype=dtype,
        cache_dir=cache,
        variant="fp16",
        use_safetensors=True,
    )
    pipe = pipe.to("cuda" if torch.cuda.is_available() else "cpu")
    pipe.enable_attention_slicing()
    pipe.set_progress_bar_config(disable=True)
    _SDXL_PIPELINE = pipe
    return _SDXL_PIPELINE


def upscale_png_bytes_sdxl_refine(payload: bytes, scale: int,
                                  prompt: str) -> bytes:
    """SDXL img2img low-strength refine. Workflow:
       1. Split RGB + alpha from the source sprite.
       2. NN-upscale RGB by `scale` -- this is the init image. NN keeps
          edges crisp so the diffusion has clean lines to work from.
       3. Round dimensions up to a multiple of 8 (SDXL VAE constraint).
       4. Run SDXL img2img with strength=SDXL_STRENGTH (~0.35) and the
          fantasy/painted prompt. Low strength means the model refines
          textures without redrawing content, so the D2 silhouette and
          colour palette are preserved.
       5. Crop back to scale*src_size, NN-upscale alpha to match, recombine.
    """
    image = Image.open(io.BytesIO(payload)).convert("RGBA")
    src_w, src_h = image.size
    rgb = image.convert("RGB")
    alpha = image.split()[3]

    target_w = src_w * scale
    target_h = src_h * scale
    rgb_up = rgb.resize((target_w, target_h), Image.NEAREST)

    # SDXL needs multiples of 8 for the VAE. Pad with replicated edge
    # pixels so the diffusion sees no hard seams, then crop back after.
    pad_w = (8 - target_w % 8) % 8
    pad_h = (8 - target_h % 8) % 8
    if pad_w or pad_h:
        padded = Image.new("RGB", (target_w + pad_w, target_h + pad_h))
        padded.paste(rgb_up, (0, 0))
        rgb_up = padded

    pipeline = get_sdxl_pipeline()
    result = pipeline(
        prompt=prompt or SDXL_DEFAULT_PROMPT,
        negative_prompt=SDXL_NEGATIVE_PROMPT,
        image=rgb_up,
        strength=SDXL_STRENGTH,
        num_inference_steps=SDXL_NUM_STEPS,
    ).images[0]

    if pad_w or pad_h:
        result = result.crop((0, 0, target_w, target_h))

    new_alpha = alpha.resize((target_w, target_h), Image.NEAREST)
    rgba = result.convert("RGBA")
    rgba.putalpha(new_alpha)

    out = io.BytesIO()
    rgba.save(out, format="PNG")
    return out.getvalue()


def upscale_png_bytes_classic(payload: bytes, scale: int, method: str) -> bytes:
    """Classic, non-AI upscalers. All operate per-channel including alpha
    so the silhouette stays in sync with the colour data.

      bilinear / bicubic / lanczos: Pillow's built-in resamplers.
        Lanczos is generally crispest of the three; bilinear softest.
      xbrz: modern pattern-matching pixel-art upscaler (xBRZ, the
        Hyllian-derived algorithm used in ZSNES/Dolphin emulators).
        Anti-aliases edges while preserving palette tones; usually the
        best classic option for game-sprite art.
    """
    image = Image.open(io.BytesIO(payload)).convert("RGBA")
    src_w, src_h = image.size
    target = (src_w * scale, src_h * scale)

    if method == "xbrz":
        import xbrz
        # xbrz supports integer factors 2..6. We pass scale directly.
        result = xbrz.scale_pillow(image, scale)
    else:
        resample = {
            "bilinear": Image.BILINEAR,
            "bicubic":  Image.BICUBIC,
            "lanczos":  Image.LANCZOS,
        }[method]
        result = image.resize(target, resample)

    out = io.BytesIO()
    result.save(out, format="PNG")
    return out.getvalue()


def upscale_png_bytes_sd(payload: bytes, scale: int, prompt: str) -> bytes:
    """SD x4 upscale path. Splits alpha from RGB so the diffusion model
    only sees colour channels (it has no concept of transparency), then
    NN-upscales the alpha mask 4x to preserve the sprite silhouette.

    For scale=2 we run the 4x model and Lanczos-downsample to 2x; SD
    has no native 2x mode."""
    image = Image.open(io.BytesIO(payload)).convert("RGBA")
    rgb = image.convert("RGB")
    alpha = image.split()[3]  # 'L' mode, source resolution.

    pipeline = get_sd_pipeline()
    result = pipeline(
        prompt=prompt or SD_DEFAULT_PROMPT,
        image=rgb,
        num_inference_steps=SD_NUM_STEPS,
    ).images[0]

    new_alpha = alpha.resize(result.size, Image.NEAREST)
    rgba = result.convert("RGBA")
    rgba.putalpha(new_alpha)

    if scale == 2:
        target = (max(1, rgba.width // 2), max(1, rgba.height // 2))
        rgba = rgba.resize(target, Image.LANCZOS)

    out = io.BytesIO()
    rgba.save(out, format="PNG")
    return out.getvalue()


def upscale_png_bytes(payload: bytes, scale: int, method: str,
                      prompt: str = "") -> bytes:
    if method not in SUPPORTED_METHODS:
        raise HTTPException(status_code=400, detail=f"Unsupported method '{method}'")
    if scale not in SUPPORTED_SCALES:
        raise HTTPException(status_code=400, detail="Scale must be 2 or 4")

    if method == "sd-x4":
        return upscale_png_bytes_sd(payload, scale, prompt)
    if method == "sdxl-refine":
        return upscale_png_bytes_sdxl_refine(payload, scale, prompt)
    if method in ("bilinear", "bicubic", "lanczos", "xbrz"):
        return upscale_png_bytes_classic(payload, scale, method)

    image = Image.open(io.BytesIO(payload)).convert("RGBA")
    bgra = cv2.cvtColor(np.array(image), cv2.COLOR_RGBA2BGRA)
    upsampler = get_upsampler(method)
    enhanced, _ = upsampler.enhance(bgra, outscale=scale)
    rgba = cv2.cvtColor(enhanced, cv2.COLOR_BGRA2RGBA)

    out = io.BytesIO()
    Image.fromarray(rgba).save(out, format="PNG")
    return out.getvalue()


def upscale_archive_bytes(payload: bytes, scale: int, method: str,
                          prompt: str = "") -> bytes:
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
            dst.write_bytes(
                upscale_png_bytes(src.read_bytes(), scale, method, prompt))

        out = io.BytesIO()
        with zipfile.ZipFile(out, "w", compression=zipfile.ZIP_DEFLATED) as zf:
            for src in out_dir.rglob("*.png"):
                zf.write(src, src.relative_to(out_dir))
        return out.getvalue()


@app.get("/health")
def health() -> JSONResponse:
    models = {m: METHODS[m]["model_name"] for m in sorted(METHODS.keys())}
    models["sd-x4"] = "stable-diffusion-x4-upscaler"
    models["sdxl-refine"] = "stable-diffusion-xl-base-1.0"
    for m in ("bilinear", "bicubic", "lanczos", "xbrz"):
        models[m] = "(classic, no model)"
    loaded = sorted(_UPSAMPLER_CACHE.keys())
    if _SD_PIPELINE is not None:
        loaded.append("sd-x4")
    if _SDXL_PIPELINE is not None:
        loaded.append("sdxl-refine")
    return JSONResponse(
        {
            "ok": True,
            "cuda": torch.cuda.is_available(),
            "device": torch.cuda.get_device_name(0) if torch.cuda.is_available() else "cpu",
            "methods": sorted(SUPPORTED_METHODS),
            "scales": sorted(SUPPORTED_SCALES),
            "models": models,
            "loaded": sorted(loaded),
        }
    )


@app.post("/upscale/image")
async def upscale_image(
    request: Request,
    method: str = Query("realesrgan"),
    scale: int = Query(2),
    prompt: str = Query(""),
) -> Response:
    payload = await request.body()
    if not payload:
        raise HTTPException(status_code=400, detail="Request body is empty")
    return Response(
        upscale_png_bytes(payload, scale, method, prompt),
        media_type="image/png",
    )


@app.post("/upscale/archive")
async def upscale_archive(
    request: Request,
    method: str = Query("realesrgan"),
    scale: int = Query(2),
    prompt: str = Query(""),
) -> Response:
    payload = await request.body()
    if not payload:
        raise HTTPException(status_code=400, detail="Request body is empty")
    return Response(
        upscale_archive_bytes(payload, scale, method, prompt),
        media_type="application/zip",
    )
