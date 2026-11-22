#!/usr/bin/env python3
# setup.py — ament_python packaging for dome_pad
# Author: Pito Salas and Claude Code
# Version: 1
# Created: 2026-11-22
# Updated: 2026-11-22
# Open Source Under MIT license
import os
from glob import glob

from setuptools import find_packages, setup

package_name = "dome_pad"

setup(
    name=package_name,
    version="0.2.0",
    packages=find_packages(exclude=["test"]),
    data_files=[
        ("share/ament_index/resource_index/packages", ["resource/" + package_name]),
        ("share/" + package_name, ["package.xml"]),
        (
            os.path.join("share", package_name, "launch"),
            glob(os.path.join("launch", "*.launch.py")),
        ),
        (
            os.path.join("share", package_name, "config"),
            glob(os.path.join("config", "*.yaml")),
        ),
    ],
    install_requires=["setuptools"],
    zip_safe=True,
    maintainer="pitosalas",
    maintainer_email="pitosalas@gmail.com",
    description="The Yellow Pad touchscreen gamepad for the DOME robot.",
    license="MIT",
    entry_points={
        "console_scripts": [
            "pad_buttons = dome_pad.pad_buttons_node:main",
        ],
    },
)
