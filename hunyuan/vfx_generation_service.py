"""
Hunyuan3D local service wrapper for the UE + AI VFX workflow.

Provides a stable process boundary around ``Hunyuan3D-2.1/api_server.py``:

* start / health-check the API server subprocess
* generate a textured GLB from an image via ``generate_from_image``
* optional text-to-3D via a pluggable image generator in ``generate_from_text``
* write normalized outputs to ``hunyuan/output/<uuid>/mesh.{glb,png}`` + ``meta.json``

The wrapper intentionally keeps Hunyuan3D inference in a separate process so
long-running model loads do not block the Unreal Editor.
"""

import base64
import io
import json
import logging
import os
import subprocess
import sys
import time
import uuid
from pathlib import Path
from typing import Any, Callable, Dict, Optional

import requests
import trimesh
from PIL import Image

logger = logging.getLogger(__name__)

DEFAULT_BASE_URL = "http://localhost:8081"
DEFAULT_POLL_INTERVAL = 2.0
DEFAULT_TIMEOUT = 600.0
DEFAULT_MAX_RETRIES = 1


def _default_output_root() -> Path:
    """Return the default output root next to this module."""
    return Path(__file__).resolve().parent / "output"


def _default_api_server_script() -> Path:
    """Return the default path to Hunyuan3D-2.1/api_server.py."""
    return Path(__file__).resolve().parent / "Hunyuan3D-2.1" / "api_server.py"


def _default_python_executable() -> str:
    """Prefer the Hunyuan3D venv interpreter; fall back to the current one."""
    venv_python = Path(__file__).resolve().parent / "venv" / "Scripts" / "python.exe"
    if venv_python.exists():
        return str(venv_python)
    return sys.executable


def _image_to_base64(image_path: str) -> str:
    """Read an image file and return a base64-encoded PNG string."""
    path = Path(image_path)
    if not path.exists():
        raise FileNotFoundError(f"Input image not found: {image_path}")

    with Image.open(path) as img:
        if img.mode != "RGBA":
            img = img.convert("RGBA")
        buffer = io.BytesIO()
        img.save(buffer, format="PNG")
    return base64.b64encode(buffer.getvalue()).decode("utf-8")


def _extract_texture_preview(glb_path: str, png_path: str) -> bool:
    """
    Try to extract the first albedo/diffuse texture from a GLB as a PNG preview.

    If the mesh has no texture, create a blank placeholder PNG so downstream
    tools always have a file to reference.

    Returns True if a real texture was extracted, False if a placeholder was written.
    """
    try:
        scene = trimesh.load(glb_path, force="scene")
    except Exception as exc:
        logger.warning("Failed to load GLB for texture preview: %s", exc)
        _write_placeholder_preview(png_path)
        return False

    textures = _collect_textures(scene)
    for texture in textures:
        try:
            image = Image.open(io.BytesIO(texture.image))
            image.save(png_path)
            return True
        except Exception as exc:
            logger.warning("Failed to save collected texture: %s", exc)

    _write_placeholder_preview(png_path)
    return False


def _collect_textures(scene: trimesh.Scene) -> list:
    """Collect texture objects from a trimesh scene in priority order."""
    textures = []
    materials = set()

    for geometry in scene.geometry.values():
        material = getattr(geometry.visual, "material", None)
        if material is None:
            continue
        materials.add(material)

    for material in materials:
        # GLTF PBR metallic/roughness workflow
        base_color = getattr(material, "baseColorTexture", None)
        if base_color is not None:
            textures.append(base_color)
            continue
        # Older / fallback diffuse texture
        diffuse = getattr(material, "diffuseTexture", None)
        if diffuse is not None:
            textures.append(diffuse)

    return textures


def _write_placeholder_preview(png_path: str) -> None:
    """Write a blank placeholder preview so the file always exists."""
    Path(png_path).parent.mkdir(parents=True, exist_ok=True)
    Image.new("RGBA", (512, 512), (128, 128, 128, 255)).save(png_path)


