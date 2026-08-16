import pytest
import sys
import pathlib

sys.path.insert(0, str(pathlib.Path(__file__).parent / "proto"))
pytest_plugins = ['pytest_userver.plugins.core']
