#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys
import shutil
import tempfile
import subprocess
from pathlib import Path
from datetime import date

###############################################################################
# CONFIGURATION (static values only)
###############################################################################

PUBLISHER = "Louthrax"
COMPANY_DOMAIN = "com.louthrax"   # used for component id

# Path to Qt Installer Framework binarycreator.exe
BINARYCREATOR_PATH = r"C:\Qt\Tools\QtInstallerFramework\4.10\bin\binarycreator.exe"

# Windows-specific: where the app will be installed
TARGET_DIR_TEMPLATE = "@ApplicationsDir@/{APP_NAME}"

# Keep the temporary structure for debugging?
KEEP_TEMP_DIR = False

###############################################################################
# END OF CONFIGURATION
###############################################################################

# These will be set from command-line arguments
APP_NAME = None        # Display / branding name
EXE_NAME = None        # Actual executable filename (without .exe)
APP_VERSION = None
TARGET_DIR = None


def write_text_file(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


def create_config_xml(config_dir: Path) -> Path:
    config_xml = f"""<?xml version="1.0" encoding="UTF-8"?>
<Installer>
    <Name>{APP_NAME}</Name>
    <Version>{APP_VERSION}</Version>
    <Title>{APP_NAME} Installer</Title>
    <Publisher>{PUBLISHER}</Publisher>

    <TargetDir>{TARGET_DIR}</TargetDir>

    <StartMenuDir>{APP_NAME}</StartMenuDir>
    <RunProgram>@TargetDir@/{EXE_NAME}.exe</RunProgram>
</Installer>
"""
    config_path = config_dir / "config.xml"
    write_text_file(config_path, config_xml)
    return config_path


def create_package_xml(meta_dir: Path) -> Path:
    component_id = f"{COMPANY_DOMAIN}.{APP_NAME.lower()}"
    today = date.today().isoformat()

    package_xml = f"""<?xml version="1.0" encoding="UTF-8"?>
<Package>
    <DisplayName>{APP_NAME}</DisplayName>
    <Description>{APP_NAME} main application</Description>
    <Version>{APP_VERSION}</Version>
    <ReleaseDate>{today}</ReleaseDate>
    <Default>true</Default>
    <Name>{component_id}</Name>
</Package>
"""
    package_path = meta_dir / "package.xml"
    write_text_file(package_path, package_xml)
    return package_path


def copy_app_files_to_data(source_dir: Path, data_dir: Path) -> None:
    if not source_dir.is_dir():
        raise RuntimeError(f"Source directory does not exist: {source_dir}")

    data_dir.mkdir(parents=True, exist_ok=True)

    for item in source_dir.iterdir():
        dest = data_dir / item.name
        if item.is_dir():
            shutil.copytree(item, dest)
        else:
            shutil.copy2(item, dest)


def run_binarycreator(config_path: Path, packages_dir: Path, output_installer: Path) -> None:
    cmd = [
        BINARYCREATOR_PATH,
        "-c", str(config_path),
        "-p", str(packages_dir),
        str(output_installer)
    ]

    print("Running binarycreator:")
    print("  " + " ".join(f'"{c}"' if " " in c else c for c in cmd))

    result = subprocess.run(cmd)
    if result.returncode != 0:
        raise RuntimeError(f"binarycreator failed with exit code {result.returncode}")


def usage() -> None:
    prog = Path(sys.argv[0]).name
    print("Usage:")
    print(f"  {prog} <APP_NAME> <EXE_NAME> <APP_VERSION> <windeployqt_output_dir> <output_installer.exe>")
    sys.exit(1)


def main() -> None:
    global APP_NAME, EXE_NAME, APP_VERSION, TARGET_DIR

    if len(sys.argv) != 6:
        usage()

    APP_NAME    = sys.argv[1]
    EXE_NAME    = sys.argv[2]
    APP_VERSION = sys.argv[3]

    source_dir = Path(sys.argv[4]).resolve()
    output_installer = Path(sys.argv[5]).resolve()

    TARGET_DIR = TARGET_DIR_TEMPLATE.format(APP_NAME=APP_NAME)

    if not source_dir.is_dir():
        print(f"Error: '{source_dir}' is not a directory.")
        sys.exit(1)

    if output_installer.suffix.lower() != ".exe":
        print("Error: output installer must have a .exe extension.")
        sys.exit(1)

    output_installer.parent.mkdir(parents=True, exist_ok=True)

    if not Path(BINARYCREATOR_PATH).is_file():
        print(f"Error: binarycreator not found at '{BINARYCREATOR_PATH}'")
        sys.exit(1)

    temp_dir = Path(tempfile.mkdtemp(prefix="qtifw_"))
    print(f"Using temporary directory: {temp_dir}")

    try:
        config_dir = temp_dir / "config"
        config_path = create_config_xml(config_dir)

        component_id = f"{COMPANY_DOMAIN}.{APP_NAME.lower()}"
        component_dir = temp_dir / "packages" / component_id
        meta_dir = component_dir / "meta"
        data_dir = component_dir / "data"

        create_package_xml(meta_dir)

        print(f"Copying files from {source_dir} to {data_dir} ...")
        copy_app_files_to_data(source_dir, data_dir)

        run_binarycreator(config_path, temp_dir / "packages", output_installer)

        print(f"\nInstaller successfully created at:\n  {output_installer}")

    finally:
        if KEEP_TEMP_DIR:
            print(f"Temporary directory kept at: {temp_dir}")
        else:
            shutil.rmtree(temp_dir, ignore_errors=True)


if __name__ == "__main__":
    main()
