#!/usr/bin/env python3
"""
Visual feedback screenshot evaluation framework for the AI VFX pipeline.

This script analyses a UE viewport screenshot and produces a structured report.
It is intentionally dependency-light: only Pillow is required for the built-in
pixel-level metrics. Optional vision-model backends can be plugged in via the
`VisionEvaluator` interface.

Usage:
    python scripts/evaluate_screenshot.py \
        D:/Playground/TA-Playground/Saved/Screenshots/aivfx_rock_abc123.png \
        --reference-color 1.0 0.2 0.0 \
        --output report.json

Exit codes:
    0 - evaluation completed (regardless of pass/fail)
    1 - runtime / configuration error

Environment variables:
    OPENAI_API_KEY - enables the optional OpenAI vision evaluator
    OPENAI_MODEL   - vision model name (default: gpt-4o-mini)
"""

from __future__ import annotations

import argparse
import base64
import json
import os
import sys
from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Dict, List, Optional, Sequence, Tuple

from PIL import Image


# ---------------------------------------------------------------------------
# Data model
# ---------------------------------------------------------------------------
@dataclass
class EvaluationReport:
    """Container for all metrics and the final pass/fail verdict."""

    image_path: str
    width: int
    height: int
    brightness: float = 0.0
    non_black_ratio: float = 0.0
    center_distance: float = 0.0
    dominant_hue: Optional[float] = None
    reference_color_distance: Optional[float] = None
    ssim_score: Optional[float] = None
    vision_feedback: Optional[str] = None
    checks: List[Dict[str, Any]] = field(default_factory=list)
    passed: bool = False

    def to_dict(self) -> Dict[str, Any]:
        return {
            "imagePath": self.image_path,
            "resolution": {"width": self.width, "height": self.height},
            "brightness": round(self.brightness, 4),
            "nonBlackRatio": round(self.non_black_ratio, 4),
            "centerDistance": round(self.center_distance, 4),
            "dominantHue": None if self.dominant_hue is None else round(self.dominant_hue, 2),
            "referenceColorDistance": (
                None if self.reference_color_distance is None else round(self.reference_color_distance, 4)
            ),
            "ssimScore": self.ssim_score,
            "visionFeedback": self.vision_feedback,
            "checks": self.checks,
            "passed": self.passed,
        }


# ---------------------------------------------------------------------------
# Pixel-level metrics
# ---------------------------------------------------------------------------
def _rgb_to_hsv(r: int, g: int, b: int) -> Tuple[float, float, float]:
    """Convert an RGB triple to HSV in the range (0..360, 0..1, 0..1)."""
    rn, gn, bn = r / 255.0, g / 255.0, b / 255.0
    mx = max(rn, gn, bn)
    mn = min(rn, gn, bn)
    diff = mx - mn

    if diff == 0:
        h = 0.0
    elif mx == rn:
        h = (60.0 * ((gn - bn) / diff) + 360.0) % 360.0
    elif mx == gn:
        h = (60.0 * ((bn - rn) / diff) + 120.0) % 360.0
    else:
        h = (60.0 * ((rn - gn) / diff) + 240.0) % 360.0

    s = 0.0 if mx == 0 else diff / mx
    v = mx
    return h, s, v


