
import os
import pytest
import tempfile
from ftplib import FTP, FTP_TLS, error_perm
import multiprocessing
from ftp_test_helper import *
from ftp_ensure_ftp_server_running import *

def test_list_tls_mode_non_empty(ftp_test_user, ftp_home_dir):
    username, password = ftp_test_user

    # Create some files
    (ftp_home_dir / "file1.txt").write_text("hello")
    (ftp_home_dir / "file2.txt").write_text("world")
    (ftp_home_dir / "file3.log").write_text("logfile")

    ftps = FTP_TLS()
    ftps.connect(FTP_HOST, FTP_PORT)
    ftps.auth()
    ftps.prot_p()
    ftps.login(username, password)
    files = ftps.nlst()
    assert "file1.txt" in files
    assert "file2.txt" in files
    assert "file3.log" in files
    ftps.quit()

def test_list_tls_mode_empty(ftp_test_user, ftp_home_dir):
    username, password = ftp_test_user
    run_cmd(f"sudo rm -rf {ftp_home_dir}/.* {ftp_home_dir}/*")
    ftps = FTP_TLS()
    ftps.connect(FTP_HOST, FTP_PORT)
    ftps.auth()
    ftps.prot_p()
    ftps.login(username, password)
    files = ftps.nlst()
    assert files == []
    ftps.quit()

def test_list_plaintext_mode_non_empty(ftp_test_user, ftp_home_dir):
    username, password = ftp_test_user

    (ftp_home_dir / "alpha.txt").write_text("A")
    (ftp_home_dir / "beta.txt").write_text("B")

    ftp = FTP()
    ftp.connect(FTP_HOST, FTP_PORT)
    ftp.login(username, password)
    files = ftp.nlst()
    assert "alpha.txt" in files
    assert "beta.txt" in files
    ftp.quit()

def test_list_plaintext_mode_empty(ftp_test_user, ftp_home_dir):
    username, password = ftp_test_user
    run_cmd(f"sudo rm -rf {ftp_home_dir}/.* {ftp_home_dir}/*")
    ftp = FTP()
    ftp.connect(FTP_HOST, FTP_PORT)
    ftp.login(username, password)
    files = ftp.nlst()
    assert files == []
    ftp.quit()

@pytest.mark.parametrize("mode", ["plain", "tls"])
def test_list_special_char_filenames(ftp_test_user, ftp_home_dir, mode):
    username, password = ftp_test_user

    names = ["file with space.txt", "we!rd$n@me.txt", "उदाहरण.txt"]
    for name in names:
        (ftp_home_dir / name).write_text("data")

    ftp_cls = FTP_TLS if mode == "tls" else FTP
    ftp = ftp_cls()
    ftp.connect(FTP_HOST, FTP_PORT)
    if mode == "tls":
        ftp.auth()
        ftp.prot_p()
    ftp.login(username, password)
    files = ftp.nlst()
    for name in names:
        assert name in files
    ftp.quit()

@pytest.mark.parametrize("mode", ["plain", "tls"])
def test_list_empty_home_dir(ftp_test_user, ftp_home_dir, mode):
    username, password = ftp_test_user
    home_path = f"/home/{username}"
    run_cmd(f"sudo rm -rf {ftp_home_dir}/.* {ftp_home_dir}/*")
    ftp_cls = FTP_TLS if mode == "tls" else FTP
    ftp = ftp_cls()
    ftp.connect(FTP_HOST, FTP_PORT)
    if mode == "tls":
        ftp.auth()
        ftp.prot_p()
    ftp.login(username, password)
    lines = []
    ftp.retrlines("LIST", lines.append)
    assert lines == []


@pytest.mark.parametrize("mode", ["plain", "tls"])
def test_list_with_files(ftp_test_user, ftp_home_dir, mode):
    username, password = ftp_test_user
    home_path = f"/home/{username}"
    with open(os.path.join(home_path, "file1.txt"), "w") as f:
        f.write("test")
    with open(os.path.join(home_path, "file2.txt"), "w") as f:
        f.write("test")
    ftp_cls = FTP_TLS if mode == "tls" else FTP
    ftp = ftp_cls()
    ftp.connect(FTP_HOST, FTP_PORT)
    if mode == "tls":
        ftp.auth()
        ftp.prot_p()
    ftp.login(username, password)
    lines = []
    ftp.retrlines("LIST", lines.append)
    assert any("file1.txt" in line for line in lines)
    assert any("file2.txt" in line for line in lines)


@pytest.mark.parametrize("mode", ["plain", "tls"])
def test_list_hidden_files(ftp_test_user, ftp_home_dir, mode):
    username, password = ftp_test_user
    home_path = f"/home/{username}"
    with open(os.path.join(home_path, ".hiddenfile"), "w") as f:
        f.write("secret")
    ftp_cls = FTP_TLS if mode == "tls" else FTP
    ftp = ftp_cls()
    ftp.connect(FTP_HOST, FTP_PORT)
    if mode == "tls":
        ftp.auth()
        ftp.prot_p()
    ftp.login(username, password)
    lines = []
    ftp.retrlines("LIST -a", lines.append)
    assert any(".hiddenfile" in line for line in lines)


@pytest.mark.parametrize("mode", ["plain", "tls"])
def test_list_subdirectory(ftp_test_user, ftp_home_dir, mode):
    username, password = ftp_test_user
    home_path = f"/home/{username}"
    os.makedirs(os.path.join(home_path, "subdir"), exist_ok=True)
    with open(os.path.join(home_path, "subdir", "inside.txt"), "w") as f:
        f.write("hello")
    ftp_cls = FTP_TLS if mode == "tls" else FTP
    ftp = ftp_cls()
    ftp.connect(FTP_HOST, FTP_PORT)
    if mode == "tls":
        ftp.auth()
        ftp.prot_p()
    ftp.login(username, password)
    ftp.cwd("subdir")
    lines = []
    ftp.retrlines("LIST", lines.append)
    assert any("inside.txt" in line for line in lines)


@pytest.mark.parametrize("mode", ["plain", "tls"])
def test_list_invalid_path(ftp_test_user, ftp_home_dir, mode):
    username, password = ftp_test_user
    ftp_cls = FTP_TLS if mode == "tls" else FTP
    ftp = ftp_cls()
    ftp.connect(FTP_HOST, FTP_PORT)
    if mode == "tls":
        ftp.auth()
        ftp.prot_p()
    ftp.login(username, password)
    with pytest.raises(error_perm):
        ftp.retrlines("LIST invalidpath")