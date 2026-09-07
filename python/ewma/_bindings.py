"""Raw ctypes bindings to libewma.so."""
import ctypes
from ._utils import find_library

_lib = ctypes.CDLL(find_library())

# --- Enums ---
EWMA_SN = 0
EWMA_SR = 1

# --- Structs ---
class EwmaResult(ctypes.Structure):
    _fields_ = [
        ("lambda", ctypes.c_double),
        ("L", ctypes.c_double),
        ("ARL", ctypes.c_double),
        ("deviation", ctypes.c_double),
    ]

class EwmaResultArray(ctypes.Structure):
    _fields_ = [
        ("results", ctypes.POINTER(EwmaResult)),
        ("count", ctypes.c_int),
    ]

# --- Config functions ---
_lib.ewma_config_create.restype = ctypes.c_void_p
_lib.ewma_config_create.argtypes = []

_lib.ewma_config_destroy.restype = None
_lib.ewma_config_destroy.argtypes = [ctypes.c_void_p]

_lib.ewma_config_from_json.restype = ctypes.c_int
_lib.ewma_config_from_json.argtypes = [ctypes.c_void_p, ctypes.c_char_p]

# Config setters
for _name in [
    "ewma_config_set_simulations", "ewma_config_set_n",
    "ewma_config_set_max_iter", "ewma_config_set_n_cores",
    "ewma_config_set_top_n", "ewma_config_set_checkpoint_interval",
    "ewma_config_set_update_interval",
]:
    fn = getattr(_lib, _name)
    fn.restype = ctypes.c_int
    fn.argtypes = [ctypes.c_void_p, ctypes.c_int]

for _name in [
    "ewma_config_set_target_arl", "ewma_config_set_tolerance",
]:
    fn = getattr(_lib, _name)
    fn.restype = ctypes.c_int
    fn.argtypes = [ctypes.c_void_p, ctypes.c_double]

_lib.ewma_config_set_chart_type.restype = ctypes.c_int
_lib.ewma_config_set_chart_type.argtypes = [ctypes.c_void_p, ctypes.c_int]

_lib.ewma_config_set_lambda_range.restype = ctypes.c_int
_lib.ewma_config_set_lambda_range.argtypes = [ctypes.c_void_p, ctypes.c_double, ctypes.c_double, ctypes.c_double]

_lib.ewma_config_set_l_range.restype = ctypes.c_int
_lib.ewma_config_set_l_range.argtypes = [ctypes.c_void_p, ctypes.c_double, ctypes.c_double, ctypes.c_double]

for _name in [
    "ewma_config_set_temp_file", "ewma_config_set_checkpoint_file",
    "ewma_config_set_final_file", "ewma_config_set_best_file",
    "ewma_config_set_log_file", "ewma_config_set_error_file",
]:
    fn = getattr(_lib, _name)
    fn.restype = ctypes.c_int
    fn.argtypes = [ctypes.c_void_p, ctypes.c_char_p]

# --- Core functions ---
_lib.ewma_calculate_arl.restype = ctypes.c_double
_lib.ewma_calculate_arl.argtypes = [ctypes.c_void_p, ctypes.c_double, ctypes.c_double, ctypes.c_int]

_lib.ewma_calculate_arl_dist.restype = ctypes.c_double
_lib.ewma_calculate_arl_dist.argtypes = [
    ctypes.c_void_p, ctypes.c_double, ctypes.c_double,
    ctypes.c_int, ctypes.POINTER(ctypes.c_double),
]

_lib.ewma_run_grid.restype = EwmaResultArray
_lib.ewma_run_grid.argtypes = [ctypes.c_void_p]

_lib.ewma_free_results.restype = None
_lib.ewma_free_results.argtypes = [EwmaResultArray]