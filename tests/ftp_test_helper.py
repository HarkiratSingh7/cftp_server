import os
import random
import string
import time
import pytest
from ftplib import FTP, FTP_TLS, error_perm, all_errors
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path



# --- Utilities ---

def run_cmd(cmd):
    import subprocess
    result = subprocess.run(cmd, shell=True, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, text=True)
    return result.stdout.strip()


def create_linux_user(username, password):
    run_cmd(f"useradd -m {username}")
    run_cmd(f"echo '{username}:{password}' | chpasswd")
    run_cmd(f"chown -R {username}:{username} /home/{username}")


def delete_linux_user(username):
    run_cmd(f"userdel -r {username}")


def generate_username_password(prefix=None):
    prefix = ''.join(random.choices(string.ascii_lowercase, k=3)) if not prefix else prefix
    suffix = ''.join(random.choices(string.ascii_lowercase + string.digits, k=3))
    username = f"{prefix}_{suffix}"
    password = ''.join(random.choices(string.ascii_letters + string.digits, k=10))
    return username, password

@pytest.fixture(scope="function")
def ftp_test_user():
    if os.geteuid() != 0:
        pytest.skip("This test must be run as root (sudo)")
    from test_ftp_auth import create_linux_user, delete_linux_user, generate_username_password
    username, password = generate_username_password("ls")
    create_linux_user(username, password)
    yield username, password
    delete_linux_user(username)

@pytest.fixture
def ftp_home_dir(ftp_test_user):
    username, _ = ftp_test_user
    home_path = Path(f"/home/{username}")
    yield home_path
