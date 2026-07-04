"""
UE Editor integration tests for the UnrealMCP VFX workflow.

Run this script inside the Unreal Editor Python environment:

    File > Execute Python Script > Content/Python/test_vfx_mcp_integration.py

or paste the contents into the Python output log console.

The script exercises four key MCP methods without requiring a live Hunyuan3D
service:

    - generate_and_import_3d   (direct meshFile import path)
    - create_material_from_textures
    - duplicate_niagara_system
    - set_niagara_parameter

It creates temporary geometry/textures, drives the commands through the local
UnrealMCP socket server (127.0.0.1:13377), and prints a JSON test summary.

Known limitations (cannot be verified in a headless environment):
    - This script must run while the Unreal Editor is open.
    - The test parent material and Niagara templates must already exist, or the
      corresponding test cases will be skipped.
    - Actual Hunyuan3D generation is NOT exercised here (too slow and requires
      the GPU inference environment).
"""

from __future__ import annotations

import json
import socket
import struct
import time
import uuid
import zlib
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

# `unreal` is only available inside the Editor Python environment.
try:
    import unreal  # type: ignore[import]
except ImportError:
    unreal = None

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
MCP_HOST = "127.0.0.1"
MCP_PORT = 13377
MCP_TIMEOUT = 60.0

PARENT_MATERIAL_OPAQUE = "/Game/Materials/Generated/M_Generated_Opaque"
NIAGARA_TEMPLATES = [
    "/Game/VFX/Templates/NS_BurstBase",
    "/Game/VFX/Templates/NS_TrailBase",
    "/Game/VFX/Templates/NS_AmbientBase",
    "/Game/VFX/Templates/NS_ImpactBase",
]


# ---------------------------------------------------------------------------
# Logging helpers
# ---------------------------------------------------------------------------
def _log(message: str) -> None:
    if unreal is not None:
        unreal.log(message)
    print(message)


def _log_warning(message: str) -> None:
    if unreal is not None:
        unreal.log_warning(message)
    print(f"WARN: {message}")


def _log_error(message: str) -> None:
    if unreal is not None:
        unreal.log_error(message)
    print(f"ERROR: {message}")


# ---------------------------------------------------------------------------
# Low-level MCP client (talks to the local UnrealMCP command server)
# ---------------------------------------------------------------------------
def mcp_call(method: str, params: Optional[Dict[str, Any]] = None, timeout: float = MCP_TIMEOUT) -> Dict[str, Any]:
    """Send a JSON-RPC request to UnrealMCP and return the parsed response."""
    payload = {
        "id": f"{method}_{time.time_ns()}",
        "method": method,
        "params": params or {},
    }
    data = json.dumps(payload, ensure_ascii=False).encode("utf-8")

    with socket.create_connection((MCP_HOST, MCP_PORT), timeout=timeout) as sock:
        sock.sendall(data)
        # Shut down the write side so the server knows the request is complete.
        try:
            sock.shutdown(socket.SHUT_WR)
        except OSError:
            pass

        response = b""
        while True:
            chunk = sock.recv(65536)
            if not chunk:
                break
            response += chunk

    if not response:
        raise RuntimeError("Empty response from UnrealMCP")

    try:
        return json.loads(response.decode("utf-8", errors="replace"))
    except json.JSONDecodeError as exc:
        raise RuntimeError(f"Invalid JSON response: {response!r}") from exc


def require_success(response: Dict[str, Any], context: str) -> Dict[str, Any]:
    """Return the result object or raise AssertionError with the MCP error."""
    if not response.get("success"):
        error = response.get("error", "unknown error")
        raise AssertionError(f"{context} failed: {error}")
    return response.get("result", {}) or {}


# ---------------------------------------------------------------------------
# Asset helpers
# ---------------------------------------------------------------------------
def _project_saved_dir() -> Path:
    if unreal is not None:
        return Path(unreal.Paths.project_saved_dir())
    return Path(__file__).resolve().parent.parent.parent / "Saved"


def _temp_dir() -> Path:
    path = _project_saved_dir() / "Temp" / "VFXIntegrationTests"
    path.mkdir(parents=True, exist_ok=True)
    return path


def _write_obj_cube(path: Path, size: float = 50.0) -> None:
    """Write a simple axis-aligned cube as a Wavefront OBJ file."""
    half = size / 2.0
    vertices = [
        (-half, -half, -half),
        (half, -half, -half),
        (half, half, -half),
        (-half, half, -half),
        (-half, -half, half),
        (half, -half, half),
        (half, half, half),
        (-half, half, half),
    ]
    faces = [
        (1, 2, 3, 4),
        (5, 6, 7, 8),
        (1, 2, 6, 5),
        (2, 3, 7, 6),
        (3, 4, 8, 7),
        (4, 1, 5, 8),
    ]
    lines = ["# Test cube generated by test_vfx_mcp_integration.py", "o TestCube"]
    for v in vertices:
        lines.append(f"v {v[0]} {v[1]} {v[2]}")
    for f in faces:
        lines.append(f"f {f[0]} {f[1]} {f[2]} {f[3]}")
    path.write_text("\n".join(lines), encoding="utf-8")


