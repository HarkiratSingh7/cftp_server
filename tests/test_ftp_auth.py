import os
import time
import pytest
from ftplib import FTP, FTP_TLS, error_perm
from concurrent.futures import ThreadPoolExecutor
from ftp_test_helper import *
from ftp_ensure_ftp_server_running import *
# --- Utilities ---


# --- FTP Helpers ---

def try_ftp_tls_login(username, password):
    ftps = FTP_TLS()
    ftps.connect(FTP_HOST, FTP_PORT, timeout=10)
    ftps.auth()
    ftps.prot_p()
    ftps.login(username, password)
    time.sleep(10)
    ftps.quit()


def try_raw_ftp_login(username, password):
    # Used to test rejection when TLS is required
    ftp = FTP()
    ftp.connect(FTP_HOST, FTP_PORT, timeout=10)
    ftp.login(username, password)
    ftp.quit()


# --- Pytest fixtures ---

@pytest.fixture(scope="function")
def ftp_test_user():
    if os.geteuid() != 0:
        pytest.skip("This test must be run as root (sudo)")
    username, password = generate_username_password()
    create_linux_user(username, password)
    yield username, password
    delete_linux_user(username)


# --- Test cases ---

def test_successful_tls_login(ftp_test_user):
    username, password = ftp_test_user
    try_ftp_tls_login(username, password)


def test_tls_auth_flow_split(ftp_test_user):
    # Explicitly test auth -> PBSZ -> PROT -> USER -> PASS
    username, password = ftp_test_user
    ftps = FTP_TLS()
    ftps.connect(FTP_HOST, FTP_PORT)
    ftps.auth()  # AUTH TLS
    ftps.prot_p()  # PROT P
    ftps.login(username, password)
    ftps.quit()

def test_invalid_password(ftp_test_user):
    username, _ = ftp_test_user
    with pytest.raises(error_perm):
        try_ftp_tls_login(username, "wrongpassword")


def test_invalid_user():
    with pytest.raises(error_perm):
        try_ftp_tls_login("nonexistent_user", "fakepass")


def test_mixed_auth_attempts(ftp_test_user):
    username, password = ftp_test_user
    total = 1000

    def worker(i):
        if i % 3 == 0:
            try_ftp_tls_login(username, password)
        elif i % 3 == 1:
            with pytest.raises(error_perm):
                try_ftp_tls_login(username, "badpass")
        else:
            with pytest.raises(error_perm):
                try_ftp_tls_login("wronguser", password)

    with ThreadPoolExecutor(max_workers=total) as executor:
        executor.map(worker, range(total))

def test_correct_auth_attempts(ftp_test_user):
    username, password = ftp_test_user
    total = 1000

    def worker(i):
        try_ftp_tls_login(username, password)

    with ThreadPoolExecutor(max_workers=total) as executor:
        executor.map(worker, range(total))

def test_wrong_auth_attempts(ftp_test_user):
    username, password = ftp_test_user
    total = 1000

    def worker(i):
        with pytest.raises(error_perm):
            try_ftp_tls_login(username, "wrongpassword")

    with ThreadPoolExecutor(max_workers=total) as executor:
        executor.map(worker, range(total))