"""
Unit tests for hunyuan/vfx_generation_service.py.

These tests mock the Hunyuan3D API server and the filesystem so they can run
without GPU, model weights, or a real server process.
"""

import base64
import io
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from vfx_generation_service import (
    VfxGenerationService,
    _extract_texture_preview,
    _image_to_base64,
    _write_placeholder_preview,
)


def _make_dummy_glb_bytes() -> bytes:
    """Return a minimal GLB header-like byte sequence (not a valid scene)."""
    return b"glTF" + b"\x00" * 20


def _make_dummy_png_bytes() -> bytes:
    buffer = io.BytesIO()
    Image.new("RGBA", (8, 8), (255, 0, 0, 255)).save(buffer, format="PNG")
    return buffer.getvalue()


class TestImageEncoding(unittest.TestCase):
    def test_image_to_base64_roundtrip(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "input.png"
            Image.new("RGBA", (16, 16), (0, 255, 0, 255)).save(path)
            encoded = _image_to_base64(str(path))
            decoded = base64.b64decode(encoded)
            self.assertTrue(decoded.startswith(b"\x89PNG"))

    def test_image_to_base64_missing_file(self):
        with self.assertRaises(FileNotFoundError):
            _image_to_base64("/nonexistent/image.png")


class TestPreviewExtraction(unittest.TestCase):
    def test_placeholder_written_when_glb_invalid(self):
        with tempfile.TemporaryDirectory() as tmp:
            glb_path = Path(tmp) / "mesh.glb"
            png_path = Path(tmp) / "mesh.png"
            glb_path.write_bytes(b"not a glb")
            result = _extract_texture_preview(str(glb_path), str(png_path))
            self.assertFalse(result)
            self.assertTrue(png_path.exists())
            with Image.open(png_path) as img:
                self.assertEqual(img.size, (512, 512))

    def test_write_placeholder_preview(self):
        with tempfile.TemporaryDirectory() as tmp:
            png_path = Path(tmp) / "preview.png"
            _write_placeholder_preview(str(png_path))
            self.assertTrue(png_path.exists())


class TestServerLifecycle(unittest.TestCase):
    def test_is_server_running_true(self):
        service = VfxGenerationService()
        with mock.patch("vfx_generation_service.requests.get") as mock_get:
            mock_get.return_value = mock.Mock(status_code=200, json=lambda: {"status": "healthy"})
            self.assertTrue(service.is_server_running())
            mock_get.assert_called_once_with(f"{service.base_url}/health", timeout=5)

    def test_is_server_running_false_on_bad_status(self):
        service = VfxGenerationService()
        with mock.patch("vfx_generation_service.requests.get") as mock_get:
            mock_get.return_value = mock.Mock(status_code=503, json=lambda: {"status": "unhealthy"})
            self.assertFalse(service.is_server_running())

    def test_is_server_running_false_on_connection_error(self):
        service = VfxGenerationService()
        with mock.patch("vfx_generation_service.requests.get", side_effect=Exception("connection refused")):
            self.assertFalse(service.is_server_running())

    def test_start_server_builds_command(self):
        with tempfile.TemporaryDirectory() as tmp:
            script_path = Path(tmp) / "api_server.py"
            script_path.write_text("# dummy")
            service = VfxGenerationService(
                api_server_script=str(script_path),
                python_executable="/fake/python",
                server_kwargs={"model_path": "tencent/Hunyuan3D-2.1", "low_vram_mode": True, "port": 9000},
            )
            with mock.patch("vfx_generation_service.subprocess.Popen") as mock_popen:
                mock_popen.return_value = mock.Mock(pid=1234, poll=lambda: None)
                process = service.start_server()
                self.assertEqual(process.pid, 1234)
                called_args = mock_popen.call_args[0][0]
                self.assertEqual(called_args[0], "/fake/python")
                self.assertEqual(called_args[1], str(script_path))
                self.assertIn("--model-path", called_args)
                self.assertIn("tencent/Hunyuan3D-2.1", called_args)
                self.assertIn("--low-vram-mode", called_args)
                self.assertIn("--port", called_args)
                self.assertIn("9000", called_args)

    def test_start_server_missing_script(self):
        service = VfxGenerationService(api_server_script="/does/not/exist.py")
        with self.assertRaises(FileNotFoundError):
            service.start_server()

    def test_stop_server_terminates_process(self):
        service = VfxGenerationService()
        proc = mock.Mock(pid=1234, poll=lambda: None)
        proc.wait = mock.Mock(return_value=None)
        service._server_process = proc
        service.stop_server()
        proc.terminate.assert_called_once()
        proc.wait.assert_called_once_with(timeout=30)

    def test_stop_server_kills_on_timeout(self):
        service = VfxGenerationService()
        proc = mock.Mock(pid=1234, poll=lambda: None)
        proc.wait = mock.Mock(side_effect=[subprocess.TimeoutExpired("wait", 30), None])
        service._server_process = proc
        service.stop_server()
        proc.kill.assert_called_once()


class TestGenerateFromImage(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.output_root = Path(self.tmp.name) / "output"
        self.input_image = Path(self.tmp.name) / "input.png"
        Image.new("RGBA", (32, 32), (0, 0, 255, 255)).save(self.input_image)

        self.service = VfxGenerationService(
            output_root=str(self.output_root),
            poll_interval=0.01,
            timeout=1.0,
            server_kwargs={"model_path": "tencent/Hunyuan3D-2.1"},
        )

    def _mock_status_completed(self):
        glb_bytes = _make_dummy_glb_bytes()
        return mock.Mock(
            status_code=200,
            json=lambda: {"status": "completed", "model_base64": base64.b64encode(glb_bytes).decode()},
        )

    @mock.patch("vfx_generation_service._extract_texture_preview")
    @mock.patch("vfx_generation_service.requests.get")
    @mock.patch("vfx_generation_service.requests.post")
    def test_success_path(self, mock_post, mock_get, mock_extract):
        mock_post.return_value = mock.Mock(status_code=200, json=lambda: {"uid": "test-uid"})
        mock_get.return_value = self._mock_status_completed()

        result = self.service.generate_from_image(str(self.input_image), auto_start_server=False)

        self.assertEqual(result["uid"], "test-uid")
        self.assertTrue(Path(result["mesh_path"]).exists())
        mock_post.assert_called_once()
        payload = mock_post.call_args[1]["json"]
        self.assertIn("image", payload)
        self.assertTrue(payload["texture"])
        self.assertEqual(payload["seed"], 1234)

    @mock.patch("vfx_generation_service._extract_texture_preview")
    @mock.patch("vfx_generation_service.requests.get")
    @mock.patch("vfx_generation_service.requests.post")
    def test_output_metadata(self, mock_post, mock_get, mock_extract):
        mock_post.return_value = mock.Mock(status_code=200, json=lambda: {"uid": "meta-uid"})
        mock_get.return_value = self._mock_status_completed()

        result = self.service.generate_from_image(
            str(self.input_image),
            params={"seed": 42, "num_inference_steps": 10, "prompt": "a magic crystal"},
            auto_start_server=False,
        )

        meta_path = Path(result["meta_path"])
        self.assertTrue(meta_path.exists())
        meta = json.loads(meta_path.read_text())
        self.assertEqual(meta["uid"], "meta-uid")
        self.assertEqual(meta["seed"], 42)
        self.assertEqual(meta["steps"], 10)
        self.assertEqual(meta["prompt"], "a magic crystal")
        self.assertEqual(meta["model_version"], "tencent/Hunyuan3D-2.1")

    @mock.patch("vfx_generation_service.VfxGenerationService.start_server")
    @mock.patch("vfx_generation_service.VfxGenerationService.is_server_running")
    @mock.patch("vfx_generation_service._extract_texture_preview")
    @mock.patch("vfx_generation_service.requests.get")
    @mock.patch("vfx_generation_service.requests.post")
    def test_auto_starts_server(
        self, mock_post, mock_get, mock_extract, mock_running, mock_start
    ):
        mock_running.side_effect = [False, True]
        mock_post.return_value = mock.Mock(status_code=200, json=lambda: {"uid": "auto-uid"})
        mock_get.return_value = self._mock_status_completed()

        self.service.generate_from_image(str(self.input_image), auto_start_server=True)

        mock_start.assert_called_once()

    @mock.patch("vfx_generation_service._extract_texture_preview")
    @mock.patch("vfx_generation_service.requests.get")
    @mock.patch("vfx_generation_service.requests.post")
    def test_retries_transient_send_failure(self, mock_post, mock_get, mock_extract):
        mock_post.side_effect = [
            Exception("connection refused"),
            mock.Mock(status_code=200, json=lambda: {"uid": "retry-uid"}),
        ]
        mock_get.return_value = self._mock_status_completed()
        self.service.max_retries = 1

        result = self.service.generate_from_image(str(self.input_image), auto_start_server=False)
        self.assertEqual(result["uid"], "retry-uid")
        self.assertEqual(mock_post.call_count, 2)

    @mock.patch("vfx_generation_service.requests.get")
    @mock.patch("vfx_generation_service.requests.post")
    def test_send_failure_exceeds_retries(self, mock_post, mock_get):
        mock_post.side_effect = Exception("connection refused")
        self.service.max_retries = 0

        with self.assertRaises(RuntimeError):
            self.service.generate_from_image(str(self.input_image), auto_start_server=False)

    @mock.patch("vfx_generation_service.requests.get")
    @mock.patch("vfx_generation_service.requests.post")
    def test_status_error(self, mock_post, mock_get):
        mock_post.return_value = mock.Mock(status_code=200, json=lambda: {"uid": "err-uid"})
        mock_get.return_value = mock.Mock(
            status_code=200,
            json=lambda: {"status": "error", "message": "shape generation failed"},
        )

        with self.assertRaises(RuntimeError) as ctx:
            self.service.generate_from_image(str(self.input_image), auto_start_server=False)
        self.assertIn("shape generation failed", str(ctx.exception))

    @mock.patch("vfx_generation_service.requests.get")
    @mock.patch("vfx_generation_service.requests.post")
    def test_status_timeout(self, mock_post, mock_get):
        mock_post.return_value = mock.Mock(status_code=200, json=lambda: {"uid": "to-uid"})
        mock_get.return_value = mock.Mock(status_code=200, json=lambda: {"status": "processing"})
        self.service.timeout = 0.05
        self.service.poll_interval = 0.01

        with self.assertRaises(TimeoutError):
            self.service.generate_from_image(str(self.input_image), auto_start_server=False)


class TestGenerateFromText(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.service = VfxGenerationService(
            output_root=str(Path(self.tmp.name) / "output"),
            poll_interval=0.01,
            timeout=1.0,
        )
        self.input_image = Path(self.tmp.name) / "input.png"
        Image.new("RGBA", (32, 32), (255, 255, 0, 255)).save(self.input_image)

    def test_raises_without_image_generator(self):
        with self.assertRaises(NotImplementedError):
            self.service.generate_from_text("a fireball", auto_start_server=False)

    @mock.patch("vfx_generation_service._extract_texture_preview")
    @mock.patch("vfx_generation_service.requests.get")
    @mock.patch("vfx_generation_service.requests.post")
    def test_uses_image_generator(self, mock_post, mock_get, mock_extract):
        mock_post.return_value = mock.Mock(status_code=200, json=lambda: {"uid": "txt-uid"})
        glb_bytes = _make_dummy_glb_bytes()
        mock_get.return_value = mock.Mock(
            status_code=200,
            json=lambda: {"status": "completed", "model_base64": base64.b64encode(glb_bytes).decode()},
        )

        def make_image(prompt: str) -> str:
            self.assertEqual(prompt, "a glowing crystal")
            return str(self.input_image)

        result = self.service.generate_from_text(
            "a glowing crystal",
            params={"seed": 7},
            image_generator=make_image,
            auto_start_server=False,
        )
        self.assertEqual(result["uid"], "txt-uid")


if __name__ == "__main__":
    unittest.main()
