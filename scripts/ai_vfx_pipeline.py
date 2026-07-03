#!/usr/bin/env python3
"""
AI VFX Pipeline — one-click orchestration for the UE + AI VFX workflow.

Connects to the UnrealMCP plugin in a running Unreal Editor and drives the
VFX tools added in Phase 2:

    generate/import 3D mesh -> create MIC -> spawn/position actor
    -> optional Niagara FX -> lights/camera -> screenshot

Usage:
    python scripts/ai_vfx_pipeline.py \
        --prompt "a glowing magic crystal" \
        --ref-image D:/Temp/crystal.png \
        --location 0,0,150 \
        --spawn-effect
"""

from __future__ import annotations

import argparse
import json
import logging
import re
import socket
import sys
import time
import uuid
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

logger = logging.getLogger("ai_vfx_pipeline")


# ---------------------------------------------------------------------------
# Project conventions
# ---------------------------------------------------------------------------
DEFAULT_TEMPLATES: Dict[str, str] = {
    "burst": "/Game/VFX/Templates/NS_BurstBase",
    "trail": "/Game/VFX/Templates/NS_TrailBase",
    "ambient": "/Game/VFX/Templates/NS_AmbientBase",
    "impact": "/Game/VFX/Templates/NS_ImpactBase",
}

DEFAULT_PARENT_MATERIALS: Dict[str, str] = {
    "opaque": "/Game/Materials/Generated/M_Generated_Opaque",
    "translucent": "/Game/Materials/Generated/M_Generated_Translucent",
    "unlit_additive": "/Game/Materials/Generated/M_Generated_Unlit_Additive",
    "masked": "/Game/Materials/Generated/M_Generated_Masked",
}

TEXTURE_SEMANTICS: List[Tuple[str, str]] = [
    # (lowercase substring, material parameter name)
    ("basecolor", "BaseColor"),
    ("albedo", "BaseColor"),
    ("diffuse", "BaseColor"),
    ("color", "BaseColor"),
    ("normal", "Normal"),
    ("roughness", "Roughness"),
    ("metallic", "Metallic"),
    ("opacitymask", "OpacityMask"),
    ("opacity", "Opacity"),
    ("emissive", "Emissive"),
    ("alpha", "Opacity"),
]


class MCPError(Exception):
    """Raised when the low-level MCP transport fails."""


class AIVfxPipelineError(Exception):
    """Raised when the AI VFX pipeline cannot complete a step."""


# ---------------------------------------------------------------------------
# Low-level MCP client
# ---------------------------------------------------------------------------
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
        """Send a single JSON-RPC request and return the parsed response dict."""
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
            raise MCPError(
                f"Cannot connect to UnrealMCP at {self.host}:{self.port}: {exc}"
            ) from exc

        if not response:
            raise MCPError("Empty response from UnrealMCP")

        try:
            return json.loads(response.decode("utf-8", errors="replace"))
        except json.JSONDecodeError as exc:
            raise MCPError(f"Invalid JSON response from UnrealMCP: {response!r}") from exc


# ---------------------------------------------------------------------------
# Pipeline
# ---------------------------------------------------------------------------
def _require_success(response: Dict[str, Any], context: str) -> Dict[str, Any]:
    """Return the result object, or raise AIVfxPipelineError on failure."""
    if not response.get("success"):
        error = response.get("error", "unknown error")
        raise AIVfxPipelineError(f"{context} failed: {error}")
    return response.get("result", {}) or {}


def _sanitize(name: str) -> str:
    """Turn an arbitrary prompt into a safe alphanumeric identifier."""
    safe = re.sub(r"[^a-zA-Z0-9]+", "_", name.strip())
    safe = safe.strip("_")
    return safe[:32] or "Asset"


def _classify_texture(asset_name: str) -> Optional[str]:
    """Map an imported texture asset name to a generated-master-material parameter."""
    lower = asset_name.lower()
    for substr, param in TEXTURE_SEMANTICS:
        if substr in lower:
            return param
    return None


def _canonical_object_path(asset_path: str) -> str:
    """Append the object-name suffix if it is missing (e.g. for set_material)."""
    if not asset_path:
        return asset_path
    name = asset_path.split("/")[-1]
    if "." not in name:
        return f"{asset_path}.{name}"
    return asset_path


