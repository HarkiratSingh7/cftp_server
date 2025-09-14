
import os
import pytest
import tempfile
from ftplib import FTP, FTP_TLS, error_perm
import multiprocessing
from ftp_test_helper import *
from ftp_ensure_ftp_server_running import *

@pytest.mark.parametrize("mode", ["plain", "tls"])
def test_stor_and_retr(ftp_test_user, ftp_home_dir, mode):
    username, password = ftp_test_user
    filename = "upload_test.txt"
    contents = b"Hello from upload\nLine2\nAnother Line\n"

    # Choose FTP or FTP_TLS based on mode
    ftp_cls = FTP_TLS if mode == "tls" else FTP
    ftp = ftp_cls()
    ftp.connect(FTP_HOST, FTP_PORT)
    if mode == "tls":
        ftp.auth()
        ftp.prot_p()
    ftp.login(username, password)

    # Upload
    with tempfile.NamedTemporaryFile(delete=False) as tmp:
        tmp.write(contents)
        tmp.flush()
        tmp_path = tmp.name

    with open(tmp_path, "rb") as f:
        ftp.storbinary(f"STOR {filename}", f)

    # Download
    retrieved = bytearray()
    ftp.retrbinary(f"RETR {filename}", retrieved.extend)
    ftp.quit()

    assert retrieved == contents
    # Cleanup
    (ftp_home_dir / filename).unlink(missing_ok=True)



@pytest.mark.parametrize("mode", ["plain", "tls"])
def test_big_file_upload_download(ftp_test_user, ftp_home_dir, mode):
    username, password = ftp_test_user
    filename = "big_upload_file.bin"
    size_in_gb = 1
    size = size_in_gb * 1024 * 1024 * 1024

    # Generate random content
    big_content = os.urandom(size)

    # Save to temp file
    with tempfile.NamedTemporaryFile(delete=False) as tmp:
        tmp.write(big_content)
        tmp_path = tmp.name

    # Connect FTP
    ftp_cls = FTP_TLS if mode == "tls" else FTP
    ftp = ftp_cls()
    ftp.connect(FTP_HOST, FTP_PORT)
    if mode == "tls":
        ftp.auth()
        ftp.prot_p()
    ftp.login(username, password)

    # Upload (STOR)
    with open(tmp_path, "rb") as f:
        ftp.storbinary(f"STOR {filename}", f)

    # Download (RETR)
    downloaded = bytearray()
    ftp.retrbinary(f"RETR {filename}", downloaded.extend)
    ftp.quit()

    # Compare
    assert downloaded == big_content

    # Cleanup
    os.unlink(tmp_path)
    (ftp_home_dir / filename).unlink(missing_ok=True)

def upload_and_download(index, username, password, mode, ftp_home_dir, size_mb):
    filename = f"file_{index:02d}.bin"
    size = size_mb * 1024 * 1024
    content = os.urandom(size)

    tmp_path = Path(tempfile.gettempdir()) / f"{filename}.tmp"
    with open(tmp_path, "wb") as f:
        f.write(content)

    ftp_cls = FTP_TLS if mode == "tls" else FTP
    ftp = ftp_cls()
    ftp.connect(FTP_HOST, FTP_PORT)
    if mode == "tls":
        ftp.auth()
        ftp.prot_p()
    ftp.login(username, password)

    # Upload
    with open(tmp_path, "rb") as f:
        ftp.storbinary(f"STOR {filename}", f)

    # Download and compare
    downloaded = bytearray()
    ftp.retrbinary(f"RETR {filename}", downloaded.extend)
    ftp.quit()

    assert downloaded == content, f"Data mismatch in file {filename}"

    # Clean up
    tmp_path.unlink(missing_ok=True)
    os.remove(os.path.join(ftp_home_dir, filename))


@pytest.mark.parametrize("mode", ["plain", "tls"])
def test_massive_parallel_file_transfers(ftp_test_user, ftp_home_dir, mode):
    username, password = ftp_test_user
    num_files = 10
    size_per_file_mb = 100  # ~1 GB total traffic

    processes = []
    for i in range(num_files):
        p = multiprocessing.Process(
            target=upload_and_download,
            args=(i, username, password, mode, str(ftp_home_dir), size_per_file_mb)
        )
        processes.append(p)
        p.start()

    for p in processes:
        p.join()
        assert p.exitcode == 0, "One of the processes failed"