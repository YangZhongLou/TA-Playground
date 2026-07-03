#!/usr/bin/env python3
"""
AI Animation Prototype — programmatic keyframe animation through UnrealMCP.

Drives Actor transforms and viewport/runtime camera from a JSON spec.
Useful for quick turnarounds and proof-of-concept shots before moving to
Sequencer.

Usage:
    python scripts/ai_animation_prototype.py \
        --spec examples/anim_crystal.json \
        --play-before-run \
        --screenshot-dir D:/Temp/anim_frames
"""

from __future__ import annotations

import argparse
import json
import logging
import math
import socket
import sys
import time
import uuid
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

logger = logging.getLogger("ai_animation_prototype")

EASE_MODES = {"linear", "ease_in_out", "step"}


def _lerp(a: float, b: float, t: float) -> float:
    return a + (b - a) * t


def _ease_in_out(t: float) -> float:
    """Smoothstep 0..1."""
    if t <= 0.0:
        return 0.0
    if t >= 1.0:
        return 1.0
    return t * t * (3.0 - 2.0 * t)


def _interp_scalar(a: float, b: float, t: float, mode: str) -> float:
    if mode == "step":
        return a if t < 1.0 else b
    if mode == "ease_in_out":
        t = _ease_in_out(t)
    return _lerp(a, b, t)


def _interp_vector(
    a: List[float], b: List[float], t: float, mode: str
) -> List[float]:
    if mode == "step":
        return list(a) if t < 1.0 else list(b)
    if mode == "ease_in_out":
        t = _ease_in_out(t)
    return [_lerp(a[i], b[i], t) for i in range(min(len(a), len(b)))]


class MCPError(Exception):
    """Raised when the MCP transport fails."""


class UnrealMCPClient:
    """Minimal TCP/JSON client for the UnrealMCP command server."""

    def __init__(self, host: str = "127.0.0.1", port: int = 13377, timeout: float = 30.0):
        self.host = host
        self.port = port
        self.timeout = timeout

    def call(
        self,
        method: str,
        params: Optional[Dict[str, Any]] = None,
        timeout: Optional[float] = None,
    ) -> Dict[str, Any]:
        payload = {
            "id": f"{method}_{time.time_ns()}",
            "method": method,
            "params": params or {},
        }
        data = json.dumps(payload, ensure_ascii=False).encode("utf-8") + b"\n"
        deadline = timeout if timeout is not None else self.timeout

        try:
            with socket.create_connection((self.host, self.port), timeout=deadline) as sock:
                sock.sendall(data)
                response = b""
                while True:
                    chunk = sock.recv(65536)
                    if not chunk:
                        break
                    response += chunk
                    if len(chunk) < 65536:
                        break
        except socket.timeout as exc:
            raise MCPError(f"MCP call timed out after {deadline}s: {method}") from exc
        except OSError as exc:
            raise MCPError(f"Cannot connect to UnrealMCP at {self.host}:{self.port}: {exc}") from exc

        if not response:
            raise MCPError("Empty response from UnrealMCP")

        try:
            return json.loads(response.decode("utf-8", errors="replace"))
        except json.JSONDecodeError as exc:
            raise MCPError(f"Invalid JSON response from UnrealMCP: {response!r}") from exc


def _require_success(response: Dict[str, Any], context: str) -> Dict[str, Any]:
    if not response.get("success"):
        error = response.get("error", "unknown error")
        raise MCPError(f"{context} failed: {error}")
    return response.get("result", {}) or {}


class Keyframe:
    """Single keyframe for an animated channel."""

    def __init__(self, data: Dict[str, Any]):
        self.time: float = float(data.get("time", 0.0))
        self.location: List[float] = list(data.get("location", [0.0, 0.0, 0.0]))
        self.rotation: List[float] = list(data.get("rotation", [0.0, 0.0, 0.0]))
        self.scale: List[float] = list(data.get("scale", [1.0, 1.0, 1.0]))


