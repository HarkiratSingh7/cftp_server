import os
import signal
import subprocess
import socket
import time
from pathlib import Path
import pytest

FTP_HOST = "127.0.0.1"
FTP_PORT = 21
BUILD_DIR = Path("build") 
SERVER_EXECUTABLE = "./build/cftp_server" 

def is_port_open(host, port, timeout=0.5):
    try:
        with socket.create_connection((host, port), timeout=timeout):
            return True
    except OSError:
        return False


def wait_for_server(host, port, timeout=5):
    start = time.time()
    while time.time() - start < timeout:
        if is_port_open(host, port):
            return True
        time.sleep(0.2)
    return False


@pytest.fixture(scope="session", autouse=True)
def start_ftp_server():
    subprocess.run("pre-commit", check=True)
    if is_port_open(FTP_HOST, FTP_PORT):
        subprocess.run("pkill -f cftp_server", shell=True, check=False)
    print("Building project with CMake...")
    cmake_cmd = f"cmake --build {BUILD_DIR} --parallel"
    result = subprocess.run(cmake_cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode != 0:
        print(result.stdout.decode())
        print(result.stderr.decode())
        pytest.fail("Failed to build C project")
    print("Starting server")
    # Step 2: Start the server as subprocess
    server_proc = subprocess.Popen([SERVER_EXECUTABLE], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    # Step 3: Wait for server to be ready
    if not wait_for_server(FTP_HOST, FTP_PORT, timeout=5):
        server_proc.terminate()
        server_proc.wait()
        pytest.fail("FTP server did not start within timeout")

    # Step 4: Yield control to the tests
    yield

    # Step 5: Cleanup - kill server
    server_proc.terminate()
    try:
        server_proc.wait(timeout=3)
    except subprocess.TimeoutExpired:
        server_proc.kill()