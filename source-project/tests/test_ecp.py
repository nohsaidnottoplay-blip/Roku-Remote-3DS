"""Exercise the real C socket client against a local simulated Roku, without a TV."""
import pathlib
import socket
import subprocess
import tempfile
import threading
import time
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class EcpTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.client = pathlib.Path(cls.temp.name) / 'ecp-client'
        subprocess.run(['gcc', '-std=gnu11', '-O2', '-Wall', '-Wextra', '-Werror',
                        '-I' + str(ROOT / 'include'), str(ROOT / 'source/ecp.c'),
                        str(ROOT / 'tests/client.c'), '-o', str(cls.client)], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def call(self, *args):
        return subprocess.run([str(self.client), *args], text=True, capture_output=True, timeout=3)

    def exchange(self, fragments, key='Home', delay=0, timeout='500'):
        listener = socket.socket()
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listener.bind(('127.0.0.1', 8060))
        listener.listen(1)
        listener.settimeout(2)
        requests, errors = [], []

        def server():
            try:
                with listener.accept()[0] as sock:
                    sock.settimeout(2)
                    data = b''
                    while b'\r\n\r\n' not in data:
                        chunk = sock.recv(4096)
                        if not chunk:
                            break
                        data += chunk
                    requests.append(data)
                    time.sleep(delay)
                    for part in fragments:
                        sock.sendall(part)
                        time.sleep(0.003)
            except (BrokenPipeError, ConnectionResetError):
                pass
            except Exception as exc:
                errors.append(exc)
            finally:
                listener.close()

        thread = threading.Thread(target=server)
        thread.start()
        result = self.call('127.0.0.1', key, timeout)
        thread.join(3)
        self.assertFalse(thread.is_alive())
        self.assertEqual(errors, [])
        self.assertEqual(len(requests), 1, 'Commands must never be automatically retried')
        return result, requests[0]

    def test_ip_validation(self):
        valid = ['192.168.1.25', '10.0.0.2', '172.16.12.34', '127.0.0.1']
        invalid = ['', 'roku.local', '1.2.3', '1.2.3.4.5', '256.1.1.1',
                   '192.168.001.2', '0.0.0.0', '224.0.0.1', '1.2.3.4\r\nHost:evil',
                   'http://192.168.1.1', '-1.2.3.4', '1..2.3', '1.2.3.', '.1.2.3']
        result = self.call('validate', *(valid + invalid))
        self.assertEqual(result.stdout.splitlines(), ['1'] * len(valid) + ['0'] * len(invalid))

    def test_post_exact_request_and_fragmented_response(self):
        r, req = self.exchange([b'HT', b'TP/1.1 200 OK\r\nCont', b'ent-Length: 0\r\n\r\n'])
        self.assertEqual(r.returncode, 0, r.stdout)
        self.assertTrue(req.startswith(b'POST /keypress/Home HTTP/1.1\r\n'))
        self.assertIn(b'Host: 127.0.0.1:8060\r\n', req)
        self.assertTrue(req.endswith(b'Content-Length: 0\r\n\r\n'))

    def test_all_buttons(self):
        for key in ['Back', 'Up', 'Down', 'Left', 'Right', 'Select', 'Play', 'Rev',
                    'Fwd', 'Info', 'InstantReplay', 'VolumeUp', 'VolumeDown', 'VolumeMute']:
            with self.subTest(key=key):
                r, req = self.exchange([b'HTTP/1.1 204 No Content\r\n\r\n'], key)
                self.assertEqual(r.returncode, 0, r.stdout)
                self.assertTrue(req.startswith(('POST /keypress/' + key + ' HTTP/1.1').encode()))

    def test_device_check(self):
        r, req = self.exchange([b'HTTP/1.1 200 OK\r\nConnection: close\r\n\r\n',
                               b'<?xml version="1.0"?><devi',
                               b'ce-info><model-name>Roku TV</model-name></device-info>'], 'check')
        self.assertEqual(r.returncode, 0, r.stdout)
        self.assertTrue(req.startswith(b'GET /query/device-info HTTP/1.1\r\n'))

    def test_chunked_device_check(self):
        body = b'<device-info><model-name>Roku TV</model-name></device-info>'
        r, _ = self.exchange([b'HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n',
                              ('%x\r\n' % len(body)).encode() + body + b'\r\n0\r\n\r\n'], 'check')
        self.assertEqual(r.returncode, 0, r.stdout)

    def test_control_denied(self):
        for status in [401, 403]:
            r, _ = self.exchange([f'HTTP/1.1 {status} Denied\r\n\r\n'.encode()])
            self.assertEqual(r.returncode, 5)
            self.assertIn('denied', r.stdout)

    def test_root_split_across_http_chunks(self):
        parts = [b'<de', b'vice-', b'info>', b'</device-info>']
        chunks = [('%x\r\n' % len(p)).encode() + p + b'\r\n' for p in parts]
        r, _ = self.exchange([b'HTTP/1.1 200 OK\r\ntransfer-encoding: chunked\r\n\r\n',
                              b''.join(chunks) + b'0\r\n\r\n'], 'check')
        self.assertEqual(r.returncode, 0, r.stdout)

    def test_rejected_command(self):
        r, _ = self.exchange([b'HTTP/1.1 404 Not Found\r\n\r\n'])
        self.assertEqual(r.returncode, 6)
        self.assertIn('404', r.stdout)

    def test_not_a_roku(self):
        r, _ = self.exchange([b'HTTP/1.1 200 OK\r\n\r\n<html>Router admin</html>'], 'check')
        self.assertEqual(r.returncode, 4)

    def test_bad_http(self):
        for data in [b'garbage\r\n\r\n', b'HTTP/1.1 2000 Nope\r\n\r\n', b'HTTP/1.1 abc Bad\r\n\r\n']:
            r, _ = self.exchange([data])
            self.assertEqual(r.returncode, 4)

    def test_timeout_without_retry(self):
        start = time.monotonic()
        r, _ = self.exchange([], delay=0.25, timeout='80')
        self.assertEqual(r.returncode, 3)
        self.assertLess(time.monotonic() - start, 1)

    def test_invalid_key_never_opens_connection(self):
        r = self.call('127.0.0.1', 'Home\r\nX-Injected:yes')
        self.assertEqual(r.returncode, 7)


if __name__ == '__main__':
    unittest.main(verbosity=2)