class VfxGenerationService:
    """
    Wrapper around the Hunyuan3D-2.1 API server.

    Parameters
    ----------
    base_url:
        URL of the running (or to-be-started) API server.
    api_server_script:
        Path to ``Hunyuan3D-2.1/api_server.py``.
    python_executable:
        Python interpreter used to launch the API server.
    output_root:
        Directory where ``<uuid>/mesh.glb`` and friends will be written.
    server_kwargs:
        Extra CLI arguments passed to ``api_server.py`` (e.g.
        ``{"model_path": "tencent/Hunyuan3D-2.1", "low_vram_mode": True}``).
    poll_interval:
        Seconds between ``/status/{uid}`` polls.
    timeout:
        Maximum seconds to wait for a generation task.
    max_retries:
        Number of retry attempts for transient failures (server not ready,
        network hiccup).
    """

    def __init__(
        self,
        base_url: str = DEFAULT_BASE_URL,
        api_server_script: Optional[str] = None,
        python_executable: Optional[str] = None,
        output_root: Optional[str] = None,
        server_kwargs: Optional[Dict[str, Any]] = None,
        poll_interval: float = DEFAULT_POLL_INTERVAL,
        timeout: float = DEFAULT_TIMEOUT,
        max_retries: int = DEFAULT_MAX_RETRIES,
    ):
        self.base_url = base_url.rstrip("/")
        self.api_server_script = Path(api_server_script or _default_api_server_script())
        self.python_executable = python_executable or _default_python_executable()
        self.output_root = Path(output_root or _default_output_root())
        self.server_kwargs = server_kwargs or {}
        self.poll_interval = poll_interval
        self.timeout = timeout
        self.max_retries = max_retries

        self._server_process: Optional[subprocess.Popen] = None

    def _health_url(self) -> str:
        return f"{self.base_url}/health"

    def _send_url(self) -> str:
        return f"{self.base_url}/send"

    def _status_url(self, uid: str) -> str:
        return f"{self.base_url}/status/{uid}"

    def is_server_running(self) -> bool:
        """Return True if the API server responds to /health."""
        try:
            response = requests.get(self._health_url(), timeout=5)
            return response.status_code == 200 and response.json().get("status") == "healthy"
        except Exception as exc:
            logger.debug("Health check failed: %s", exc)
            return False

    def start_server(self) -> subprocess.Popen:
        """Start ``api_server.py`` as a subprocess and return the process handle."""
        if not self.api_server_script.exists():
            raise FileNotFoundError(f"API server script not found: {self.api_server_script}")

        cmd = [self.python_executable, str(self.api_server_script)]
        for key, value in self.server_kwargs.items():
            flag = f"--{key.replace('_', '-')}"
            if isinstance(value, bool):
                if value:
                    cmd.append(flag)
            else:
                cmd.extend([flag, str(value)])

        logger.info("Starting Hunyuan3D API server: %s", " ".join(cmd))
        self._server_process = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        return self._server_process

    def ensure_server_running(self) -> None:
        """Start the server if it is not already responding."""
        if self.is_server_running():
            logger.info("Hunyuan3D API server is already running at %s", self.base_url)
            return

        self.start_server()
        deadline = time.time() + 120
        while time.time() < deadline:
            if self.is_server_running():
                logger.info("Hunyuan3D API server is ready")
                return
            time.sleep(1)
        raise RuntimeError("Hunyuan3D API server failed to become healthy within 120 seconds")

    def stop_server(self) -> None:
        """Terminate the managed subprocess, if any."""
        if self._server_process is None or self._server_process.poll() is not None:
            return
        logger.info("Stopping Hunyuan3D API server (pid %s)", self._server_process.pid)
        self._server_process.terminate()
        try:
            self._server_process.wait(timeout=30)
        except subprocess.TimeoutExpired:
            self._server_process.kill()
            self._server_process.wait(timeout=10)

    def _wait_for_completion(self, uid: str) -> bytes:
        """Poll ``/status/{uid}`` until the model is ready and return decoded GLB bytes."""
        deadline = time.time() + self.timeout
        while time.time() < deadline:
            response = requests.get(self._status_url(uid), timeout=10)
            response.raise_for_status()
            data = response.json()
            status = data.get("status")

            if status == "completed":
                model_base64 = data.get("model_base64")
                if not model_base64:
                    raise RuntimeError("Status 'completed' but no model_base64 returned")
                return base64.b64decode(model_base64)
            if status == "error":
                raise RuntimeError(f"Generation failed: {data.get('message', 'unknown error')}")

            time.sleep(self.poll_interval)

        raise TimeoutError(f"Generation timed out after {self.timeout} seconds (uid={uid})")

    def _build_request_payload(self, image_base64: str, params: Dict[str, Any]) -> Dict[str, Any]:
        """Build the JSON payload accepted by ``/send``."""
        payload = {
            "image": image_base64,
            "texture": True,
            "remove_background": True,
            "seed": params.get("seed", 1234),
            "octree_resolution": params.get("octree_resolution", 256),
            "num_inference_steps": params.get("num_inference_steps", 5),
            "guidance_scale": params.get("guidance_scale", 5.0),
            "num_chunks": params.get("num_chunks", 8000),
            "face_count": params.get("face_count", 40000),
        }
        # Allow explicit overrides without forcing defaults on None
        for key in payload:
            if key in params and params[key] is not None:
                payload[key] = params[key]
        return payload

    def generate_from_image(
        self,
        image_path: str,
        params: Optional[Dict[str, Any]] = None,
        output_uuid: Optional[str] = None,
        auto_start_server: bool = True,
    ) -> Dict[str, str]:
        """
        Generate a textured GLB from an input image.

        Parameters
        ----------
        image_path:
            Path to the reference image (PNG/JPG/etc.).
        params:
            Generation parameters forwarded to Hunyuan3D (seed, octree_resolution,
            num_inference_steps, guidance_scale, num_chunks, face_count).
        output_uuid:
            Optional UUID for the output directory. Generated automatically if omitted.
        auto_start_server:
            If True, start the API server when it is not already running.

        Returns
        -------
        dict with keys ``output_dir``, ``mesh_path``, ``preview_path``, ``meta_path``,
        and ``uid``.
        """
        params = params or {}
        if auto_start_server:
            self.ensure_server_running()

        image_base64 = _image_to_base64(image_path)
        payload = self._build_request_payload(image_base64, params)

        last_error: Optional[Exception] = None
        for attempt in range(self.max_retries + 1):
            try:
                response = requests.post(self._send_url(), json=payload, timeout=30)
                response.raise_for_status()
                uid = response.json().get("uid")
                if not uid:
                    raise RuntimeError("API server did not return a task uid")
                break
            except Exception as exc:
                last_error = exc
                logger.warning("Send request failed (attempt %d/%d): %s", attempt + 1, self.max_retries + 1, exc)
                if attempt < self.max_retries:
                    time.sleep(self.poll_interval)
        else:
            raise RuntimeError(f"Failed to submit generation task after {self.max_retries + 1} attempts") from last_error

        glb_bytes = self._wait_for_completion(uid)
        return self._write_outputs(uid, glb_bytes, image_path, params, output_uuid or uid)

    def generate_from_text(
        self,
        text_prompt: str,
        params: Optional[Dict[str, Any]] = None,
        image_generator: Optional[Callable[[str], str]] = None,
        **kwargs,
    ) -> Dict[str, str]:
        """
        Optional text-to-3D entry point.

        Local text-to-3D is not supported directly by Hunyuan3D. This method either
        accepts a pluggable ``image_generator`` that turns the prompt into an image
        path and then calls ``generate_from_image``, or raises ``NotImplementedError``
        with guidance on supplying a reference image.

        Parameters
        ----------
        text_prompt:
            The text description of the desired asset.
        params:
            Generation parameters forwarded to ``generate_from_image``.
        image_generator:
            Optional callable ``image_generator(prompt) -> image_path``. When provided,
            the returned image is used as the reference for image-to-3D generation.
        **kwargs:
            Forwarded to ``generate_from_image``.

        Returns
        -------
        Same dict as ``generate_from_image``.
        """
        if image_generator is None:
            raise NotImplementedError(
                "Text-to-3D is not implemented locally in this MVP. "
                "Provide a reference image to generate_from_image, or supply an "
                "image_generator callable (e.g. a local SD/FLUX wrapper) to generate_from_text."
            )

        image_path = image_generator(text_prompt)
        return self.generate_from_image(image_path, params=params, **kwargs)

    def _write_outputs(
        self,
        uid: str,
        glb_bytes: bytes,
        image_path: str,
        params: Dict[str, Any],
        output_uuid: str,
    ) -> Dict[str, str]:
        """Persist the generated GLB, a PNG preview, and metadata."""
        output_dir = self.output_root / output_uuid
        output_dir.mkdir(parents=True, exist_ok=True)

        mesh_path = output_dir / "mesh.glb"
        preview_path = output_dir / "mesh.png"
        meta_path = output_dir / "meta.json"

        mesh_path.write_bytes(glb_bytes)
        _extract_texture_preview(str(mesh_path), str(preview_path))

        meta = {
            "uid": uid,
            "output_uuid": output_uuid,
            "input_image": os.path.abspath(image_path),
            "mesh_path": str(mesh_path.resolve()),
            "preview_path": str(preview_path.resolve()),
            "prompt": params.get("prompt", ""),
            "seed": params.get("seed", 1234),
            "steps": params.get("num_inference_steps", 5),
            "octree_resolution": params.get("octree_resolution", 256),
            "guidance_scale": params.get("guidance_scale", 5.0),
            "model_version": self.server_kwargs.get("model_path", "tencent/Hunyuan3D-2.1"),
            "texture": True,
        }
        meta_path.write_text(json.dumps(meta, indent=2, ensure_ascii=False), encoding="utf-8")

        logger.info("Saved generated asset to %s", output_dir)
        return {
            "output_dir": str(output_dir.resolve()),
            "mesh_path": str(mesh_path.resolve()),
            "preview_path": str(preview_path.resolve()),
            "meta_path": str(meta_path.resolve()),
            "uid": uid,
        }


