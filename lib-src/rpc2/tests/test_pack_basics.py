# test packing and unpacking of basic RPC2 datatypes

import rpc2

def test_pack_integer(librpc2):
    # check if packing fails when the buffer is too small
    buf = rpc2.BUFFER(3)
    assert librpc2.pack_integer(buf, 0) == -1

    buf = rpc2.BUFFER(12)
    assert librpc2.pack_integer(buf, 1) == 0
    assert buf[:] == b'\0\0\0\1\0\0\0\0\0\0\0\0'

    # check if appending works
    assert librpc2.pack_integer(buf, 42) == 0
    assert buf[:] == b'\0\0\0\1\0\0\0*\0\0\0\0'

def test_pack_unsigned(librpc2):
    buf = rpc2.BUFFER(7)
    assert librpc2.pack_unsigned(buf, -1) == 0
    assert buf[:] == b'\xff\xff\xff\xff\0\0\0'

    # check if packing fails when the buffer is too small
    assert librpc2.pack_unsigned(buf, -1) == -1

def test_pack_double(librpc2):
    buf = rpc2.BUFFER(12)
    assert librpc2.pack_double(buf, 26.1) == 0
    assert buf[:] == b'\x9a\x99\x99\x99\x99\x19:@\0\0\0\0'

    # check if packing fails when the buffer is too small
    assert librpc2.pack_double(buf, 0.0) == -1
    
def test_pack_bytes(librpc2):
    buf = rpc2.BUFFER(8)
    assert librpc2.pack_bytes(buf, b'foo', 3) == 0
    assert buf[:] == b'foo\0\0\0\0\0'

    # check if appending works/padding worked
    assert librpc2.pack_bytes(buf, b'bar', 3) == 0
    assert buf[:] == b'foo\0bar\0'

def test_pack_byte(librpc2):
    buf = rpc2.BUFFER(5)
    assert librpc2.pack_byte(buf, b'f') == 0
    assert buf[:] == b'f\0\0\0\0'
    assert librpc2.pack_byte(buf, b'o') == 0
    assert buf[:] == b'f\0\0\0o'
    assert librpc2.pack_byte(buf, b'o') == -1

def test_pack_string(librpc2):
    buf = rpc2.BUFFER(10)
    assert librpc2.pack_string(buf, b'foobar') == -1

    buf = rpc2.BUFFER(13)
    assert librpc2.pack_string(buf, b'foobar') == 0
    assert buf[:] == b'\0\0\0\6foobar\0\0\0'
    assert librpc2.pack_byte(buf, b'b') == 0
    assert buf[:] == b'\0\0\0\6foobar\0\0b'

