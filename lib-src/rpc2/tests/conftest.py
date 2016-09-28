# fixture to return the uninstalled librpc2 library

import pytest
from rpc2 import BUFFER, RPC2_CountedBS, RPC2_BoundedBS
from ctypes import *
from subprocess import check_output
import os

@pytest.fixture(scope="session")
def librpc2():
    """Returns a ctypes wrapped librpc2 library"""
    env = dict(
        var.split('=', 1)
        for var in check_output([
            'libtool', '--mode=execute',
            '-dlopen', 'rpc2-src/librpc2.la',
            'env'
        ]).decode('ascii').split('\n')
        if '=' in var
    )
    library_path = env['LD_LIBRARY_PATH'].split(':')[0]

    _librpc2 = cdll.LoadLibrary(os.path.join(library_path, "librpc2.so.5"))
    _librpc2.pack_integer.argtypes = [POINTER(BUFFER), c_int]
    _librpc2.unpack_integer.argtypes = [POINTER(BUFFER), POINTER(c_int)]
    _librpc2.pack_unsigned.argtypes = [POINTER(BUFFER), c_uint]
    _librpc2.unpack_unsigned.argtypes = [POINTER(BUFFER), POINTER(c_uint)]
    _librpc2.pack_double.argtypes = [POINTER(BUFFER), c_double]
    _librpc2.unpack_double.argtypes = [POINTER(BUFFER), POINTER(c_double)]
    _librpc2.pack_bytes.argtypes = [POINTER(BUFFER), POINTER(c_char), c_size_t]
    _librpc2.unpack_bytes.argtypes = [POINTER(BUFFER), POINTER(c_char), c_size_t]
    _librpc2.pack_byte.argtypes = [POINTER(BUFFER), c_char]
    _librpc2.unpack_byte.argtypes = [POINTER(BUFFER), POINTER(c_char)]
    _librpc2.pack_string.argtypes = [POINTER(BUFFER), c_char_p]
    _librpc2.unpack_string.argtypes = [POINTER(BUFFER), POINTER(c_char_p)]
    _librpc2.pack_countedbs.argtypes = [POINTER(BUFFER), POINTER(RPC2_CountedBS)]
    _librpc2.unpack_countedbs.argtypes = [POINTER(BUFFER), POINTER(RPC2_CountedBS)]
    _librpc2.pack_boundedbs.argtypes = [POINTER(BUFFER), POINTER(RPC2_BoundedBS)]
    _librpc2.unpack_boundedbs.argtypes = [POINTER(BUFFER), POINTER(RPC2_BoundedBS)]
    return _librpc2

