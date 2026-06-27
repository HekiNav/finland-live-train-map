#!/usr/bin/env python3
"""
Build script for ESP32 web installer deployment
"""
import os
import subprocess
import shutil
import sys
import json
from typing import List, Optional

# Configuration
ENVIRONMENTS = [
    {"id": "hki-ltm_100", "name": "LTM Firmware (Helsinki) V1.0"},
    {"id": "fin-ltm_100", "name": "LTM Firmware (Finland) V1.0"},
]
WEB_INSTALLER_SRC = "website/web_installer"
SITE_DIR = "_site"
BUILD_DIR = "firmware/.pio/build"


def run_command(cmd: List[str], cwd: Optional[str] = None) -> None:
    """Run a shell command and exit on failure"""
    try:
        subprocess.run(cmd, check=True, cwd=cwd)
    except subprocess.CalledProcessError as e:
        print(f"Command failed: {' '.join(cmd)}")
        print(f"Error: {e}")
        sys.exit(1)


def build_environments() -> None:
    """Build all configured environments with mergebin target"""
    for env in ENVIRONMENTS:
        print(f"Building environment: {env['id']}")
        run_command(["pio", "run", "-d","./firmware", "-e", env["id"], "--target", "mergebin_seperate_bootloader"])


def create_manifest(env: dict[str, str], site_bin_dir: str) -> None:
    """Create manifest.json for the given environment in the _site/bin/env directory."""
    manifest: dict[str, object] = {
        "name": env["name"],
        "version": "",
        "new_install_prompt_erase": True,
        "builds": [
            {
                "parts": [
                    {
                        "path": "bootloader.bin",
                        "offset": 0,
                    },
                    {
                        "path": "firmware-app0.bin",
                        "offset": 65536,
                    }
                ],
            }
        ],
    }
    manifest_path = os.path.join(site_bin_dir, "manifest.json")
    with open(manifest_path, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)


def prepare_deployment_files() -> None:
    """Prepare all files for deployment to _site directory"""
    # Create directory structure
    for env in ENVIRONMENTS:
        os.makedirs(os.path.join(SITE_DIR, "bin", env["id"]), exist_ok=True)

    # Copy web installer files (favicon and html)
    shutil.copy(os.path.join(WEB_INSTALLER_SRC, "led-rails.html"), SITE_DIR)
    shutil.copy(os.path.join(WEB_INSTALLER_SRC, "favicon.png"), SITE_DIR)

    # Copy firmware binaries and create manifest.json
    for env in ENVIRONMENTS:
        # 
        src_bin = os.path.join(BUILD_DIR, env["id"], "firmware-app0.bin")
        dst_bin_dir = os.path.join(SITE_DIR, "bin", env["id"])
        dst_bin = os.path.join(dst_bin_dir, "firmware-app0.bin")
        shutil.copy(src_bin, dst_bin)

        src_bin = os.path.join(BUILD_DIR, env["id"], "bootloader.bin")
        dst_bin_dir = os.path.join(SITE_DIR, "bin", env["id"])
        dst_bin = os.path.join(dst_bin_dir, "bootloader.bin")
        shutil.copy(src_bin, dst_bin)

        print(f"Copied firmware and bootloader for {env['id']}")
        create_manifest(env, dst_bin_dir)


def main() -> None:
    """Main entry point"""
    print("Starting build process...")
    build_environments()
    print("Preparing deployment files...")
    prepare_deployment_files()
    print("Build process completed successfully!")


if __name__ == "__main__":
    main()