def _png_bytes(width: int, height: int, rgb: Tuple[int, int, int]) -> bytes:
    """Create a minimal RGB PNG from standard library modules only."""

    def chunk(tag: bytes, data: bytes) -> bytes:
        crc = zlib.crc32(tag + data) & 0xFFFFFFFF
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", crc)

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    # One filter byte (0 = none) per row, followed by RGB triples.
    row = bytes([0]) + bytes(rgb) * width
    raw = row * height
    return (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", ihdr)
        + chunk(b"IDAT", zlib.compress(raw, 9))
        + chunk(b"IEND", b"")
    )


def _write_png(path: Path, width: int, height: int, rgb: Tuple[int, int, int]) -> None:
    path.write_bytes(_png_bytes(width, height, rgb))


def _canonical_object_path(asset_path: str) -> str:
    """Append the object-name suffix if it is missing."""
    if not asset_path:
        return asset_path
    name = asset_path.split("/")[-1]
    if "." not in name:
        return f"{asset_path}.{name}"
    return asset_path


# ---------------------------------------------------------------------------
# Test runner primitives
# ---------------------------------------------------------------------------
class TestResult:
    def __init__(self, name: str, passed: bool, message: str = "", elapsed_ms: float = 0.0):
        self.name = name
        self.passed = passed
        self.message = message
        self.elapsed_ms = elapsed_ms

    def as_dict(self) -> Dict[str, Any]:
        return {
            "name": self.name,
            "passed": self.passed,
            "message": self.message,
            "elapsedMs": round(self.elapsed_ms, 2),
        }