class AIVfxPipeline:
    """High-level orchestrator for the AI -> UE asset workflow."""

    def __init__(
        self,
        mcp_host: str = "127.0.0.1",
        mcp_port: int = 13377,
        destination_path: str = "/Game/Generated/Meshes",
        material_destination: str = "/Game/Generated/Materials",
        niagara_destination: str = "/Game/VFX/Instances",
        generation_timeout: float = 600.0,
        poll_interval: float = 2.0,
        client: Optional[UnrealMCPClient] = None,
    ):
        self.client = client or UnrealMCPClient(mcp_host, mcp_port)
        self.destination_path = destination_path
        self.material_destination = material_destination
        self.niagara_destination = niagara_destination
        self.generation_timeout = generation_timeout
        self.poll_interval = poll_interval

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------
    def run(
        self,
        prompt: str,
        reference_image: Optional[str] = None,
        location: Tuple[float, float, float] = (0.0, 0.0, 0.0),
        rotation: Tuple[float, float, float] = (0.0, 0.0, 0.0),
        scale: Tuple[float, float, float] = (1.0, 1.0, 1.0),
        spawn_effect: bool = False,
        effect_template: Optional[str] = None,
        effect_params: Optional[Dict[str, Any]] = None,
        effect_location: Optional[Tuple[float, float, float]] = None,
        screenshot: bool = True,
        screenshot_name: Optional[str] = None,
        add_lights: bool = True,
        generation_params: Optional[Dict[str, Any]] = None,
        material_parent: Optional[str] = None,
    ) -> Dict[str, Any]:
        """
        Execute the full pipeline.

        Returns a dict with ``actorName``, ``assetPath``, ``materialPath``,
        ``effectActorName``, ``screenshotPath``, etc.
        """
        if not prompt:
            raise AIVfxPipelineError("prompt is required")
        if reference_image is None:
            raise AIVfxPipelineError(
                "reference_image is required in this MVP; prompt-only generation is not supported"
            )

        ref_path = Path(reference_image)
        if not ref_path.exists():
            raise AIVfxPipelineError(f"Reference image not found: {reference_image}")

        tag = uuid.uuid4().hex[:6]
        safe = _sanitize(prompt)
        actor_name = f"AIGenerated_{safe}_{tag}"
        material_name = f"MI_{safe}_{tag}"

        logger.info("Starting AI VFX pipeline for: %s", prompt)

        # 1. Generate/import 3D mesh.
        gen_result = self._generate_and_import(
            prompt=prompt,
            reference_image=str(ref_path.resolve()),
            actor_name=actor_name,
            location=location,
            rotation=rotation,
            scale=scale,
            generation_params=generation_params or {},
        )
        asset_path = gen_result.get("assetPath")
        actor_name = gen_result.get("actorName") or actor_name
        mesh_file = gen_result.get("meshFile")
        logger.info("Imported asset %s as actor %s", asset_path, actor_name)

        # 2. Create material instance from discovered textures and apply it.
        material_path = self._create_and_apply_material(
            actor_name=actor_name,
            material_name=material_name,
            material_parent=material_parent,
        )

        # 3. Optional Niagara effect.
        effect_actor_name: Optional[str] = None
        if spawn_effect:
            effect_actor_name = self._spawn_niagara_effect(
                prompt=prompt,
                safe=safe,
                tag=tag,
                effect_template=effect_template,
                effect_params=effect_params or {},
                effect_location=effect_location,
                base_location=location,
            )

        # 4. Lights / camera.
        if add_lights:
            self._setup_lights(tag)

        focus_resp = self.client.call("focus_viewport", {"actorName": actor_name})
        if not focus_resp.get("success"):
            logger.warning("Could not focus viewport: %s", focus_resp.get("error"))

        # 5. Screenshot.
        screenshot_path: Optional[str] = None
        if screenshot:
            filename = screenshot_name or f"aivfx_{safe}_{tag}"
            screen_result = _require_success(
                self.client.call("take_screenshot", {"filename": filename}),
                "take_screenshot",
            )
            screenshot_path = screen_result.get("path")
            logger.info("Screenshot saved to: %s", screenshot_path)

        return {
            "success": True,
            "prompt": prompt,
            "referenceImage": str(ref_path.resolve()),
            "actorName": actor_name,
            "assetPath": asset_path,
            "meshFile": mesh_file,
            "materialPath": material_path,
            "effectActorName": effect_actor_name,
            "screenshotPath": screenshot_path,
            "location": list(location),
        }

    # ------------------------------------------------------------------
    # Internal steps
    # ------------------------------------------------------------------
    def _generate_and_import(
        self,
        prompt: str,
        reference_image: str,
        actor_name: str,
        location: Tuple[float, float, float],
        rotation: Tuple[float, float, float],
        scale: Tuple[float, float, float],
        generation_params: Dict[str, Any],
    ) -> Dict[str, Any]:
        """Call generate_and_import_3d and poll until completion."""
        resp = self.client.call(
            "generate_and_import_3d",
            {
                "destinationPath": self.destination_path,
                "prompt": prompt,
                "referenceImage": reference_image,
                "actorName": actor_name,
                "location": list(location),
                "rotation": list(rotation),
                "scale": list(scale),
                "generationParams": generation_params,
                "waitForCompletion": False,
            },
            timeout=self.generation_timeout + 30.0,
        )
        result = _require_success(resp, "generate_and_import_3d")

        if result.get("status") == "pending":
            job_id = result["jobId"]
            logger.info("Hunyuan3D generation started: %s", job_id)
            return self._wait_for_generation(job_id)

        if result.get("status") != "completed":
            raise AIVfxPipelineError(
                f"Unexpected generate_and_import_3d status: {result.get('status')}"
            )
        return result

    def _wait_for_generation(self, job_id: str) -> Dict[str, Any]:
        """Poll get_generate_and_import_3d_status until done or timeout."""
        deadline = time.time() + self.generation_timeout
        while time.time() < deadline:
            resp = self.client.call(
                "get_generate_and_import_3d_status",
                {"jobId": job_id},
                timeout=30.0,
            )
            result = _require_success(resp, "get_generate_and_import_3d_status")
            status = result.get("status")
            logger.info("Generation job %s status: %s", job_id, status)

            if status == "completed":
                return result
            if status == "failed":
                raise AIVfxPipelineError(
                    f"Generation job failed: {result.get('message', 'unknown error')}"
                )

            time.sleep(self.poll_interval)

        raise AIVfxPipelineError(
            f"Generation job timed out after {self.generation_timeout}s"
        )

    def _discover_texture_maps(self) -> Dict[str, str]:
        """Look for imported Texture assets under the mesh destination path."""
        resp = self.client.call("get_asset_list", {"path": self.destination_path})
        result = _require_success(resp, "get_asset_list")
        assets = result.get("assets", [])

        maps: Dict[str, str] = {}
        for asset in assets:
            asset_class = asset.get("class", "")
            if "Texture" not in asset_class:
                continue
            param = _classify_texture(asset.get("name", ""))
            if param:
                maps[param] = asset.get("path", "")
        return maps

    def _pick_parent_material(self, maps: Dict[str, str]) -> str:
        """Choose a sensible master material based on the discovered maps."""
        if "OpacityMask" in maps:
            return DEFAULT_PARENT_MATERIALS["masked"]
        if "Opacity" in maps or "Emissive" in maps:
            return DEFAULT_PARENT_MATERIALS["translucent"]
        return DEFAULT_PARENT_MATERIALS["opaque"]

    def _create_and_apply_material(
        self,
        actor_name: str,
        material_name: str,
        material_parent: Optional[str],
    ) -> Optional[str]:
        """Create a MIC from discovered textures and assign it to the actor."""
        maps = self._discover_texture_maps()
        if not maps:
            logger.warning("No texture maps discovered; skipping MIC creation")
            return None

        parent = material_parent or self._pick_parent_material(maps)
        mic_path = f"{self.material_destination}/{material_name}"

        mat_resp = self.client.call(
            "create_material_from_textures",
            {
                "path": mic_path,
                "parentPath": parent,
                "maps": maps,
                "reuse": True,
            },
        )
        mat_result = _require_success(mat_resp, "create_material_from_textures")
        material_path = mat_result.get("path")
        if not material_path:
            raise AIVfxPipelineError("create_material_from_textures did not return a path")
        logger.info("Created MIC: %s", material_path)

        set_resp = self.client.call(
            "set_material",
            {
                "actorName": actor_name,
                "materialPath": _canonical_object_path(material_path),
            },
        )
        _require_success(set_resp, "set_material")
        logger.info("Applied material to actor %s", actor_name)
        return material_path

    def _pick_niagara_template(self, prompt: str, effect_template: Optional[str]) -> str:
        """Select a Niagara template from explicit key or prompt keywords."""
        if effect_template and effect_template in DEFAULT_TEMPLATES:
            return DEFAULT_TEMPLATES[effect_template]

        lower = prompt.lower()
        if "trail" in lower:
            return DEFAULT_TEMPLATES["trail"]
        if "ambient" in lower or "loop" in lower or "field" in lower:
            return DEFAULT_TEMPLATES["ambient"]
        if "impact" in lower or "hit" in lower or "explosion" in lower:
            return DEFAULT_TEMPLATES["impact"]
        return DEFAULT_TEMPLATES["burst"]

    def _spawn_niagara_effect(
        self,
        prompt: str,
        safe: str,
        tag: str,
        effect_template: Optional[str],
        effect_params: Dict[str, Any],
        effect_location: Optional[Tuple[float, float, float]],
        base_location: Tuple[float, float, float],
    ) -> str:
        """Duplicate a Niagara template, spawn it, and optionally tune parameters."""
        template_path = self._pick_niagara_template(prompt, effect_template)
        effect_name = f"NS_{safe}_{tag}"
        new_path = f"{self.niagara_destination}/{effect_name}"
        spawn_loc = list(effect_location) if effect_location else list(base_location)
        actor_name = f"{effect_name}_Actor"

        initial_params: Dict[str, Any] = {}
        for key, value in effect_params.items():
            if not key.startswith("User."):
                key = f"User.{key}"
            initial_params[key] = value

        resp = self.client.call(
            "duplicate_niagara_system",
            {
                "templatePath": template_path,
                "newPath": new_path,
                "initialParameters": initial_params,
                "spawnActor": True,
                "actorName": actor_name,
                "location": spawn_loc,
            },
        )
        result = _require_success(resp, "duplicate_niagara_system")
        spawned_actor = result.get("actorName") or actor_name
        logger.info("Spawned Niagara effect: %s", spawned_actor)

        for param_name, value in effect_params.items():
            full_name = param_name if param_name.startswith("User.") else f"User.{param_name}"
            set_resp = self.client.call(
                "set_niagara_parameter",
                {
                    "actorName": spawned_actor,
                    "parameterName": full_name,
                    "value": value,
                },
            )
            _require_success(set_resp, f"set_niagara_parameter {full_name}")

        return spawned_actor

    def _setup_lights(self, tag: str) -> None:
        """Add a simple three-point + sun lighting rig for the new asset."""
        self._spawn_point_light(f"AI_KeyLight_{tag}", [400.0, 300.0, 400.0], 80.0, [1.0, 0.95, 0.85])
        self._spawn_point_light(
            f"AI_RimLight_{tag}", [-400.0, -200.0, 300.0], 40.0, [0.5, 0.8, 1.0]
        )

        sun_name = f"AI_Sun_{tag}"
        _require_success(
            self.client.call(
                "spawn_actor",
                {
                    "className": "DirectionalLight",
                    "name": sun_name,
                    "location": [0.0, 400.0, 800.0],
                    "mobility": "Movable",
                },
            ),
            f"spawn sun {sun_name}",
        )
        _require_success(
            self.client.call(
                "set_light_parameters",
                {
                    "actorName": sun_name,
                    "intensity": 10.0,
                    "color": [1.0, 0.98, 0.90],
                },
            ),
            f"set sun params {sun_name}",
        )

        # Disable auto-exposure so the screenshot is not under/over-exposed.
        self.client.call(
            "run_console_command",
            {"command": "r.DefaultFeature.AutoExposure 0"},
        )

    def _spawn_point_light(
        self,
        name: str,
        location: List[float],
        intensity: float,
        color: List[float],
    ) -> None:
        """Spawn and configure a point light."""
        _require_success(
            self.client.call(
                "spawn_actor",
                {"className": "PointLight", "name": name, "location": location},
            ),
            f"spawn light {name}",
        )
        _require_success(
            self.client.call(
                "set_light_parameters",
                {"actorName": name, "intensity": intensity, "color": color},
            ),
            f"set light params {name}",
        )


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------
def _parse_location(value: str) -> Tuple[float, float, float]:
    try:
        parts = [float(x.strip()) for x in value.split(",")]
    except ValueError as exc:
        raise argparse.ArgumentTypeError(f"location must be x,y,z: {value}") from exc
    if len(parts) != 3:
        raise argparse.ArgumentTypeError(f"location must have 3 components: {value}")
    return tuple(parts)


