# basic RPC2 datatypes

from ctypes import *

class BUFFER(Structure):
    _fields_ = [
        ("buffer", POINTER(c_char)),
        ("eob", POINTER(c_char)),
        ("who", c_int)
    ]

    def __init__(self, size_or_bytes=None, who='client', *args, **kwargs):
        super(BUFFER, self).__init__(*args, **kwargs)

        self.who = 0 if who == 'client' else 1

        self.content = self.buf = self.eob = None
        if size_or_bytes is None: return

        self.content = create_string_buffer(size_or_bytes)
        self.buffer = self.eob = self.content
        self.eob = cast(byref(self.content, sizeof(self.content)), POINTER(c_char))

    def __getitem__(self, *args, **kwargs):
        return self.content.__getitem__(*args, **kwargs)

class RPC2_CountedBS(Structure):
    _fields_ = [
        ("SeqLen", c_uint),
        ("SeqBody", POINTER(c_char))
    ]

class RPC2_BoundedBS(Structure):
    _fields_ = [
        ("MaxSeqLen", c_uint),
        ("SeqLen", c_uint),
        ("SeqBody", POINTER(c_char))
    ]