def compute_pixel_metrics(image: Image.Image) -> Dict[str, Any]:
    """Compute brightness, coverage, center-of-mass and dominant hue."""
    rgb = image.convert("RGB")
    width, height = rgb.size
    raw = rgb.tobytes()
    total = width * height
    if total == 0:
        return {
            "brightness": 0.0,
            "non_black_ratio": 0.0,
            "center_distance": 0.0,
            "dominant_hue": None,
        }

    # Reconstruct RGB triples from raw bytes.
    pixels = list(zip(raw[0::3], raw[1::3], raw[2::3]))

    # Brightness: mean value in HSV.
    values = [max(p) / 255.0 for p in pixels]
    brightness = sum(values) / total

    # Coverage: fraction of pixels that are not essentially black.
    non_black = [v for v in values if v > 0.05]
    non_black_ratio = len(non_black) / total

    # Center of mass of non-dark pixels, normalised to image space.
    cx, cy, mass = 0.0, 0.0, 0.0
    for y in range(height):
        row_offset = y * width
        for x in range(width):
            v = values[row_offset + x]
            if v > 0.05:
                cx += x * v
                cy += y * v
                mass += v

    if mass > 0:
        cx /= mass
        cy /= mass
        image_center_x = (width - 1) / 2.0
        image_center_y = (height - 1) / 2.0
        max_dist = ((width / 2.0) ** 2 + (height / 2.0) ** 2) ** 0.5
        center_distance = ((cx - image_center_x) ** 2 + (cy - image_center_y) ** 2) ** 0.5 / max_dist
    else:
        center_distance = 1.0

    # Dominant hue: weighted by saturation and value so greys do not dominate.
    hue_votes: Dict[int, float] = {}
    for p, v in zip(pixels, values):
        if v <= 0.05:
            continue
        h, s, _ = _rgb_to_hsv(*p)
        weight = v * s
        bucket = int(h / 10.0) * 10  # 36 buckets
        hue_votes[bucket] = hue_votes.get(bucket, 0.0) + weight

    dominant_hue = max(hue_votes, key=hue_votes.get) if hue_votes else None

    return {
        "brightness": brightness,
        "non_black_ratio": non_black_ratio,
        "center_distance": center_distance,
        "dominant_hue": dominant_hue,
    }


def color_distance(rgb_a: Sequence[float], rgb_b: Sequence[float]) -> float:
    """Normalised Euclidean distance between two RGB colours in [0,1]^3."""
    if len(rgb_a) < 3 or len(rgb_b) < 3:
        raise ValueError("RGB inputs must have at least 3 components")
    dr = rgb_a[0] - rgb_b[0]
    dg = rgb_a[1] - rgb_b[1]
    db = rgb_a[2] - rgb_b[2]
    return (dr * dr + dg * dg + db * db) ** 0.5 / (3.0 ** 0.5)


def compute_ssim(image_a: Image.Image, image_b: Image.Image) -> Optional[float]:
    """Compute SSIM when scikit-image is available; otherwise return None."""
    try:
        import numpy as np
        from skimage.metrics import structural_similarity as ssim
    except Exception:  # noqa: BLE001
        return None

    a = np.array(image_a.convert("L"))
    b = np.array(image_b.convert("L"))
    if a.shape != b.shape:
        b = np.array(image_b.convert("L").resize(image_a.size, Image.Resampling.LANCZOS))
    score, _ = ssim(a, b, full=True)
    return float(score)


# ---------------------------------------------------------------------------
# Pluggable vision-model evaluator
# ---------------------------------------------------------------------------
class VisionEvaluator(ABC):
    """Abstract backend for optional vision-model-based feedback."""

    @abstractmethod
    def evaluate(self, image_path: Path, prompt: str) -> Optional[str]:
        """Return a textual assessment or None if unavailable."""


class OpenAIVisionEvaluator(VisionEvaluator):
    """Example backend using OpenAI's vision API. Disabled without an API key."""

    def __init__(self, api_key: Optional[str] = None, model: Optional[str] = None):
        self.api_key = api_key or os.environ.get("OPENAI_API_KEY")
        self.model = model or os.environ.get("OPENAI_MODEL", "gpt-4o-mini")

    def _encode_image(self, image_path: Path) -> str:
        with open(image_path, "rb") as f:
            return base64.b64encode(f.read()).decode("utf-8")

    def evaluate(self, image_path: Path, prompt: str) -> Optional[str]:
        if not self.api_key:
            return None
        try:
            import requests
        except ImportError:
            return None

        b64 = self._encode_image(image_path)
        headers = {"Authorization": f"Bearer {self.api_key}", "Content-Type": "application/json"}
        payload = {
            "model": self.model,
            "messages": [
                {
                    "role": "user",
                    "content": [
                        {"type": "text", "text": prompt},
                        {
                            "type": "image_url",
                            "image_url": {"url": f"data:image/png;base64,{b64}"},
                        },
                    ],
                }
            ],
            "max_tokens": 256,
        }
        try:
            response = requests.post(
                "https://api.openai.com/v1/chat/completions",
                headers=headers,
                json=payload,
                timeout=60.0,
            )
            response.raise_for_status()
            return response.json()["choices"][0]["message"]["content"]
        except Exception as exc:  # noqa: BLE001
            return f"Vision API error: {exc}"


