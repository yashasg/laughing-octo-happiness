"""Tests for version module."""

import re
from version import __version__


def test_version_exists():
    assert __version__ is not None
    assert isinstance(__version__, str)


def test_version_semver_format():
    """Version must follow semantic versioning (MAJOR.MINOR.PATCH)."""
    pattern = r"^\d+\.\d+\.\d+$"
    assert re.match(pattern, __version__), (
        f"Version '{__version__}' does not match semver format X.Y.Z"
    )


def test_version_is_not_empty():
    assert len(__version__) > 0
