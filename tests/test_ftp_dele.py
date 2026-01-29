import os
import tempfile
import multiprocessing
from pathlib import Path

import pytest
from ftplib import FTP, FTP_TLS, error_perm

from ftp_test_helper import *
from ftp_ensure_ftp_server_running import *


def _connect(mode: str):
    ftp_cls = FTP_TLS if mode == "tls" else FTP
    ftp = ftp_cls()
    ftp.connect(FTP_HOST, FTP_PORT)
    if mode == "tls":
        # Secure control with AUTH TLS and switch data channel to PROT P
        ftp.auth()
        ftp.prot_p()
    return ftp


@pytest.mark.parametrize("mode", ["plain", "tls"])
def test_dele_existing_file(ftp_test_user, ftp_home_dir, mode):
    username, password = ftp_test_user
    filename = "dele_existing.txt"
    data = b"to-be-deleted\n"

    # Connect and login
    ftp = _connect(mode)
    ftp.login(username, password)

    # Upload a file to delete
    with tempfile.NamedTemporaryFile(delete=False) as tmp:
        tmp.write(data)
        tmp.flush()
        tmp_path = tmp.name

    with open(tmp_path, "rb") as f:
        ftp.storbinary(f"STOR {filename}", f)

    # Delete using DELE
    resp = ftp.delete(filename)
    ftp.quit()

    # Validate file is gone on server FS
    assert not (ftp_home_dir / filename).exists()
    # Optionally ensure server returned a 2xx confirmation
    assert resp.startswith("2")

    # Local cleanup
    Path(tmp_path).unlink(missing_ok=True)


@pytest.mark.parametrize("mode", ["plain", "tls"])
def test_dele_nonexistent_file_raises_550(ftp_test_user, mode):
    username, password = ftp_test_user
    missing = "this_file_does_not_exist.xyz"

    ftp = _connect(mode)
    ftp.login(username, password)

    with pytest.raises(error_perm) as ei:
        ftp.delete(missing)
    ftp.quit()

    # Many servers use a 550 permanent error for missing file
    assert "550" in str(ei.value)


@pytest.mark.parametrize("mode", ["plain", "tls"])
def test_dele_directory_should_fail(ftp_test_user, mode):
    username, password = ftp_test_user
    dirname = "dele_is_not_for_dirs"

    ftp = _connect(mode)
    ftp.login(username, password)

    # Create a directory then try DELE on it (should fail; use RMD for dirs)
    ftp.mkd(dirname)
    with pytest.raises(error_perm) as ei:
        ftp.delete(dirname)

    # Cleanup directory with proper command
    ftp.rmd(dirname)
    ftp.quit()

    # Server typically responds with a permanent error, commonly 550
    assert "55" in str(ei.value)  # accept 550/553/etc. variants


def _upload_and_delete(index, username, password, mode, ftp_home_dir_str):
    filename = f"dele_parallel_{index:02d}.bin"
    content = os.urandom(256 * 1024)  # 256 KiB per file

    # Prepare temp file
    tmp_path = Path(tempfile.gettempdir()) / f"{filename}.tmp"
    with open(tmp_path, "wb") as f:
        f.write(content)

    # Connect
    ftp_cls = FTP_TLS if mode == "tls" else FTP
    ftp = ftp_cls()
    ftp.connect(FTP_HOST, FTP_PORT)
    if mode == "tls":
        ftp.auth()
        ftp.prot_p()
    ftp.login(username, password)

    # Upload then delete
    with open(tmp_path, "rb") as f:
        ftp.storbinary(f"STOR {filename}", f)
    resp = ftp.delete(filename)
    ftp.quit()

    # Validate deletion from server side path
    ftp_home_dir = Path(ftp_home_dir_str)
    if (ftp_home_dir / filename).exists():
        raise AssertionError(f"File still exists on server: {filename}")
    if not resp.startswith("2"):
        raise AssertionError(f"Unexpected DELE response: {resp}")

    # Local cleanup
    tmp_path.unlink(missing_ok=True)


@pytest.mark.parametrize("mode", ["plain", "tls"])
def test_parallel_deletes(ftp_test_user, ftp_home_dir, mode):
    username, password = ftp_test_user
    num_files = 8

    procs = []
    for i in range(num_files):
        p = multiprocessing.Process(
            target=_upload_and_delete,
            args=(i, username, password, mode, str(ftp_home_dir)),
        )
        procs.append(p)
        p.start()

    for p in procs:
        p.join()
        assert p.exitcode == 0, "One of the parallel delete workers failed"
