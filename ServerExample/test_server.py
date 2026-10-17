"""Pruebas del protocolo HTTP y de la migración de la base de datos."""

import http.client
import hashlib
import json
import socket
import sqlite3
import ssl
import struct
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from pathlib import Path
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import x25519
from cryptography.hazmat.primitives.ciphers.aead import ChaCha20Poly1305

from server import MensingerServer, create_database, wav_duration_ms
from secure_store import ContentCipher, default_key_path, migrate_content
from secure_channel import (KDF_LABEL, REQUEST_AAD, REQUEST_NONCE, RESPONSE_AAD,
                            RESPONSE_NONCE, SecureChannel)


def wav(seconds):
    """Crea un WAV mínimo con el formato exacto que acepta el endpoint de audio."""
    samples = 10910 * seconds
    pcm = b"\x00\x00" * samples
    return struct.pack(
        "<4sI4s4sIHHIIHH4sI", b"RIFF", 36 + len(pcm), b"WAVE",
        b"fmt ", 16, 1, 1, 10910, 21820, 2, 16, b"data", len(pcm),
    ) + pcm


class ServerExampleTests(unittest.TestCase):
    """Prueba migración, protocolo HTTP, medios, notificaciones y perfiles."""

    def setUp(self):
        """Levanta un servidor temporal sobre una base de esquema antiguo."""
        self.temp = tempfile.TemporaryDirectory()
        self.db = Path(self.temp.name) / "messages.sqlite3"
        # Esquema de la primera versión del ejemplo, antes de añadir voz.
        with sqlite3.connect(self.db) as connection:
            connection.execute(
                "CREATE TABLE messages (id INTEGER PRIMARY KEY AUTOINCREMENT, "
                "sender TEXT NOT NULL, recipient TEXT NOT NULL, body TEXT NOT NULL, "
                "notification_delivered INTEGER NOT NULL DEFAULT 0, "
                "created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP)"
            )
            connection.execute(
                "INSERT INTO messages(sender, recipient, body) VALUES ('111', '222', 'antiguo')"
            )
        create_database(self.db)
        self.server = MensingerServer(("127.0.0.1", 0), self.db, "v1.1.0",
                                      encrypted_transport=False)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()

    def tearDown(self):
        """Apaga el servidor de pruebas y elimina los archivos temporales."""
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(timeout=2)
        self.temp.cleanup()

    def request(self, method, path, body=None, content_type=None, headers=None):
        """Envía una petición local y devuelve estado HTTP junto con el cuerpo."""
        connection = http.client.HTTPConnection("127.0.0.1", self.server.server_port, timeout=10)
        request_headers = dict(headers or {})
        if content_type:
            request_headers["Content-Type"] = content_type
        connection.request(method, path, body=body, headers=request_headers)
        response = connection.getresponse()
        result = (response.status, response.read())
        connection.close()
        return result

    def test_migration_history_pagination_and_notifications(self):
        """Comprueba migración automática, cursores y consumo de notificaciones."""
        status, body = self.request("GET", "/historial/111/222?limit=20&before=0")
        self.assertEqual(status, 200)
        self.assertIn(b"1\t111\ttexto\t0\tantiguo", body)

        payload = json.dumps({"de": "222", "para": "111", "texto": "respuesta"}).encode()
        self.assertEqual(self.request("POST", "/enviar", payload, "application/json")[0], 200)
        self.assertEqual(self.request("GET", "/notificaciones/111"), (200, b"222"))
        self.assertEqual(self.request("GET", "/notificaciones/111"), (200, b"NINGUNO"))
        status, body = self.request("GET", "/historial/111/222?limit=1&before=2")
        self.assertEqual(status, 200)
        self.assertIn(b"antiguo", body)
        self.assertNotIn(b"respuesta", body)

    def test_senders_and_presence_expiration(self):
        """La bandeja conserva desconocidos leídos y la presencia vence sin latido."""
        text = json.dumps({"de": "333", "para": "111", "texto": "hola"}).encode()
        self.assertEqual(self.request("POST", "/enviar", text, "application/json")[0], 200)
        status, rows = self.request("GET", "/remitentes/111")
        self.assertEqual(status, 200)
        self.assertIn(b"333\t1\t1\t2\n", rows)
        self.assertEqual(self.request("GET", "/remitentes/111"), (200, rows))
        self.request("GET", "/historial/111/333")
        self.assertIn(b"333\t0\t1\t2\n", self.request("GET", "/remitentes/111")[1])
        self.assertEqual(self.request("GET", "/presencias/111?ids=333"), (200, b"333\t0\n"))
        self.assertEqual(self.request("POST", "/presencia/333"), (200, b"OK"))
        self.assertEqual(self.request("GET", "/presencias/111?ids=333"), (200, b"333\t1\n"))
        self.server.presence["333"] -= 61
        self.assertEqual(self.request("GET", "/presencias/111?ids=333"), (200, b"333\t0\n"))
        self.assertEqual(self.request("GET", "/presencias/111?ids=x")[0], 400)

    def test_profile_creation_date_migration(self):
        """Migra perfiles antiguos y fija el alta de perfiles nuevos para gráficas."""
        legacy = Path(self.temp.name) / "legacy.sqlite3"
        create_database(legacy)
        with sqlite3.connect(legacy) as connection:
            connection.execute("DROP TABLE profiles")
            connection.execute("CREATE TABLE profiles (user_id TEXT PRIMARY KEY, "
                               "updated_at TEXT NOT NULL)")
            connection.execute("INSERT INTO profiles VALUES ('111', '2025-01-02 03:04:05')")
        create_database(legacy)
        with sqlite3.connect(legacy) as connection:
            self.assertEqual(connection.execute(
                "SELECT created_at FROM profiles WHERE user_id='111'").fetchone()[0],
                "2025-01-02 03:04:05")
            connection.execute("INSERT INTO profiles(user_id, updated_at) "
                               "VALUES ('222', CURRENT_TIMESTAMP)")
            self.assertIsNotNone(connection.execute(
                "SELECT created_at FROM profiles WHERE user_id='222'").fetchone()[0])

    def test_audio_upload_download_and_limits(self):
        """Comprueba persistencia WAV, descarga y rechazo de formato/duración inválidos."""
        audio = wav(2)
        self.assertEqual(wav_duration_ms(audio), 2000)
        status, body = self.request("POST", "/audio/111/222", audio, "audio/wav")
        self.assertEqual(status, 200)
        message_id = int(body)
        self.assertEqual(self.request("GET", f"/audio/{message_id}",
                                      headers={"X-User-ID": "111"}), (200, audio))
        status, history = self.request("GET", "/historial/111/222")
        self.assertEqual(status, 200)
        self.assertIn(f"{message_id}\t111\taudio\t2000\t".encode(), history)
        with sqlite3.connect(self.db) as connection:
            persisted = connection.execute(
                "SELECT audio_data FROM messages WHERE id = ?", (message_id,)
            ).fetchone()[0]
        self.assertNotEqual(persisted, audio)
        self.assertTrue(persisted.startswith(b"MP1"))
        self.assertEqual(self.server.cipher.open_blob("messages", message_id,
                                                     "audio_data", persisted), audio)
        self.assertEqual(self.request("POST", "/audio/111/222", wav(30), "audio/wav")[0], 200)
        self.assertEqual(self.request("POST", "/audio/111/222", b"bad", "audio/wav")[0], 413)
        self.assertEqual(self.request("POST", "/audio/111/222", wav(31), "audio/wav")[0], 413)
        self.assertEqual(self.request("GET", "/audio/999999",
                                      headers={"X-User-ID": "111"})[0], 404)

    def test_profile_fields_and_photo(self):
        """Comprueba actualización de perfil y foto original en sus rutas HTTP."""
        self.assertEqual(self.request("GET", "/perfil/111")[0], 404)
        photo = b"\x1f\x00" * (400 * 240)
        self.assertEqual(self.request("POST", "/perfil/111/foto", photo,
                                      "application/x-mensinger-bgr565")[0], 200)
        data = {"username": "maria", "nickname": "Mari", "bio": "Hola",
                "description": "Pruebas desde 3DS"}
        self.assertEqual(self.request("POST", "/perfil/111", json.dumps(data).encode(),
                                      "application/json")[0], 200)
        status, body = self.request("GET", "/perfil/111")
        self.assertEqual(status, 200)
        self.assertEqual(json.loads(body)["nickname"], "Mari")
        self.assertTrue(json.loads(body)["has_photo"])
        self.assertEqual(self.request("GET", "/perfil/111/foto"), (200, photo))
        self.assertEqual(self.request("POST", "/perfil/111", b"{}", "application/json")[0], 400)
        self.assertEqual(self.request("POST", "/perfil/111/foto", b"bad",
                                      "application/x-mensinger-bgr565")[0], 413)

    def test_chat_photo_thumbnail_and_access(self):
        """La miniatura mantiene proporciones, se regenera y respeta los participantes."""
        photo = b"".join(struct.pack("<H", (x // 5) & 31)
                         for _y in range(240) for x in range(400))
        status, body = self.request("POST", "/foto/111/222", photo,
                                    "application/x-mensinger-bgr565")
        self.assertEqual(status, 200)
        message_id = int(body)
        path = f"/foto-miniatura/{message_id}"
        self.assertEqual(self.request("GET", path, headers={"X-User-ID": "333"})[0], 403)
        status, thumb = self.request("GET", path, headers={"X-User-ID": "111"})
        self.assertEqual(status, 200)
        self.assertEqual(len(thumb), 80 * 48 * 2)
        self.assertEqual(thumb[:4], struct.pack("<HH", 0, 1))
        with sqlite3.connect(self.db) as connection:
            connection.execute("UPDATE messages SET image_thumb = NULL WHERE id = ?", (message_id,))
        self.assertEqual(self.request("GET", path, headers={"X-User-ID": "222"}), (200, thumb))

    def test_password_protects_reads_writes_and_media(self):
        self.assertEqual(self.request("GET", "/acceso/111"), (200, b"LIBRE"))
        payload = json.dumps({"password": "clave segura"}).encode()
        status, token = self.request("POST", "/contrasena/111", payload, "application/json")
        self.assertEqual(status, 200)
        self.assertTrue(token)
        self.assertEqual(self.request("GET", "/acceso/111"), (200, b"PROTEGIDO"))
        self.assertEqual(self.request("GET", "/historial/111/222")[0], 401)
        self.assertEqual(self.request("GET", "/notificaciones/111")[0], 401)
        self.assertEqual(self.request("GET", "/conteo/111/222")[0], 401)
        text = json.dumps({"de": "111", "para": "222", "texto": "secreto"}).encode()
        self.assertEqual(self.request("POST", "/enviar", text, "application/json")[0], 401)
        self.assertEqual(self.request("POST", "/perfil/111", b"{}", "application/json")[0], 401)
        self.assertEqual(self.request("GET", "/audio/1", headers={"X-User-ID": "333"})[0], 404)
        wrong = json.dumps({"password": "otra"}).encode()
        self.assertEqual(self.request("POST", "/acceso/111", wrong, "application/json")[0], 401)
        status, logged_in = self.request("POST", "/acceso/111", payload, "application/json")
        self.assertEqual(status, 200)
        auth = {"Authorization": "Bearer " + logged_in.decode()}
        self.assertEqual(self.request("GET", "/historial/111/222", headers=auth)[0], 200)
        self.assertEqual(self.request("POST", "/enviar", text, "application/json", auth)[0], 200)
        self.assertEqual(self.request("GET", "/conteo/111/222", headers=auth), (200, b"2"))
        status, audio_id = self.request("POST", "/audio/111/222", wav(1), "audio/wav", auth)
        self.assertEqual(status, 200)
        self.assertEqual(self.request("GET", f"/audio/{int(audio_id)}",
                                      headers={"X-User-ID": "111"})[0], 401)
        self.assertEqual(self.request("GET", f"/audio/{int(audio_id)}",
                                      headers={"X-User-ID": "111", **auth})[0], 200)
        self.assertEqual(self.request("POST", "/contrasena/111", b'{"password":""}',
                                      "application/json", auth), (200, b"LIBRE"))
        self.assertEqual(self.request("GET", "/historial/111/222")[0], 200)

    def test_search_visibility_and_meet_people(self):
        data = {"username": "Maria", "nickname": "Mari", "bio": "Hola a todos",
                "description": "Amistad", "show_in_search": True, "meet_people": False}
        self.assertEqual(self.request("POST", "/perfil/111", json.dumps(data).encode(),
                                      "application/json")[0], 200)
        self.assertIn(b"111\tMaria\tMari\tHola a todos", self.request("GET", "/buscar?q=mar")[1])
        self.assertNotIn(b"111", self.request("GET", "/buscar?modo=conocer")[1])
        data["meet_people"] = True
        data["show_in_search"] = False
        self.request("POST", "/perfil/111", json.dumps(data).encode(), "application/json")
        self.assertNotIn(b"111", self.request("GET", "/buscar?q=mar")[1])
        self.assertIn(b"111", self.request("GET", "/buscar?modo=conocer")[1])

    def test_profile_colors_are_shared_and_validated(self):
        data = {"username": "Mara", "nickname": "Mar", "bio": "Hola",
                "description": "Perfil", "chat_color": 2, "background_color": 3,
                "text_color": 4, "nickname_color": 5, "name_color": 1}
        self.assertEqual(self.request("POST", "/perfil/111", json.dumps(data).encode(),
                                      "application/json")[0], 200)
        status, profile = self.request("GET", "/perfil/111")
        self.assertEqual(status, 200)
        self.assertEqual(json.loads(profile)["background_color"], 3)
        self.assertEqual(json.loads(profile)["name_color"], 1)
        self.assertIn(b"111\tMara\tMar\tHola\tPerfil\t0\t2\t3\t4\t5\t1",
                      self.request("GET", "/perfiles?ids=111")[1])
        data["chat_color"] = 6
        self.assertEqual(self.request("POST", "/perfil/111", json.dumps(data).encode(),
                                      "application/json")[0], 400)

    def test_search_matches_fragments_without_case_or_accent(self):
        data = {"username": "José Luis", "nickname": "Pepe", "bio": "Hola",
                "description": "Perfil", "show_in_search": True}
        self.request("POST", "/perfil/111", json.dumps(data).encode(), "application/json")
        self.assertIn(b"111", self.request("GET", "/buscar?q=SE%20LU")[1])
        self.assertIn(b"111", self.request("GET", "/buscar?q=jose")[1])
        self.assertIn(b"111", self.request("GET", "/buscar?q=pep")[1])

    def test_encrypted_migration_key_and_tamper_detection(self):
        with sqlite3.connect(self.db) as connection:
            encrypted = connection.execute("SELECT body FROM messages WHERE id=1").fetchone()[0]
        self.assertTrue(encrypted.startswith("enc:v1:"))
        self.assertNotIn("antiguo", encrypted)
        self.assertEqual(self.server.cipher.open_text("messages", 1, "body", encrypted), "antiguo")
        self.assertFalse(migrate_content(self.db, self.server.cipher))
        with self.assertRaises(ValueError):
            self.server.cipher.open_text("messages", 2, "body", encrypted)
        with self.assertRaises(ValueError):
            migrate_content(self.db, ContentCipher(b"x" * 32))
        self.assertEqual(len(default_key_path(self.db).read_bytes()), 32)

    def test_secure_channel_authentication_and_replay(self):
        """Comprueba interoperabilidad del sobre, rechazo de cambios y replay."""
        channel = SecureChannel(Path(self.temp.name) / "transport.key")
        self.assertEqual(SecureChannel(channel.key_path).public_key, channel.public_key)
        public_packet = channel.issue_challenge()
        self.assertEqual(public_packet[:4], b"M3K1")
        server_public, challenge = public_packet[4:36], public_packet[36:52]
        private = x25519.X25519PrivateKey.generate()
        client_public = private.public_key().public_bytes(
            serialization.Encoding.Raw, serialization.PublicFormat.Raw)
        shared = private.exchange(x25519.X25519PublicKey.from_public_bytes(server_public))
        key = hashlib.blake2b(shared + client_public + server_public + challenge + KDF_LABEL,
                              digest_size=32).digest()
        plaintext = struct.pack(">BHBBBI", 0, 8, 0, 0, 0, 0) + b"/version"
        encrypted = ChaCha20Poly1305(key).encrypt(REQUEST_NONCE, plaintext, REQUEST_AAD)
        packet = b"M3Q1" + challenge + client_public + encrypted[-16:] + encrypted[:-16]
        self.assertEqual(channel.open_request(packet)[0],
                         ("GET", "/version", "", "", "", b""))
        with self.assertRaises(ValueError):
            channel.open_request(packet)
        response = channel.seal_response(200, b"v1.4.2", key)
        plain = ChaCha20Poly1305(key).decrypt(RESPONSE_NONCE,
                                              response[20:] + response[4:20], RESPONSE_AAD)
        self.assertEqual(plain, struct.pack(">HI", 200, 6) + b"v1.4.2")
        bad = bytearray(channel.issue_challenge())
        bad[4] ^= 1
        self.assertNotEqual(bytes(bad[4:36]), channel.public_key)

    def test_encrypted_http_routes(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(timeout=2)
        self.server = MensingerServer(("127.0.0.1", 0), self.db, "v1.1.0",
                                      encrypted_transport=True)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.assertEqual(self.request("GET", "/version")[0], 403)
        self.assertEqual(self.request("POST", "/enviar", b"secreto", "application/json")[0], 403)
        status, public_packet = self.request("GET", "/clave-servidor")
        self.assertEqual(status, 200)
        server_public, challenge = public_packet[4:36], public_packet[36:52]
        private = x25519.X25519PrivateKey.generate()
        client_public = private.public_key().public_bytes(
            serialization.Encoding.Raw, serialization.PublicFormat.Raw)
        key = hashlib.blake2b(
            private.exchange(x25519.X25519PublicKey.from_public_bytes(server_public)) +
            client_public + server_public + challenge + KDF_LABEL, digest_size=32).digest()
        plain = struct.pack(">BHBBBI", 0, 8, 0, 0, 0, 0) + b"/version"
        encrypted = ChaCha20Poly1305(key).encrypt(REQUEST_NONCE, plain, REQUEST_AAD)
        packet = b"M3Q1" + challenge + client_public + encrypted[-16:] + encrypted[:-16]
        self.assertNotIn(b"/version", packet)
        status, response = self.request("POST", "/canal", packet, "application/octet-stream")
        self.assertEqual(status, 200)
        decoded = ChaCha20Poly1305(key).decrypt(
            RESPONSE_NONCE, response[20:] + response[4:20], RESPONSE_AAD)
        self.assertEqual(decoded, struct.pack(">HI", 200, 6) + b"v1.1.0")
        self.assertEqual(self.request("POST", "/canal", packet,
                                      "application/octet-stream")[0], 400)

        def sealed_request(method, path, body=b"", content_type="", user=""):
            status, identity = self.request("GET", "/clave-servidor")
            self.assertEqual(status, 200)
            public, nonce = identity[4:36], identity[36:52]
            secret = x25519.X25519PrivateKey.generate()
            ephemeral = secret.public_key().public_bytes(
                serialization.Encoding.Raw, serialization.PublicFormat.Raw)
            session_key = hashlib.blake2b(
                secret.exchange(x25519.X25519PublicKey.from_public_bytes(public)) +
                ephemeral + public + nonce + KDF_LABEL, digest_size=32).digest()
            route, kind, owner = path.encode(), content_type.encode(), user.encode()
            inner = struct.pack(">BHBBBI", 0 if method == "GET" else 1,
                                len(route), len(kind), len(owner), 0, len(body)) + \
                route + kind + owner + body
            encrypted_inner = ChaCha20Poly1305(session_key).encrypt(
                REQUEST_NONCE, inner, REQUEST_AAD)
            outer = b"M3Q1" + nonce + ephemeral + encrypted_inner[-16:] + encrypted_inner[:-16]
            self.assertNotIn(route, outer)
            if body:
                self.assertNotIn(body, outer)
            status, reply = self.request("POST", "/canal", outer, "application/octet-stream")
            self.assertEqual(status, 200)
            result = ChaCha20Poly1305(session_key).decrypt(
                RESPONSE_NONCE, reply[20:] + reply[4:20], RESPONSE_AAD)
            code, size = struct.unpack(">HI", result[:6])
            self.assertEqual(size, len(result) - 6)
            return code, result[6:]

        message = json.dumps({"de": "111", "para": "222", "texto": "mensaje oculto"}).encode()
        self.assertEqual(sealed_request("POST", "/enviar", message,
                                        "application/json"), (200, b"OK"))
        code, history = sealed_request("GET", "/historial/111/222?limit=20&before=0")
        self.assertEqual(code, 200)
        self.assertIn(b"mensaje oculto", history)

    def test_cli_can_start_without_transport_encryption(self):
        """El parámetro 'no' debe exponer HTTP real sin exigir archivos TLS."""
        with socket.socket() as reservation:
            reservation.bind(("127.0.0.1", 0))
            port = reservation.getsockname()[1]
        root = Path(self.temp.name) / "plain-mode"
        root.mkdir()
        command = [sys.executable, str(Path(__file__).with_name("server.py")),
                   "--host", "127.0.0.1", "--port", str(port),
                   "--db", str(root / "messages.sqlite3"),
                   "--cifrado-conexion", "no"]
        flags = subprocess.CREATE_NO_WINDOW if sys.platform == "win32" else 0
        process = subprocess.Popen(command, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,
                                   creationflags=flags)
        try:
            response = None
            deadline = time.monotonic() + 8
            while time.monotonic() < deadline:
                if process.poll() is not None:
                    self.fail("El servidor HTTP terminó antes de atender peticiones: " +
                              process.stderr.read().decode(errors="replace"))
                try:
                    client = http.client.HTTPConnection("127.0.0.1", port, timeout=1)
                    client.request("GET", "/version")
                    answer = client.getresponse()
                    response = (answer.status, answer.read())
                    client.close()
                    break
                except (OSError, http.client.HTTPException):
                    time.sleep(0.1)
            self.assertEqual(response, (200, b"v1.4.4"))
            self.assertFalse((root / "transport.key").exists())
            self.assertTrue((root / "messages.key").exists())
        finally:
            process.terminate()
            process.wait(timeout=5)
            process.stderr.close()

if __name__ == "__main__":
    unittest.main()
