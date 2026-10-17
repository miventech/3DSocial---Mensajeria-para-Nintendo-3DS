"""Pruebas del protocolo HTTP y de la migración de la base de datos."""

import http.client
import json
import sqlite3
import struct
import tempfile
import threading
import unittest
from pathlib import Path

from server import MensingerServer, create_database, wav_duration_ms


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
        self.server = MensingerServer(("127.0.0.1", 0), self.db, "v1.1.0")
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()

    def tearDown(self):
        """Apaga el servidor de pruebas y elimina los archivos temporales."""
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(timeout=2)
        self.temp.cleanup()

    def request(self, method, path, body=None, content_type=None):
        """Envía una petición local y devuelve estado HTTP junto con el cuerpo."""
        connection = http.client.HTTPConnection("127.0.0.1", self.server.server_port, timeout=3)
        headers = {"Content-Type": content_type} if content_type else {}
        connection.request(method, path, body=body, headers=headers)
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

    def test_audio_upload_download_and_limits(self):
        """Comprueba persistencia WAV, descarga y rechazo de formato/duración inválidos."""
        audio = wav(2)
        self.assertEqual(wav_duration_ms(audio), 2000)
        status, body = self.request("POST", "/audio/111/222", audio, "audio/wav")
        self.assertEqual(status, 200)
        message_id = int(body)
        self.assertEqual(self.request("GET", f"/audio/{message_id}"), (200, audio))
        status, history = self.request("GET", "/historial/111/222")
        self.assertEqual(status, 200)
        self.assertIn(f"{message_id}\t111\taudio\t2000\t".encode(), history)
        with sqlite3.connect(self.db) as connection:
            persisted = connection.execute(
                "SELECT audio_data FROM messages WHERE id = ?", (message_id,)
            ).fetchone()[0]
        self.assertEqual(persisted, audio)
        self.assertEqual(self.request("POST", "/audio/111/222", wav(30), "audio/wav")[0], 200)
        self.assertEqual(self.request("POST", "/audio/111/222", b"bad", "audio/wav")[0], 413)
        self.assertEqual(self.request("POST", "/audio/111/222", wav(31), "audio/wav")[0], 413)
        self.assertEqual(self.request("GET", "/audio/999999")[0], 404)

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

if __name__ == "__main__":
    unittest.main()