def main():
    """Minimal CLI for ad-hoc generation."""
    import argparse

    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(name)s: %(message)s")

    parser = argparse.ArgumentParser(description="Hunyuan3D VFX generation service wrapper")
    parser.add_argument("image", type=str, help="Input image path")
    parser.add_argument("--output-root", type=str, default=None, help="Output directory root")
    parser.add_argument("--base-url", type=str, default=DEFAULT_BASE_URL, help="API server base URL")
    parser.add_argument("--model-path", type=str, default="tencent/Hunyuan3D-2.1", help="Hunyuan3D model path")
    parser.add_argument("--device", type=str, default="cuda", help="Device passed to api_server.py")
    parser.add_argument("--low-vram-mode", action="store_true", help="Enable low VRAM mode")
    parser.add_argument("--seed", type=int, default=1234, help="Random seed")
    parser.add_argument("--octree-resolution", type=int, default=256, help="Octree resolution")
    parser.add_argument("--num-inference-steps", type=int, default=5, help="Inference steps")
    parser.add_argument("--guidance-scale", type=float, default=5.0, help="Guidance scale")
    parser.add_argument("--timeout", type=float, default=DEFAULT_TIMEOUT, help="Generation timeout")
    parser.add_argument("--no-auto-start", action="store_true", help="Do not auto-start the API server")
    args = parser.parse_args()

    service = VfxGenerationService(
        base_url=args.base_url,
        output_root=args.output_root,
        server_kwargs={
            "model_path": args.model_path,
            "device": args.device,
            "low_vram_mode": args.low_vram_mode,
        },
        timeout=args.timeout,
    )

    result = service.generate_from_image(
        args.image,
        params={
            "seed": args.seed,
            "octree_resolution": args.octree_resolution,
            "num_inference_steps": args.num_inference_steps,
            "guidance_scale": args.guidance_scale,
        },
        auto_start_server=not args.no_auto_start,
    )
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
