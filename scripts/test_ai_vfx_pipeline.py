"""
Mock-based tests for scripts/ai_vfx_pipeline.py.

These tests do not require a running Unreal Editor or Hunyuan3D service.
They verify the orchestration logic by recording every MCP call made by
AIVfxPipeline and returning canned responses.
"""

from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent))

from ai_vfx_pipeline import (
    AIVfxPipeline,
    AIVfxPipelineError,
    DEFAULT_PARENT_MATERIALS,
    DEFAULT_TEMPLATES,
    _canonical_object_path,
    _classify_texture,
    _sanitize,
    build_parser,
    main,
)


class FakeMCPClient:
    """Record-keeping fake for UnrealMCPClient."""

    def __init__(self, responses: Optional[Dict[str, List[Dict[str, Any]]]] = None):
        self.calls: List[Tuple[str, Optional[Dict[str, Any]], Optional[float]]] = []
        self.responses = responses or {}

    def call(
        self,
        method: str,
        params: Optional[Dict[str, Any]] = None,
        timeout: Optional[float] = None,
    ) -> Dict[str, Any]:
        self.calls.append((method, params, timeout))
        queue = self.responses.get(method)
        if queue:
            return queue.pop(0)
        return {"success": True, "result": {}}


def _completed_generation_result(**overrides: Any) -> Dict[str, Any]:
    base = {
        "success": True,
        "result": {
            "assetPath": "/Game/Generated/Meshes/SM_Rock.SM_Rock",
            "actorName": "AIGenerated_Rock_abc123",
            "meshFile": "D:/Playground/TA-Playground/hunyuan/output/gen-xxx/mesh.glb",
            "location": [0, 0, 0],
            "rotation": [0, 0, 0],
            "scale": [1, 1, 1],
            "status": "completed",
        },
    }
    base["result"].update(overrides)
    return base


class TestUtilities(unittest.TestCase):
    def test_sanitize(self):
        self.assertEqual(_sanitize("a glowing magic crystal"), "a_glowing_magic_crystal")
        self.assertEqual(_sanitize("Fire!!! Burst 123"), "Fire_Burst_123")
        self.assertEqual(_sanitize("   "), "Asset")

    def test_classify_texture(self):
        self.assertEqual(_classify_texture("T_Rock_Diffuse"), "BaseColor")
        self.assertEqual(_classify_texture("rock_normal"), "Normal")
        self.assertEqual(_classify_texture("ORM_roughness"), "Roughness")
        self.assertEqual(_classify_texture("T_Emissive_Mask"), "Emissive")
        self.assertIsNone(_classify_texture("unknown"))

    def test_canonical_object_path(self):
        self.assertEqual(
            _canonical_object_path("/Game/Materials/MI_Foo"),
            "/Game/Materials/MI_Foo.MI_Foo",
        )
        self.assertEqual(
            _canonical_object_path("/Game/Materials/MI_Foo.MI_Foo"),
            "/Game/Materials/MI_Foo.MI_Foo",
        )


