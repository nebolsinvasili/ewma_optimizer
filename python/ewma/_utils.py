"""Library path discovery for libewma.so."""
import os
from pathlib import Path

def find_library() -> str:
    """Find libewma.so relative to this package or in standard paths."""
    pkg_dir = Path(__file__).resolve().parent
    project_root = pkg_dir.parent.parent
    lib_path = project_root / "lib" / "libewma.so"
    if lib_path.exists():
        return str(lib_path)

    env_path = os.environ.get("EWMA_LIB_PATH")
    if env_path and Path(env_path).exists():
        return env_path

    for search in ["/usr/local/lib", "/usr/lib", Path.home() / "lib"]:
        p = Path(search) / "libewma.so"
        if p.exists():
            return str(p)

    raise FileNotFoundError(
        "Cannot find libewma.so. Build with 'make lib' or set EWMA_LIB_PATH."
    )