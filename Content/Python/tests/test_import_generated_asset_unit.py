"""Lightweight unit tests for import_generated_asset helpers.

This module mocks the ``unreal`` module so the tests can run in a plain
Python environment (CI, pre-commit, etc.) without the editor.

Run from the project root:
    python Content/Python/tests/test_import_generated_asset_unit.py
"""

from __future__ import annotations

import json
import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import MagicMock, patch

# Inject a mock ``unreal`` module before importing the script under test.
_unreal_mock = MagicMock()
_unreal_mock.log = print
_unreal_mock.log_warning = print
_unreal_mock.log_error = print
_unreal_mock.Vector = lambda x, y, z: (x, y, z)
sys.modules["unreal"] = _unreal_mock

# Allow the test to be run from the project root without installing the script.
_CONTENT_PYTHON = os.path.normpath(
    os.path.join(os.path.dirname(__file__), "..")
)
if _CONTENT_PYTHON not in sys.path:
    sys.path.insert(0, _CONTENT_PYTHON)

import import_generated_asset as iga


class SemanticMapParameterTests(unittest.TestCase):
    """Tests for the semantic map -> material parameter name mapping."""

    def test_common_semantics_are_mapped(self) -> None:
        self.assertEqual(iga._semantic_to_param_name("BaseColor"), "BaseColor")
        self.assertEqual(iga._semantic_to_param_name("basecolor"), "BaseColor")
        self.assertEqual(iga._semantic_to_param_name("Normal"), "Normal")
        self.assertEqual(iga._semantic_to_param_name("RoughnessMap"), "Roughness")
        self.assertEqual(iga._semantic_to_param_name("metallic"), "Metallic")
        self.assertEqual(iga._semantic_to_param_name("Opacity"), "Opacity")
        self.assertEqual(iga._semantic_to_param_name("EmissiveColor"), "Emissive")
        self.assertEqual(iga._semantic_to_param_name("OpacityMask"), "OpacityMask")

    def test_unknown_semantic_passed_through(self) -> None:
        self.assertEqual(iga._semantic_to_param_name("CustomUserParam"), "CustomUserParam")


class SupportedMeshFormatTests(unittest.TestCase):
    """Tests for mesh format validation."""

    def test_supported_extensions(self) -> None:
        for ext in (".fbx", ".obj", ".glb", ".gltf"):
            self.assertIn(ext, iga.SUPPORTED_MESH_EXTENSIONS)

    @patch("import_generated_asset.os.path.isfile", return_value=True)
    def test_unsupported_extension_raises(self, _mock_isfile: object) -> None:
        with self.assertRaises(ValueError):
            iga.import_mesh("/fake/path/mesh.unknown")


class FileNotFoundTests(unittest.TestCase):
    """Tests for input path validation."""

    def test_missing_mesh_file_raises(self) -> None:
        with self.assertRaises(FileNotFoundError):
            iga.import_mesh("/nonexistent/path/mesh.glb")


class CreateMaterialCliTests(unittest.TestCase):
    """Tests for the create_material CLI path (params-file / result-file)."""

    @patch("import_generated_asset.create_material_instance_from_maps", return_value="/Game/Generated/Materials/MI_Test.MI_Test")
    def test_params_file_and_result_file(self, mock_create: object) -> None:
        params = {
            "name": "MI_Test",
            "parent_path": "/Game/Materials/Generated/M_Generated_Opaque",
            "maps": {"BaseColor": "/Game/Generated/Textures/T_Test"},
            "scalar_parameters": {"Roughness": 0.8},
            "vector_parameters": {"EmissiveColor": [1.0, 0.5, 0.0]},
            "reuse": False,
        }
        with tempfile.TemporaryDirectory() as tmpdir:
            params_file = os.path.join(tmpdir, "params.json")
            result_file = os.path.join(tmpdir, "result.json")
            Path(params_file).write_text(json.dumps(params), encoding="utf-8")

            argv = [
                "import_generated_asset.py",
                "create_material",
                "--params-file", params_file,
                "--result-file", result_file,
            ]
            with patch.object(sys, "argv", argv):
                iga._main_cli()

            result = json.loads(Path(result_file).read_text(encoding="utf-8"))
            self.assertTrue(result["success"], result.get("error"))
            self.assertEqual(result["path"], "/Game/Generated/Materials/MI_Test.MI_Test")
            self.assertEqual(result["parent_path"], params["parent_path"])
            self.assertEqual(result["maps"], params["maps"])

            mock_create.assert_called_once()
            args, kwargs = mock_create.call_args
            self.assertEqual(args, ("MI_Test", params["parent_path"], params["maps"], "/Game/Generated/Materials"))
            self.assertEqual(kwargs.get("scalar_parameters"), params["scalar_parameters"])
            self.assertEqual(kwargs.get("vector_parameters"), params["vector_parameters"])
            self.assertEqual(kwargs.get("reuse"), False)

    @patch("import_generated_asset.create_material_instance_from_maps", return_value="/Game/Generated/Materials/MI_Direct.MI_Direct")
    def test_direct_cli_arguments(self, mock_create: object) -> None:
        with tempfile.TemporaryDirectory() as tmpdir:
            result_file = os.path.join(tmpdir, "result.json")
            argv = [
                "import_generated_asset.py",
                "create_material",
                "MI_Direct",
                "/Game/Materials/Generated/M_Generated_Opaque",
                "--maps", '{"BaseColor":"/Game/Tex"}',
                "--scalar-params", '{"Roughness":0.5}',
                "--vector-params", '[0.0,1.0,0.0]',
                "--reuse", "false",
                "--result-file", result_file,
            ]
            with patch.object(sys, "argv", argv):
                iga._main_cli()

            result = json.loads(Path(result_file).read_text(encoding="utf-8"))
            self.assertTrue(result["success"], result.get("error"))
            self.assertEqual(result["path"], "/Game/Generated/Materials/MI_Direct.MI_Direct")

    @patch("import_generated_asset.create_material_instance_from_maps", side_effect=ValueError("boom"))
    def test_error_written_to_result_file(self, _mock_create: object) -> None:
        with tempfile.TemporaryDirectory() as tmpdir:
            result_file = os.path.join(tmpdir, "result.json")
            argv = [
                "import_generated_asset.py",
                "create_material",
                "MI_Bad",
                "/Game/Materials/Generated/M_Generated_Opaque",
                "--maps", '{"BaseColor":"/Game/Tex"}',
                "--result-file", result_file,
            ]
            with patch.object(sys, "argv", argv):
                iga._main_cli()

            result = json.loads(Path(result_file).read_text(encoding="utf-8"))
            self.assertFalse(result["success"])
            self.assertIn("boom", result["error"])


if __name__ == "__main__":
    unittest.main()
