"""Exercise only a spawned SwiftEdit child on an owned disposable pseudo-terminal."""
from __future__ import annotations
import argparse
import fcntl
import os
from pathlib import Path
import pty
import select
import signal
import struct
import subprocess
import tempfile
import termios
import time


def terminal_mode(slave: int) -> list[object]:
    # Darwin sets transient PENDIN when restoring ICANON. FIONREAD processes
    # pending input through the restored discipline without consuming/flushing it.
    # Compare every mode field after that kernel transition, without masking bits.
    # See apple-oss-distributions/xnu bsd/kern/tty.c: ttioctl and ttnread.
    fcntl.ioctl(slave, termios.FIONREAD, struct.pack('i', 0))
    mode: list[object] = termios.tcgetattr(slave)
    return mode


def await_bytes(master: int, child: subprocess.Popen[bytes], marker: bytes) -> None:
    deadline: float = time.monotonic() + 8.0
    observed: bytes = b''
    while time.monotonic() < deadline:
        if child.poll() is not None:
            raise RuntimeError('Terminal child exited before readiness')
        readable: list[int]
        readable, _, _ = select.select([master], [], [], 0.1)
        if readable:
            observed += os.read(master, 65536)
            if marker in observed:
                return
            observed = observed[-65536:]
    raise RuntimeError('Terminal readiness timed out')


def finish(master: int, child: subprocess.Popen[bytes]) -> int:
    deadline: float = time.monotonic() + 8.0
    while time.monotonic() < deadline:
        result: int | None = child.poll()
        if result is not None:
            return result
        readable: list[int]
        readable, _, _ = select.select([master], [], [], 0.05)
        if readable:
            os.read(master, 65536)
    raise RuntimeError('Terminal child did not finish')


def exercise(executable: Path, path: Path, interrupt: bool) -> None:
    master: int
    slave: int
    master, slave = pty.openpty()
    child: subprocess.Popen[bytes] | None = None
    try:
        fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack('HHHH', 24, 80, 0, 0))
        original: list[object] = terminal_mode(slave)
        child = subprocess.Popen([str(executable), str(path)], stdin=slave, stdout=slave,
                                 stderr=slave, start_new_session=True)
        await_bytes(master, child, b'\x1b[?2004h')
        if termios.tcgetattr(slave) == original:
            raise RuntimeError('Terminal did not enter raw input mode')
        if interrupt:
            child.send_signal(signal.SIGTERM)
        else:
            payload: bytes = 'é😀\r\n'.encode('utf-8') + b'\x13\x18'
            keys: bytes = b'\x1b[200~' + payload + b'\x1b[201~\x1a\x19\x13\x18'
            written: int = os.write(master, keys)
            if written != len(keys):
                raise RuntimeError('Incomplete smoke input write')
        result: int = finish(master, child)
        if (interrupt and result == 0) or (not interrupt and result != 0):
            raise RuntimeError('Unexpected terminal exit status: ' + str(result))
        restored: list[object] = terminal_mode(slave)
        if restored != original:
            print('Terminal original mode:', repr(original), flush=True)
            print('Terminal restored mode:', repr(restored), flush=True)
            print('Interrupted case:', interrupt, flush=True)
            raise RuntimeError('Terminal mode was not restored')
        expected: bytes = b'base\n'
        if not interrupt:
            expected = 'é😀\r\n'.encode('utf-8') + b'\x13\x18base\n'
        if path.read_bytes() != expected:
            raise RuntimeError('Paste, undo/redo, save or interruption changed incorrect bytes')
    finally:
        if child is not None and child.poll() is None:
            child.kill()
            child.wait()
        os.close(master)
        os.close(slave)


def main() -> None:
    parser: argparse.ArgumentParser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    arguments: argparse.Namespace = parser.parse_args()
    executable: Path = arguments.executable.resolve(strict=True)
    with tempfile.TemporaryDirectory(prefix='swiftedit-terminal-') as directory:
        path: Path = Path(directory) / 'café.txt'
        path.write_bytes(b'base\n')
        exercise(executable, path, False)
        path.write_bytes(b'base\n')
        exercise(executable, path, True)
    print('POSIX terminal: atomic Unicode/control paste, undo/redo, save, exit and mode restoration passed.')


if __name__ == '__main__':
    main()