class TestAIVfxPipeline(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.ref_image = Path(self.tmp.name) / "ref.png"
        self.ref_image.write_bytes(b"fake png")

    def _make_pipeline(
        self,
        responses: Optional[Dict[str, List[Dict[str, Any]]]] = None,
    ) -> Tuple[AIVfxPipeline, FakeMCPClient]:
        fake = FakeMCPClient(responses)
        pipeline = AIVfxPipeline(
            mcp_host="127.0.0.1",
            mcp_port=13377,
            generation_timeout=1.0,
            poll_interval=0.01,
            client=fake,
        )
        return pipeline, fake

    def test_full_run_with_material_and_screenshot(self):
        responses = {
            "generate_and_import_3d": [
                {
                    "success": True,
                    "result": {
                        "jobId": "gen-3d-test",
                        "status": "pending",
                        "message": "started",
                    },
                },
            ],
            "get_generate_and_import_3d_status": [
                {
                    "success": True,
                    "result": {
                        "jobId": "gen-3d-test",
                        "status": "running",
                        "progress": 0.5,
                        "assetPath": None,
                        "actorName": None,
                    },
                },
                _completed_generation_result(),
            ],
            "get_asset_list": [
                {
                    "success": True,
                    "result": {
                        "assets": [
                            {
                                "name": "mesh",
                                "path": "/Game/Generated/Meshes/mesh.mesh",
                                "class": "StaticMesh",
                            },
                            {
                                "name": "T_Rock_Diffuse",
                                "path": "/Game/Generated/Meshes/T_Rock_Diffuse.T_Rock_Diffuse",
                                "class": "Texture2D",
                            },
                            {
                                "name": "T_Rock_Normal",
                                "path": "/Game/Generated/Meshes/T_Rock_Normal.T_Rock_Normal",
                                "class": "Texture2D",
                            },
                        ],
                        "count": 3,
                    },
                },
            ],
            "create_material_from_textures": [
                {
                    "success": True,
                    "result": {
                        "path": "/Game/Generated/Materials/MI_Rock_abc123",
                        "parentPath": DEFAULT_PARENT_MATERIALS["opaque"],
                        "maps": {},
                        "reused": False,
                    },
                },
            ],
            "set_material": [{"success": True, "result": {"actor": "AIGenerated_Rock_abc123"}}],
            "focus_viewport": [{"success": True, "result": {"focused_at": "[0, 0, 150]"}}],
            "take_screenshot": [
                {
                    "success": True,
                    "result": {
                        "path": "D:/Playground/TA-Playground/Saved/Screenshots/aivfx_Rock_abc123.png",
                        "saved": True,
                    },
                },
            ],
            "spawn_actor": [{"success": True, "result": {}}] * 10,
            "set_light_parameters": [{"success": True, "result": {}}] * 10,
            "run_console_command": [{"success": True, "result": {"executed": True}}],
        }

        pipeline, fake = self._make_pipeline(responses)
        result = pipeline.run(
            prompt="a glowing rock",
            reference_image=str(self.ref_image),
            location=(0, 0, 150),
            spawn_effect=False,
        )

        self.assertTrue(result["success"])
        self.assertEqual(result["actorName"], "AIGenerated_Rock_abc123")
        self.assertEqual(result["assetPath"], "/Game/Generated/Meshes/SM_Rock.SM_Rock")
        self.assertEqual(result["materialPath"], "/Game/Generated/Materials/MI_Rock_abc123")
        self.assertEqual(result["location"], [0, 0, 150])

        gen_calls = [c for c in fake.calls if c[0] == "generate_and_import_3d"]
        self.assertEqual(len(gen_calls), 1)
        self.assertEqual(gen_calls[0][1]["destinationPath"], "/Game/Generated/Meshes")
        self.assertEqual(gen_calls[0][1]["referenceImage"], str(self.ref_image.resolve()))
        self.assertEqual(gen_calls[0][1]["waitForCompletion"], False)

        status_calls = [c for c in fake.calls if c[0] == "get_generate_and_import_3d_status"]
        self.assertEqual(len(status_calls), 2)

        mat_call = [c for c in fake.calls if c[0] == "create_material_from_textures"][0]
        self.assertEqual(mat_call[1]["parentPath"], DEFAULT_PARENT_MATERIALS["opaque"])
        self.assertEqual(
            mat_call[1]["maps"],
            {
                "BaseColor": "/Game/Generated/Meshes/T_Rock_Diffuse.T_Rock_Diffuse",
                "Normal": "/Game/Generated/Meshes/T_Rock_Normal.T_Rock_Normal",
            },
        )

        set_mat_call = [c for c in fake.calls if c[0] == "set_material"][0]
        self.assertEqual(
            set_mat_call[1]["materialPath"],
            "/Game/Generated/Materials/MI_Rock_abc123.MI_Rock_abc123",
        )

        screenshot_call = [c for c in fake.calls if c[0] == "take_screenshot"][0]
        self.assertTrue(screenshot_call[1]["filename"].startswith("aivfx_"))

    def test_run_with_niagara_effect(self):
        responses = {
            "generate_and_import_3d": [_completed_generation_result()],
            "get_asset_list": [{"success": True, "result": {"assets": [], "count": 0}}],
            "focus_viewport": [{"success": True, "result": {}}],
            "take_screenshot": [
                {
                    "success": True,
                    "result": {"path": "D:/Temp/screen.png", "saved": True},
                }
            ],
            "duplicate_niagara_system": [
                {
                    "success": True,
                    "result": {
                        "templatePath": DEFAULT_TEMPLATES["burst"],
                        "newPath": "/Game/VFX/Instances/NS_a_glowing_rock_abc123.NS_a_glowing_rock_abc123",
                        "actorName": "NS_a_glowing_rock_abc123_Actor",
                        "location": [0, 0, 150],
                        "parametersSet": ["User.Color"],
                    },
                }
            ],
            "set_niagara_parameter": [{"success": True, "result": {}}],
            "spawn_actor": [{"success": True, "result": {}}] * 10,
            "set_light_parameters": [{"success": True, "result": {}}] * 10,
            "run_console_command": [{"success": True, "result": {"executed": True}}],
        }

        pipeline, fake = self._make_pipeline(responses)
        result = pipeline.run(
            prompt="a glowing rock",
            reference_image=str(self.ref_image),
            spawn_effect=True,
            effect_template="burst",
            effect_params={"Color": [1.0, 0.2, 0.0], "Size": 2.5},
        )

        self.assertEqual(result["effectActorName"], "NS_a_glowing_rock_abc123_Actor")

        dup_call = [c for c in fake.calls if c[0] == "duplicate_niagara_system"][0]
        self.assertEqual(dup_call[1]["templatePath"], DEFAULT_TEMPLATES["burst"])
        self.assertTrue(dup_call[1]["spawnActor"])
        self.assertEqual(dup_call[1]["initialParameters"], {"User.Color": [1.0, 0.2, 0.0], "User.Size": 2.5})

        set_param_calls = [c for c in fake.calls if c[0] == "set_niagara_parameter"]
        self.assertEqual(len(set_param_calls), 2)
        names = {c[1]["parameterName"] for c in set_param_calls}
        self.assertEqual(names, {"User.Color", "User.Size"})

    def test_generation_failure_raises(self):
        responses = {
            "generate_and_import_3d": [
                {
                    "success": True,
                    "result": {
                        "jobId": "gen-3d-fail",
                        "status": "pending",
                    },
                }
            ],
            "get_generate_and_import_3d_status": [
                {
                    "success": True,
                    "result": {
                        "jobId": "gen-3d-fail",
                        "status": "failed",
                        "message": "GPU out of memory",
                    },
                }
            ],
        }
        pipeline, _ = self._make_pipeline(responses)
        with self.assertRaises(AIVfxPipelineError) as ctx:
            pipeline.run(prompt="a rock", reference_image=str(self.ref_image))
        self.assertIn("GPU out of memory", str(ctx.exception))

    def test_missing_reference_image_raises(self):
        pipeline, _ = self._make_pipeline()
        with self.assertRaises(AIVfxPipelineError) as ctx:
            pipeline.run(prompt="a rock")
        self.assertIn("reference_image is required", str(ctx.exception))

    def test_missing_image_file_raises(self):
        pipeline, _ = self._make_pipeline()
        with self.assertRaises(AIVfxPipelineError) as ctx:
            pipeline.run(prompt="a rock", reference_image="/does/not/exist.png")
        self.assertIn("Reference image not found", str(ctx.exception))


class TestCLI(unittest.TestCase):
    def test_parser_accepts_options(self):
        parser = build_parser()
        args = parser.parse_args(
            [
                "--prompt",
                "a fireball",
                "--ref-image",
                "D:/Temp/fire.png",
                "--location",
                "0,0,200",
                "--spawn-effect",
                "--effect-template",
                "impact",
                "--effect-params",
                '{"Color":[1,0.3,0]}',
                "--no-lights",
                "--no-screenshot",
                "--timeout",
                "120",
            ]
        )
        self.assertEqual(args.prompt, "a fireball")
        self.assertEqual(args.ref_image, "D:/Temp/fire.png")
        self.assertEqual(args.location, (0.0, 0.0, 200.0))
        self.assertTrue(args.spawn_effect)
        self.assertEqual(args.effect_template, "impact")
        self.assertEqual(args.effect_params, {"Color": [1, 0.3, 0]})
        self.assertTrue(args.no_lights)
        self.assertTrue(args.no_screenshot)
        self.assertEqual(args.timeout, 120.0)

    @mock.patch("ai_vfx_pipeline.AIVfxPipeline")
    def test_main_invokes_pipeline(self, mock_pipeline_cls):
        mock_pipeline = mock_pipeline_cls.return_value
        mock_pipeline.run.return_value = {"success": True, "actorName": "TestActor"}

        with mock.patch("builtins.print") as mock_print:
            rc = main(
                [
                    "--prompt",
                    "a crystal",
                    "--ref-image",
                    str(Path(__file__).resolve()),
                    "--location",
                    "0,0,100",
                    "--no-lights",
                ]
            )

        self.assertEqual(rc, 0)
        mock_pipeline_cls.assert_called_once()
        mock_pipeline.run.assert_called_once()
        run_kwargs = mock_pipeline.run.call_args.kwargs
        self.assertEqual(run_kwargs["prompt"], "a crystal")
        self.assertEqual(run_kwargs["location"], (0.0, 0.0, 100.0))
        self.assertFalse(run_kwargs["add_lights"])
        mock_print.assert_called_once()


if __name__ == "__main__":
    unittest.main()