def _parse_json(value: str) -> Dict[str, Any]:
    try:
        parsed = json.loads(value)
    except json.JSONDecodeError as exc:
        raise argparse.ArgumentTypeError(f"invalid JSON: {value}") from exc
    if not isinstance(parsed, dict):
        raise argparse.ArgumentTypeError(f"JSON must be an object: {value}")
    return parsed


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="AI VFX Pipeline — generate, import, and place AI assets in Unreal Engine"
    )
    parser.add_argument("--prompt", required=True, help="Text prompt describing the asset")
    parser.add_argument(
        "--ref-image",
        help="Reference image path (required in this MVP; prompt-only generation is not supported)",
    )
    parser.add_argument("--location", type=_parse_location, default="0,0,0")
    parser.add_argument("--rotation", type=_parse_location, default="0,0,0")
    parser.add_argument("--scale", type=_parse_location, default="1,1,1")
    parser.add_argument(
        "--spawn-effect", action="store_true", help="Also duplicate and spawn a Niagara template"
    )
    parser.add_argument(
        "--effect-template",
        choices=list(DEFAULT_TEMPLATES.keys()),
        help="Niagara template key (burst/trail/ambient/impact)",
    )
    parser.add_argument(
        "--effect-params",
        type=_parse_json,
        default="{}",
        help='JSON object of Niagara User parameters, e.g. {"Color":[1,0.2,0]}',
    )
    parser.add_argument("--effect-location", type=_parse_location, default=None)
    parser.add_argument("--destination-path", default="/Game/Generated/Meshes")
    parser.add_argument("--material-destination", default="/Game/Generated/Materials")
    parser.add_argument("--niagara-destination", default="/Game/VFX/Instances")
    parser.add_argument(
        "--material-parent", default=None, help="Override MIC parent material path"
    )
    parser.add_argument(
        "--generation-params",
        type=_parse_json,
        default="{}",
        help='JSON params forwarded to Hunyuan3D, e.g. {"seed":42,"steps":30}',
    )
    parser.add_argument("--screenshot-name", default=None)
    parser.add_argument("--no-screenshot", action="store_true")
    parser.add_argument("--no-lights", action="store_true")
    parser.add_argument("--mcp-host", default="127.0.0.1")
    parser.add_argument("--mcp-port", type=int, default=13377)
    parser.add_argument("--timeout", type=float, default=600.0)
    parser.add_argument("--poll-interval", type=float, default=2.0)
    parser.add_argument("--log-level", default="INFO")
    return parser


def main(argv: Optional[List[str]] = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    logging.basicConfig(
        level=getattr(logging, args.log_level.upper(), logging.INFO),
        format="%(asctime)s %(levelname)s: %(message)s",
    )

    pipeline = AIVfxPipeline(
        mcp_host=args.mcp_host,
        mcp_port=args.mcp_port,
        destination_path=args.destination_path,
        material_destination=args.material_destination,
        niagara_destination=args.niagara_destination,
        generation_timeout=args.timeout,
        poll_interval=args.poll_interval,
    )

    result = pipeline.run(
        prompt=args.prompt,
        reference_image=args.ref_image,
        location=args.location,
        rotation=args.rotation,
        scale=args.scale,
        spawn_effect=args.spawn_effect,
        effect_template=args.effect_template,
        effect_params=args.effect_params,
        effect_location=args.effect_location,
        screenshot=not args.no_screenshot,
        screenshot_name=args.screenshot_name,
        add_lights=not args.no_lights,
        generation_params=args.generation_params,
        material_parent=args.material_parent,
    )
    print(json.dumps(result, indent=2, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    sys.exit(main())
