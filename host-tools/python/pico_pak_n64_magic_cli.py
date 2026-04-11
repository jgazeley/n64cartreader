#!/usr/bin/env python3
"""Compatibility launcher for the refactored Pico Pak host CLI package."""

from pico_pak.main import main


if __name__ == "__main__":
    raise SystemExit(main())