# ---------------------------------------------------------------------------
# Core evaluation pipeline
# ---------------------------------------------------------------------------
def evaluate_screenshot(
    image_path: Path,
    reference_color: Optional[Sequence[float]] = None,
    reference_image: Optional[Path] = None,
    vision_evaluator: Optional[VisionEvaluator] = None,
    vision_prompt: Optional[str] = None,
    brightness_range: Tuple[float, float] = (0.05, 0.95),
    max_center_distance: float = 0.35,
    min_non_black_ratio: float = 0.01,
    max_reference_color_distance: Optional[float] = None,
    min_ssim: Optional[float] = None,
) -> EvaluationReport:
    """Evaluate a single screenshot and return a populated report."""

    image = Image.open(image_path)
    report = EvaluationReport(
        image_path=str(image_path.resolve()),
        width=image.width,
        height=image.height,
    )

    metrics = compute_pixel_metrics(image)
    report.brightness = metrics["brightness"]
    report.non_black_ratio = metrics["non_black_ratio"]
    report.center_distance = metrics["center_distance"]
    report.dominant_hue = metrics["dominant_hue"]

    # --- Built-in checks ----------------------------------------------------
    report.checks.append(
        {
            "name": "brightness_in_range",
            "passed": brightness_range[0] <= report.brightness <= brightness_range[1],
            "value": report.brightness,
            "threshold": brightness_range,
        }
    )
    report.checks.append(
        {
            "name": "object_on_screen",
            "passed": report.non_black_ratio >= min_non_black_ratio,
            "value": report.non_black_ratio,
            "threshold": min_non_black_ratio,
        }
    )
    report.checks.append(
        {
            "name": "object_centered",
            "passed": report.center_distance <= max_center_distance,
            "value": report.center_distance,
            "threshold": max_center_distance,
        }
    )

    # --- Reference colour check ---------------------------------------------
    if reference_color is not None:
        # Compute mean colour of non-dark pixels.
        rgb = image.convert("RGB")
        raw = rgb.tobytes()
        pixels = list(zip(raw[0::3], raw[1::3], raw[2::3]))
        values = [max(p) / 255.0 for p in pixels]
        r_sum = g_sum = b_sum = mass = 0.0
        for p, v in zip(pixels, values):
            if v > 0.05:
                r_sum += p[0] / 255.0 * v
                g_sum += p[1] / 255.0 * v
                b_sum += p[2] / 255.0 * v
                mass += v
        if mass > 0:
            mean_color = (r_sum / mass, g_sum / mass, b_sum / mass)
            report.reference_color_distance = color_distance(mean_color, reference_color)
            threshold = max_reference_color_distance if max_reference_color_distance is not None else 0.5
            report.checks.append(
                {
                    "name": "reference_color_match",
                    "passed": report.reference_color_distance <= threshold,
                    "value": report.reference_color_distance,
                    "threshold": threshold,
                }
            )

    # --- Reference image SSIM check -----------------------------------------
    if reference_image is not None:
        ref = Image.open(reference_image)
        report.ssim_score = compute_ssim(image, ref)
        if report.ssim_score is not None:
            threshold = min_ssim if min_ssim is not None else 0.3
            report.checks.append(
                {
                    "name": "ssim_vs_reference",
                    "passed": report.ssim_score >= threshold,
                    "value": report.ssim_score,
                    "threshold": threshold,
                }
            )

    # --- Optional vision-model feedback -------------------------------------
    if vision_evaluator is not None:
        prompt = vision_prompt or (
            "Assess this Unreal Engine screenshot. Is the generated asset visible, "
            "reasonably centred, and does the overall colour scheme match a fantasy "
            "VFX prop? Reply in one short paragraph."
        )
        report.vision_feedback = vision_evaluator.evaluate(image_path, prompt)

    report.passed = all(c["passed"] for c in report.checks)
    return report


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------
def _parse_color(value: str) -> Tuple[float, float, float]:
    parts = [float(x) for x in value.split(",")]
    if len(parts) != 3:
        raise argparse.ArgumentTypeError("colour must be r,g,b in [0,1]")
    return tuple(parts)  # type: ignore[return-value]


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Evaluate a UE viewport screenshot for the AI VFX pipeline"
    )
    parser.add_argument("image", type=Path, help="Path to the screenshot PNG/JPG")
    parser.add_argument(
        "--reference-color",
        type=_parse_color,
        default=None,
        help="Expected dominant colour as r,g,b in [0,1] (e.g. 1.0,0.2,0.0)",
    )
    parser.add_argument(
        "--reference-image",
        type=Path,
        default=None,
        help="Reference image for SSIM comparison",
    )
    parser.add_argument("--min-brightness", type=float, default=0.05)
    parser.add_argument("--max-brightness", type=float, default=0.95)
    parser.add_argument("--max-center-distance", type=float, default=0.35)
    parser.add_argument("--min-non-black-ratio", type=float, default=0.01)
    parser.add_argument("--max-color-distance", type=float, default=None)
    parser.add_argument("--min-ssim", type=float, default=None)
    parser.add_argument("--vision", action="store_true", help="Enable OpenAI vision evaluator if API key is set")
    parser.add_argument("--vision-prompt", type=str, default=None)
    parser.add_argument("--output", type=Path, default=None, help="Write JSON report to this file")
    parser.add_argument("--quiet", action="store_true", help="Only output the JSON report")
    return parser


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    if not args.image.exists():
        print(f"Image not found: {args.image}", file=sys.stderr)
        return 1

    vision_evaluator: Optional[VisionEvaluator] = None
    if args.vision:
        vision_evaluator = OpenAIVisionEvaluator()

    try:
        report = evaluate_screenshot(
            image_path=args.image,
            reference_color=args.reference_color,
            reference_image=args.reference_image,
            vision_evaluator=vision_evaluator,
            vision_prompt=args.vision_prompt,
            brightness_range=(args.min_brightness, args.max_brightness),
            max_center_distance=args.max_center_distance,
            min_non_black_ratio=args.min_non_black_ratio,
            max_reference_color_distance=args.max_color_distance,
            min_ssim=args.min_ssim,
        )
    except Exception as exc:  # noqa: BLE001
        print(f"Evaluation failed: {exc}", file=sys.stderr)
        return 1

    report_dict = report.to_dict()
    json_text = json.dumps(report_dict, indent=2, ensure_ascii=False)

    if args.output:
        args.output.write_text(json_text, encoding="utf-8")

    if args.quiet:
        print(json_text)
    else:
        print(f"Screenshot: {report.image_path}")
        print(f"Resolution: {report.width}x{report.height}")
        print(f"Brightness: {report.brightness:.3f}")
        print(f"Non-black ratio: {report.non_black_ratio:.3f}")
        print(f"Center distance: {report.center_distance:.3f}")
        print(f"Dominant hue: {report.dominant_hue}")
        if report.reference_color_distance is not None:
            print(f"Reference colour distance: {report.reference_color_distance:.3f}")
        if report.ssim_score is not None:
            print(f"SSIM vs reference: {report.ssim_score:.3f}")
        if report.vision_feedback:
            print(f"Vision feedback: {report.vision_feedback}")
        print("Checks:")
        for check in report.checks:
            status = "PASS" if check["passed"] else "FAIL"
            print(f"  [{status}] {check['name']}: {check['value']} (threshold {check['threshold']})")
        print(f"Overall: {'PASS' if report.passed else 'FAIL'}")
        if args.output:
            print(f"Report written to: {args.output}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