class ActorTrack:
    """All keyframes for one Actor. Supports existing actors or Blueprint spawning."""

    def __init__(self, data: Dict[str, Any]):
        self.blueprint_path: Optional[str] = data.get("blueprint_path")
        self.name: str = data.get("name") or data.get("actor_name") or ""
        self.easing: str = data.get("easing", "linear")
        self.spawn_location: List[float] = list(data.get("location", [0.0, 0.0, 0.0]))
        self.spawn_rotation: List[float] = list(data.get("rotation", [0.0, 0.0, 0.0]))
        self.spawn_scale: List[float] = list(data.get("scale", [1.0, 1.0, 1.0]))

        if not self.name and not self.blueprint_path:
            raise ValueError("Actor track needs 'name' or 'blueprint_path'")
        if self.easing not in EASE_MODES:
            raise ValueError(f"Unknown easing '{self.easing}' for actor {self.name or self.blueprint_path}")
        self.keyframes: List[Keyframe] = [Keyframe(k) for k in data.get("keyframes", [])]
        if len(self.keyframes) < 2:
            raise ValueError(f"Actor {self.name or self.blueprint_path} needs at least 2 keyframes")
        self.keyframes.sort(key=lambda k: k.time)

    def sample(self, t: float) -> Tuple[List[float], List[float], List[float]]:
        frames = self.keyframes
        if t <= frames[0].time:
            return frames[0].location, frames[0].rotation, frames[0].scale
        if t >= frames[-1].time:
            return frames[-1].location, frames[-1].rotation, frames[-1].scale

        for i in range(len(frames) - 1):
            a, b = frames[i], frames[i + 1]
            if a.time <= t <= b.time:
                span = b.time - a.time
                if span <= 0.0:
                    return a.location, a.rotation, a.scale
                ratio = (t - a.time) / span
                loc = _interp_vector(a.location, b.location, ratio, self.easing)
                rot = _interp_vector(a.rotation, b.rotation, ratio, self.easing)
                scl = _interp_vector(a.scale, b.scale, ratio, self.easing)
                return loc, rot, scl
        return frames[-1].location, frames[-1].rotation, frames[-1].scale


class CameraTrack:
    """Viewport or runtime camera keyframes."""

    def __init__(self, data: Dict[str, Any]):
        self.type: str = data.get("type", "viewport")
        self.keyframes: List[Keyframe] = [Keyframe(k) for k in data.get("keyframes", [])]
        if len(self.keyframes) < 2:
            raise ValueError("Camera track needs at least 2 keyframes")
        self.keyframes.sort(key=lambda k: k.time)
        self.easing: str = data.get("easing", "ease_in_out")
        if self.easing not in EASE_MODES:
            raise ValueError(f"Unknown camera easing '{self.easing}'")

    def sample(self, t: float) -> Tuple[List[float], List[float]]:
        frames = self.keyframes
        if t <= frames[0].time:
            return frames[0].location, frames[0].rotation
        if t >= frames[-1].time:
            return frames[-1].location, frames[-1].rotation

        for i in range(len(frames) - 1):
            a, b = frames[i], frames[i + 1]
            if a.time <= t <= b.time:
                span = b.time - a.time
                if span <= 0.0:
                    return a.location, a.rotation
                ratio = (t - a.time) / span
                loc = _interp_vector(a.location, b.location, ratio, self.easing)
                rot = _interp_vector(a.rotation, b.rotation, ratio, self.easing)
                return loc, rot
        return frames[-1].location, frames[-1].rotation


class AnimationSpec:
    """Parsed animation specification."""

    def __init__(self, data: Dict[str, Any]):
        self.duration: float = float(data.get("duration", 5.0))
        self.fps: float = float(data.get("fps", 24.0))
        self.actors: List[ActorTrack] = [ActorTrack(a) for a in data.get("actors", [])]
        self.camera: Optional[CameraTrack] = None
        if "camera" in data:
            self.camera = CameraTrack(data["camera"])
        self.screenshots: Dict[str, Any] = data.get("screenshots", {})


