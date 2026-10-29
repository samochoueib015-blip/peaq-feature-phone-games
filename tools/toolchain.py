"""Locate LLVM without depending on a particular user's installation paths."""
import os
import shutil
from pathlib import Path


def tool(name):
    suffix = '.exe' if os.name == 'nt' else ''
    candidates = []
    if os.environ.get('PEAQ_LLVM'):
        candidates.append(Path(os.environ['PEAQ_LLVM']) / (name + suffix))
    found = shutil.which(name)
    if found:
        candidates.append(Path(found))
    if os.name == 'nt':
        candidates.append(Path(os.environ.get('ProgramFiles', 'C:/Program Files')) / 'LLVM/bin' / (name + suffix))
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    raise FileNotFoundError(f'{name} missing: install LLVM or set PEAQ_LLVM to its bin directory')