class IntegrationTestRunner:
    def __init__(self) -> None:
        self.results: List[TestResult] = []
        self.created_actors: List[str] = []
        self.created_assets: List[str] = []
        self.temp_files: List[Path] = []

    def run(self) -> Dict[str, Any]:
        start = time.time()
        try:
            self._check_mcp_connection()
            self._test_generate_and_import_3d()
            self._test_material_workflow()
            self._test_niagara_workflow()
        except Exception as exc:  # noqa: BLE001
            _log_error(f"Unexpected error during test run: {exc}")
        finally:
            self._cleanup()

        elapsed = (time.time() - start) * 1000.0
        passed = sum(1 for r in self.results if r.passed)
        total = len(self.results)
        return {
            "success": passed == total and total > 0,
            "passed": passed,
            "total": total,
            "elapsedMs": round(elapsed, 2),
            "results": [r.as_dict() for r in self.results],
            "createdActors": self.created_actors,
            "createdAssets": self.created_assets,
        }

    def _record(self, name: str, passed: bool, message: str = "", elapsed_ms: float = 0.0) -> None:
        self.results.append(TestResult(name, passed, message, elapsed_ms))
        status = "PASS" if passed else "FAIL"
        _log(f"[{status}] {name} ({elapsed_ms:.1f}ms) {message}".strip())

    def _check_mcp_connection(self) -> None:
        try:
            resp = mcp_call("get_editor_info", {}, timeout=5.0)
            require_success(resp, "get_editor_info")
            _log("Connected to UnrealMCP")
        except Exception as exc:  # noqa: BLE001
            _log_error(f"Cannot connect to UnrealMCP at {MCP_HOST}:{MCP_PORT}: {exc}")
            raise

    def _test_generate_and_import_3d(self) -> None:
        name = "generate_and_import_3d (direct import)"
        t0 = time.time()
        actor_name = f"AIVFXTest_Cube_{uuid.uuid4().hex[:6]}"
        obj_path = _temp_dir() / f"test_cube_{uuid.uuid4().hex[:6]}.obj"
        _write_obj_cube(obj_path)
        self.temp_files.append(obj_path)

        try:
            resp = mcp_call(
                "generate_and_import_3d",
                {
                    "meshFile": str(obj_path),
                    "destinationPath": "/Game/Generated/Test/Meshes",
                    "actorName": actor_name,
                    "location": [0.0, 0.0, 100.0],
                    "rotation": [0.0, 0.0, 0.0],
                    "scale": [1.0, 1.0, 1.0],
                },
            )
            result = require_success(resp, name)
            assert result.get("status") == "completed", f"unexpected status: {result.get('status')}"
            assert result.get("assetPath"), "missing assetPath"
            assert result.get("actorName") == actor_name, f"actor mismatch: {result.get('actorName')}"

            # Verify the actor is actually in the world.
            list_resp = mcp_call("get_actor_list", {})
            actors = require_success(list_resp, "get_actor_list").get("actors", [])
            assert any(a.get("name") == actor_name for a in actors), f"actor {actor_name} not found in scene"

            self.created_actors.append(actor_name)
            self.created_assets.append(_canonical_object_path(result["assetPath"]))
            self._record(name, True, f"actor={actor_name}", (time.time() - t0) * 1000.0)
        except Exception as exc:  # noqa: BLE001
            self._record(name, False, str(exc), (time.time() - t0) * 1000.0)

    def _test_material_workflow(self) -> None:
        # Find an actor created earlier; if none exists, skip.
        actor_name = next((a for a in self.created_actors if a.startswith("AIVFXTest_Cube_")), None)
        if actor_name is None:
            self._record(
                "create_material_from_textures",
                False,
                "No test actor available from generate_and_import_3d",
            )
            self._record("set_texture_parameter", False, "No test actor available")
            return

        # Import two tiny test textures.
        white_png = _temp_dir() / f"test_white_{uuid.uuid4().hex[:6]}.png"
        red_png = _temp_dir() / f"test_red_{uuid.uuid4().hex[:6]}.png"
        _write_png(white_png, 8, 8, (255, 255, 255))
        _write_png(red_png, 8, 8, (255, 0, 0))
        self.temp_files.extend([white_png, red_png])

        white_tex_path = self._import_texture(white_png, "T_AIVFXTest_White")
        red_tex_path = self._import_texture(red_png, "T_AIVFXTest_Red")
        if not white_tex_path or not red_tex_path:
            self._record(
                "create_material_from_textures",
                False,
                "Could not import test textures via MCP",
            )
            self._record("set_texture_parameter", False, "Texture import failed")
            return

        # Only run if the expected opaque master material exists.
        material_exists = self._asset_exists(PARENT_MATERIAL_OPAQUE)
        if not material_exists:
            msg = f"Parent material not found: {PARENT_MATERIAL_OPAQUE}"
            self._record("create_material_from_textures", False, msg)
            self._record("set_texture_parameter", False, msg)
            return

        # --- create_material_from_textures ---
        name = "create_material_from_textures"
        t0 = time.time()
        mic_name = f"MI_AIVFXTest_{uuid.uuid4().hex[:6]}"
        mic_path = f"/Game/Generated/Test/Materials/{mic_name}"
        try:
            resp = mcp_call(
                "create_material_from_textures",
                {
                    "path": mic_path,
                    "parentPath": PARENT_MATERIAL_OPAQUE,
                    "maps": {"BaseColor": white_tex_path},
                    "reuse": False,
                },
            )
            result = require_success(resp, name)
            returned_path = result.get("path")
            assert returned_path, "missing returned MIC path"
            assert self._asset_exists(returned_path), f"created MIC does not exist: {returned_path}"
            self.created_assets.append(_canonical_object_path(returned_path))
            self._record(name, True, f"path={returned_path}", (time.time() - t0) * 1000.0)
        except Exception as exc:  # noqa: BLE001
            self._record(name, False, str(exc), (time.time() - t0) * 1000.0)
            return

        # Apply the MIC to the test actor so set_texture_parameter has a target.
        try:
            set_mat_resp = mcp_call(
                "set_material",
                {
                    "actorName": actor_name,
                    "materialPath": _canonical_object_path(returned_path),
                    "slotIndex": 0,
                },
            )
            require_success(set_mat_resp, "set_material")
        except Exception as exc:  # noqa: BLE001
            _log_warning(f"set_material failed (set_texture_parameter may still work): {exc}")

        # --- set_texture_parameter ---
        name = "set_texture_parameter"
        t0 = time.time()
        try:
            resp = mcp_call(
                "set_texture_parameter",
                {
                    "actorName": actor_name,
                    "parameterName": "BaseColor",
                    "texturePath": red_tex_path,
                    "slotIndex": 0,
                },
            )
            require_success(resp, name)
            self._record(name, True, f"set BaseColor to {red_tex_path}", (time.time() - t0) * 1000.0)
        except Exception as exc:  # noqa: BLE001
            self._record(name, False, str(exc), (time.time() - t0) * 1000.0)

    def _import_texture(self, png_path: Path, asset_name: str) -> Optional[str]:
        """Import a PNG texture and return its content path, or None on failure."""
        try:
            resp = mcp_call(
                "import_asset",
                {
                    "file_path": str(png_path),
                    "destination_path": "/Game/Generated/Test/Textures",
                },
            )
            result = require_success(resp, "import_asset")
            imported = result.get("imported", [])
            for item in imported:
                path = item.get("path")
                if path:
                    canonical = _canonical_object_path(path)
                    self.created_assets.append(canonical)
                    return canonical
        except Exception as exc:  # noqa: BLE001
            _log_warning(f"Failed to import {png_path}: {exc}")
        return None

    def _asset_exists(self, asset_path: str) -> bool:
        try:
            resp = mcp_call(
                "get_asset_info",
                {"path": _canonical_object_path(asset_path)},
                timeout=10.0,
            )
            return resp.get("success", False)
        except Exception:  # noqa: BLE001
            return False

    def _test_niagara_workflow(self) -> None:
        # Pick the first template that actually exists.
        template_path: Optional[str] = None
        for tp in NIAGARA_TEMPLATES:
            if self._asset_exists(tp):
                template_path = tp
                break

        if template_path is None:
            msg = "No Niagara template found in /Game/VFX/Templates"
            self._record("duplicate_niagara_system", False, msg)
            self._record("set_niagara_parameter", False, msg)
            return

        actor_name = f"AIVFXTest_Niagara_{uuid.uuid4().hex[:6]}"
        ns_name = f"NS_AIVFXTest_{uuid.uuid4().hex[:6]}"
        new_path = f"/Game/Generated/Test/VFX/{ns_name}"

        # --- duplicate_niagara_system ---
        name = "duplicate_niagara_system"
        t0 = time.time()
        try:
            resp = mcp_call(
                "duplicate_niagara_system",
                {
                    "templatePath": template_path,
                    "newPath": new_path,
                    "spawnActor": True,
                    "actorName": actor_name,
                    "location": [0.0, 0.0, 150.0],
                    "initialParameters": {"User.Color": [1.0, 0.0, 0.0]},
                },
            )
            result = require_success(resp, name)
            returned_path = result.get("newPath")
            assert returned_path, "missing newPath"
            assert self._asset_exists(returned_path), f"duplicated system not found: {returned_path}"

            # Verify the spawned actor is in the world.
            list_resp = mcp_call("get_actor_list", {})
            actors = require_success(list_resp, "get_actor_list").get("actors", [])
            assert any(a.get("name") == actor_name for a in actors), f"Niagara actor {actor_name} not found"

            self.created_actors.append(actor_name)
            self.created_assets.append(_canonical_object_path(returned_path))
            self._record(
                name,
                True,
                f"template={template_path} actor={actor_name}",
                (time.time() - t0) * 1000.0,
            )
        except Exception as exc:  # noqa: BLE001
            self._record(name, False, str(exc), (time.time() - t0) * 1000.0)
            self._record("set_niagara_parameter", False, "duplicate_niagara_system failed")
            return

        # --- set_niagara_parameter ---
        name = "set_niagara_parameter"
        t0 = time.time()
        try:
            resp = mcp_call(
                "set_niagara_parameter",
                {
                    "actorName": actor_name,
                    "parameterName": "User.Color",
                    "value": [0.0, 1.0, 0.0],
                },
            )
            result = require_success(resp, name)
            assert result.get("valueType") in ("Vector3", "LinearColor"), f"unexpected type: {result.get('valueType')}"
            self._record(name, True, f"type={result.get('valueType')}", (time.time() - t0) * 1000.0)
        except Exception as exc:  # noqa: BLE001
            self._record(name, False, str(exc), (time.time() - t0) * 1000.0)

    def _cleanup(self) -> None:
        _log("Cleaning up test actors and assets...")
        for actor_name in self.created_actors:
            try:
                mcp_call("destroy_actor", {"name": actor_name}, timeout=10.0)
            except Exception as exc:  # noqa: BLE001
                _log_warning(f"Failed to destroy actor {actor_name}: {exc}")

        for asset_path in self.created_assets:
            try:
                mcp_call("delete_asset", {"path": asset_path}, timeout=10.0)
            except Exception as exc:  # noqa: BLE001
                _log_warning(f"Failed to delete asset {asset_path}: {exc}")

        for temp_file in self.temp_files:
            try:
                temp_file.unlink(missing_ok=True)
            except OSError:
                pass


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------
def main() -> Dict[str, Any]:
    _log("=" * 60)
    _log("UnrealMCP VFX Integration Tests")
    _log("=" * 60)
    summary = IntegrationTestRunner().run()
    _log("=" * 60)
    _log(json.dumps(summary, indent=2, ensure_ascii=False))
    _log("=" * 60)
    return summary


if __name__ == "__main__":
    main()
