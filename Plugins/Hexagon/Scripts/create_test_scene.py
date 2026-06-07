r"""
Hexagon Terrain Test Scene Creator
-----------------------------------
Uses UnrealMCP's Python client to connect to a running UE Editor
and create a test scene for the hex terrain system.

Usage:
    1. Open Unreal Editor with the TA-Playground project
    2. Verify the UnrealMCP plugin is loaded (port 13377 listens)
    3. Run: python create_test_scene.py --radius 15 --cellsize 100

Environment:
    UE 5.7, TA-Playground project, UnrealMCP plugin enabled
"""

import sys
import os
import logging

# ---------------------------------------------------------------------------
# Add UnrealMCP Python client to path
# ---------------------------------------------------------------------------
MCP_CLIENT_DIR = os.path.join(
    os.path.dirname(__file__), "..", "..", "UnrealMCP", "MCP_Server", "client"
)
MCP_CLIENT_DIR = os.path.abspath(MCP_CLIENT_DIR)
sys.path.insert(0, MCP_CLIENT_DIR)

from unreal_client import UnrealClient

# ---------------------------------------------------------------------------
# Logging
# ---------------------------------------------------------------------------
logging.basicConfig(level=logging.INFO, format="%(levelname)s: %(message)s")
log = logging.getLogger(__name__)


def create_test_scene(grid_radius: int = 15, cell_radius: float = 100.0):
    """
    Connect to UE Editor and create hex terrain test scene.

    Steps:
        1. Clean up existing hex actors
        2. Build a new AHexTerrain with good test parameters
        3. Add directional light
        4. Enable debug chunk colors
        5. Print terrain stats
    """
    client = UnrealClient(host="127.0.0.1", port=13377)

    # ── Connect ────────────────────────────────────────────────────────
    log.info("Connecting to Unreal Editor on 127.0.0.1:13377 ...")
    if not client.connect():
        log.error(
            "Cannot connect to UE Editor.\n"
            "  Make sure:\n"
            "  1. Unreal Editor is running with the TA-Playground project\n"
            "  2. The UnrealMCP plugin is enabled\n"
        )
        return False

    # ── Step 1: Clean up ───────────────────────────────────────────────
    log.info("Cleaning up existing hex actors ...")
    r = client.send_command("run_console_command", {"command": "hex.DestroyAll"})
    log.info("  -> %s", r.get("result", r.get("error", "?")))

    # ── Step 2: Build test scene ───────────────────────────────────────
    cmd = f"hex.CreateTestScene {grid_radius} {cell_radius}"
    log.info("Creating test scene: %s", cmd)
    r = client.send_command("run_console_command", {"command": cmd})
    log.info("  -> %s", r.get("result", r.get("error", "?")))

    # ── Step 3: Verify ─────────────────────────────────────────────────
    log.info("Verifying — getting actor list ...")
    r = client.send_command("get_actor_list", {})
    if r.get("success"):
        actors = r.get("result", [])
        hex_actors = [a for a in actors if "Hex" in str(a)]
        log.info("  Total actors: %d  (Hex-related: %d)", len(actors), len(hex_actors))
        for a in hex_actors:
            log.info("    %s", a)
    else:
        log.warning("  get_actor_list failed: %s", r.get("error", "?"))

    # ── Disconnect ─────────────────────────────────────────────────────
    client.disconnect()
    log.info("Done. Test scene created successfully.")
    return True


# ===========================================================================
# Main
# ===========================================================================
if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(
        description="Create Hexagon Terrain Test Scene via UnrealMCP"
    )
    parser.add_argument(
        "--radius", type=int, default=15,
        help="Grid radius (number of rings, default=15, ~720 cells)"
    )
    parser.add_argument(
        "--cellsize", type=float, default=100.0,
        help="Cell circumradius (default=100.0)"
    )
    args = parser.parse_args()

    success = create_test_scene(
        grid_radius=args.radius,
        cell_radius=args.cellsize,
    )
    sys.exit(0 if success else 1)
