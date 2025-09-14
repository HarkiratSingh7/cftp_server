# stress_test_pasv_ports.py
import os
import sys
import time
import random
import socket
import errno
import threading
import pytest
from contextlib import contextmanager
from ctypes import cdll, c_int

# ------------- Settings -------------
# Absolute or relative path to the shared library
LIB_PATH = "./build/libcftp_unit_test_lib.so"

# Passive range to test (use an isolated range to avoid conflicts)
RANGE_START = 55000
RANGE_END = 55100

# ------------- Load library via ctypes -------------
# ctypes.cdll.LoadLibrary loads a shared library; we then declare argtypes/restype
# so Python marshals parameters safely across the C boundary.
# Ref: Python ctypes docs (library loading, argtypes/restype)
lib = cdll.LoadLibrary(LIB_PATH)

lib.connections_init_pasv_range.argtypes = (c_int, c_int)
lib.connections_init_pasv_range.restype = c_int

lib.connections_shutdown.argtypes = ()
lib.connections_shutdown.restype = None

lib.select_leftmost_available_port.argtypes = ()
lib.select_leftmost_available_port.restype = c_int

lib.release_port.argtypes = (c_int,)
lib.release_port.restype = None

# ------------- Helpers -------------

@contextmanager
def manage_listener(port: int):
    """Bind a TCP socket on INADDR_ANY:port to simulate external occupancy; returns a listening socket."""
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    # SO_REUSEADDR helps when quickly rebinding during tests
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        s.bind(("0.0.0.0", port))
        s.listen(1)
        yield s
    finally:
        try:
            s.close()
        except Exception:
            pass

def init_allocator(start=RANGE_START, end=RANGE_END):
    rc = lib.connections_init_pasv_range(int(start), int(end))
    if rc != 0:
        raise RuntimeError(f"connections_init_pasv_range failed rc={rc} for [{start}..{end}]")
    return start, end

def shutdown_allocator():
    lib.connections_shutdown()

def select_port():
    """Call into C to select the leftmost available passive port; -1 indicates no port available."""
    return int(lib.select_leftmost_available_port())

def release(port: int):
    lib.release_port(int(port))

# ------------- Test cases -------------
@pytest.mark.xdist_group(name="serial")
def test_basic_allocation_release():
    print("[basic] allocating and releasing one port")
    init_allocator()
    p = select_port()
    assert RANGE_START <= p <= RANGE_END, f"unexpected port {p}"
    # Next selection, without release, must be next leftmost
    q = select_port()
    assert q == p + 1, f"expected leftmost {p+1}, got {q}"
    # Release first port; leftmost should become p again
    release(p)
    r = select_port()
    assert r == p, f"expected leftmost {p} after release, got {r}"
    shutdown_allocator()

@pytest.mark.xdist_group(name="serial")
def test_exhaustion_and_recovery():
    print("[exhaustion] allocate full range, expect -1, then release and recover")
    init_allocator()
    got = []
    for i in range(RANGE_START, RANGE_END + 1):
        port = select_port()
        assert port != -1, f"unexpected early exhaustion at {i + 1 - RANGE_START}th attempt"
        got.append(port)
    assert select_port() == -1, "expected exhaustion after consuming all ports"
    # Release all and ensure leftmost is again start
    for port in got:
        release(port)
    p = select_port()
    assert p == RANGE_START, f"expected {RANGE_START} after recovery, got {p}"
    shutdown_allocator()

@pytest.mark.xdist_group(name="serial")
def test_leftmost_property_with_external_occupancy():
    print("[leftmost] verify leftmost logic under external occupancy")
    init_allocator()
    # Occupy RANGE_START..RANGE_START+4 externally
    holders = []
    for port in range(RANGE_START, RANGE_START + 5):
        holders.append(manage_listener(port))
    for cm in holders:
        cm.__enter__()
    try:
        # The allocator should skip busy ports and return the first free one
        p = select_port()
        expected = RANGE_START + 5
        assert p == expected, f"expected leftmost free {expected}, got {p}"
        # Release and ensure the next leftmost moves to expected+1
        q = select_port()
        assert q == expected + 1, f"expected {expected+1}, got {q}"
        # Now free external ports and verify leftmost falls back to RANGE_START
        for cm in reversed(holders):
            cm.__exit__(None, None, None)
        release(p)
        release(q)
        r = select_port()
        assert r == RANGE_START, f"expected {RANGE_START} after external release, got {r}"
    finally:
        # Make sure any open context managers are closed
        for cm in holders:
            try:
                cm.__exit__(None, None, None)
            except Exception:
                pass
        shutdown_allocator()

@pytest.mark.xdist_group(name="serial")
def test_range_size_one():
    print("[edge] range size 1")
    start, end = RANGE_START, RANGE_START
    init_allocator(start, end)
    p = select_port()
    assert p == start, f"expected only port {start}, got {p}"
    # Next should be -1 until released
    assert select_port() == -1, "expected exhaustion for single-port range"
    release(p)
    assert select_port() == start, "expected available again after release"
    shutdown_allocator()

@pytest.mark.xdist_group(name="serial")
def test_out_of_range_release_is_ignored():
    print("[edge] releasing out-of-range should be no-op")
    init_allocator()
    # Releasing outside range must not crash or corrupt state
    release(RANGE_START - 10)
    release(RANGE_END + 10)
    # Allocate two to ensure allocator still sane
    p = select_port()
    q = select_port()
    assert p == RANGE_START and q == RANGE_START + 1, "state corrupted by out-of-range release"
    shutdown_allocator()

@pytest.mark.xdist_group(name="serial")
def test_external_contention_during_selection():
    print("[contention] start a thread that randomly occupies/free ports while selections run")
    init_allocator()
    stop = threading.Event()

    def churner():
        rng = random.Random(123)
        holders = {}
        try:
            while not stop.is_set():
                port = rng.randint(RANGE_START, RANGE_END)
                if port in holders:
                    # free it
                    cm = holders.pop(port)
                    cm.__exit__(None, None, None)
                else:
                    # occupy it
                    cm = manage_listener(port)
                    try:
                        cm.__enter__()
                        holders[port] = cm
                    except OSError as e:
                        # Likely EADDRINUSE from the kernel: ignore
                        if e.errno != errno.EADDRINUSE:
                            raise
                time.sleep(0.002)
        finally:
            for cm in holders.values():
                try:
                    cm.__exit__(None, None, None)
                except Exception:
                    pass

    t = threading.Thread(target=churner, daemon=True)
    t.start()
    try:
        # Try selecting and releasing many times; should never return a port outside range or crash
        for _ in range(1000):
            p = select_port()
            if p != -1:
                assert RANGE_START <= p <= RANGE_END, f"bad port {p}"
                # Immediately release to keep pressure high
                release(p)
            else:
                # Temporary exhaustion under churn is acceptable
                time.sleep(0.001)
    finally:
        stop.set()
        t.join(timeout=1.0)
        shutdown_allocator()

@pytest.mark.xdist_group(name="serial")
def benchmark_throughput(iterations=5000):
    print(f"[bench] {iterations} iterations of select+release")
    init_allocator()
    t0 = time.time()
    for _ in range(iterations):
        p = select_port()
        if p != -1:
            release(p)
    dt = time.time() - t0
    print(f"Throughput: {iterations/dt:.1f} ops/s over {iterations} iterations")
    shutdown_allocator()