class AIAnimationPrototype:
    """Runs an animation spec against a live Unreal Editor."""

    def __init__(
        self,
        client: Optional[UnrealMCPClient] = None,
        mcp_host: str = "127.0.0.1",
        mcp_port: int = 13377,
    ):
        self.client = client or UnrealMCPClient(mcp_host, mcp_port)

    def run(
        self,
        spec: AnimationSpec,
        play_before_run: bool = False,
        screenshot_dir: Optional[str] = None,
    ) -> Dict[str, Any]:
        if play_before_run:
            logger.info("Starting Play In Editor")
            _require_success(self.client.call("play_in_editor"), "play_in_editor")
            time.sleep(1.0)

        screenshots: List[str] = []
        output_dir = Path(screenshot_dir) if screenshot_dir else None
        if output_dir:
            output_dir.mkdir(parents=True, exist_ok=True)

        self._spawn_blueprint_actors(spec)

        frame_count = int(math.ceil(spec.duration * spec.fps))
        logger.info(
            "Running animation: %.2fs @ %.1f fps (%d frames)",
            spec.duration,
            spec.fps,
            frame_count,
        )

        start_time = time.monotonic()

        for frame in range(frame_count + 1):
            t = min(frame / spec.fps, spec.duration)
            target_wall_time = start_time + t
            now = time.monotonic()
            sleep_needed = target_wall_time - now
            if sleep_needed > 0.0:
                time.sleep(sleep_needed)

            self._update_actors(spec, t)
            if spec.camera:
                self._update_camera(spec.camera, t)

            if output_dir and spec.screenshots.get("enabled", True):
                prefix = spec.screenshots.get("prefix", "frame")
                filename = f"{prefix}_{frame:04d}"
                result = _require_success(
                    self.client.call("take_screenshot", {"filename": filename}),
                    "take_screenshot",
                )
                path = result.get("path")
                if path:
                    screenshots.append(path)

        elapsed = time.monotonic() - start_time
        logger.info("Animation finished in %.2fs (target %.2fs)", elapsed, spec.duration)

        return {
            "success": True,
            "duration": spec.duration,
            "fps": spec.fps,
            "frames": frame_count + 1,
            "elapsed": elapsed,
            "screenshots": screenshots,
        }

    def _spawn_blueprint_actors(self, spec: AnimationSpec) -> None:
        for track in spec.actors:
            if not track.blueprint_path:
                continue
            actor_name = track.name or f"AIAnim_{uuid.uuid4().hex[:6]}"
            resp = self.client.call(
                "spawn_blueprint_actor",
                {
                    "blueprint_path": track.blueprint_path,
                    "name": actor_name,
                    "location": track.spawn_location,
                    "rotation": track.spawn_rotation,
                },
            )
            result = _require_success(resp, f"spawn_blueprint_actor {track.blueprint_path}")
            track.name = result.get("actorName") or actor_name
            logger.info("Spawned Blueprint actor: %s", track.name)

            if track.spawn_scale != [1.0, 1.0, 1.0]:
                _require_success(
                    self.client.call(
                        "set_actor_transform",
                        {
                            "name": track.name,
                            "location": track.spawn_location,
                            "rotation": track.spawn_rotation,
                            "scale": track.spawn_scale,
                        },
                    ),
                    f"set_actor_transform {track.name}",
                )

    def _update_actors(self, spec: AnimationSpec, t: float) -> None:
        for track in spec.actors:
            if not track.name:
                raise MCPError(f"Actor track has no name and no blueprint_path")
            loc, rot, scl = track.sample(t)
            _require_success(
                self.client.call(
                    "set_actor_transform",
                    {
                        "name": track.name,
                        "location": loc,
                        "rotation": rot,
                        "scale": scl,
                    },
                ),
                f"set_actor_transform {track.name}",
            )

    def _update_camera(self, camera: CameraTrack, t: float) -> None:
        loc, rot = camera.sample(t)
        if camera.type == "runtime":
            _require_success(
                self.client.call(
                    "set_runtime_camera_transform",
                    {"location": loc},
                ),
                "set_runtime_camera_transform",
            )
        else:
            _require_success(
                self.client.call(
                    "set_viewport_camera",
                    {"location": loc, "rotation": rot},
                ),
                "set_viewport_camera",
            )


def load_spec(path: str) -> AnimationSpec:
    with open(path, "r", encoding="utf-8") as f:
        data = json.load(f)
    return AnimationSpec(data)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="AI Animation Prototype — drive UE actors from a JSON keyframe spec"
    )
    parser.add_argument("--spec", required=True, help="Path to animation JSON spec")
    parser.add_argument(
        "--play-before-run",
        action="store_true",
        help="Enter Play In Editor before running the animation",
    )
    parser.add_argument("--stop-after-run", action="store_true", help="Stop PIE after animation")
    parser.add_argument("--screenshot-dir", default=None, help="Directory to save frame screenshots")
    parser.add_argument("--mcp-host", default="127.0.0.1")
    parser.add_argument("--mcp-port", type=int, default=13377)
    parser.add_argument("--log-level", default="INFO")
    return parser


def main(argv: Optional[List[str]] = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    logging.basicConfig(
        level=getattr(logging, args.log_level.upper(), logging.INFO),
        format="%(asctime)s %(levelname)s: %(message)s",
    )

    spec = load_spec(args.spec)
    runner = AIAnimationPrototype(mcp_host=args.mcp_host, mcp_port=args.mcp_port)

    result = runner.run(
        spec=spec,
        play_before_run=args.play_before_run,
        screenshot_dir=args.screenshot_dir,
    )

    if args.stop_after_run:
        logger.info("Stopping Play In Editor")
        try:
            _require_success(runner.client.call("stop_play_in_editor"), "stop_play_in_editor")
        except MCPError as exc:
            logger.warning("Could not stop PIE: %s", exc)

    print(json.dumps(result, indent=2, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    sys.exit(main())
