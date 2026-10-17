"""Servidor HTTP de ejemplo compatible con el cliente Mensinger 3DS.

Valida el protocolo de la consola, aplica autorización por perfil y cifra o
descifra los campos de contenido al entrar/salir de SQLite. La CLI permite
activar un canal de aplicación cifrado sobre HTTP; el cifrado de la base sigue
activo en ambos modos.
"""

import argparse
import hashlib
import hmac
import io
import json
import os
import re
import secrets
import sqlite3
import struct
import threading
import time
import unicodedata
from contextlib import contextmanager
from email.message import Message
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlsplit

from secure_store import ContentCipher, default_key_path, migrate_content
from secure_channel import MAX_ENVELOPE, SecureChannel


ID_PATTERN = re.compile(r"[0-9]{1,10}\Z")
MAX_REQUEST_BYTES = 2048
# El cliente de 3DS lee la respuesta una sola vez en un buffer de 2048 bytes.
MAX_CHAT_BYTES = 1400
SAMPLE_RATE = 10910
MAX_AUDIO_SAMPLES = SAMPLE_RATE * 30
MAX_AUDIO_BYTES = 44 + MAX_AUDIO_SAMPLES * 2
PHOTO_WIDTH = 400
PHOTO_HEIGHT = 240
PHOTO_BYTES = PHOTO_WIDTH * PHOTO_HEIGHT * 2
CHAT_THUMB_WIDTH = 80
CHAT_THUMB_HEIGHT = 48
CHAT_THUMB_BYTES = CHAT_THUMB_WIDTH * CHAT_THUMB_HEIGHT * 2
PROFILE_THUMB_SIZE = 32
PROFILE_THUMB_BYTES = PROFILE_THUMB_SIZE * PROFILE_THUMB_SIZE * 2


@contextmanager
def open_database(path):
    """Abre SQLite con espera ante bloqueos y cierra siempre la conexión.

    El contexto de escritura de sqlite3 confirma los cambios al salir sin
    errores y los revierte si una excepción interrumpe la operación.
    """
    connection = sqlite3.connect(path, timeout=5)
    connection.execute("PRAGMA busy_timeout = 5000")
    try:
        with connection:
            yield connection
    finally:
        connection.close()


def create_database(path):
    """Crea el esquema o migra una base anterior sin borrar sus mensajes."""
    path.parent.mkdir(parents=True, exist_ok=True)
    with open_database(path) as connection:
        connection.execute(
            """CREATE TABLE IF NOT EXISTS messages (
                   id INTEGER PRIMARY KEY AUTOINCREMENT,
                   sender TEXT NOT NULL,
                   recipient TEXT NOT NULL,
                   body TEXT NOT NULL,
                   notification_delivered INTEGER NOT NULL DEFAULT 0,
                   created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
               )"""
        )
        connection.execute(
            "CREATE INDEX IF NOT EXISTS messages_chat ON messages(sender, recipient, id)"
        )
        connection.execute(
            "CREATE INDEX IF NOT EXISTS messages_notifications "
            "ON messages(recipient, notification_delivered, id)"
        )
        columns = {row[1] for row in connection.execute("PRAGMA table_info(messages)")}
        if "kind" not in columns:
            connection.execute("ALTER TABLE messages ADD COLUMN kind TEXT NOT NULL DEFAULT 'texto'")
        if "duration_ms" not in columns:
            connection.execute("ALTER TABLE messages ADD COLUMN duration_ms INTEGER NOT NULL DEFAULT 0")
        if "audio_data" not in columns:
            connection.execute("ALTER TABLE messages ADD COLUMN audio_data BLOB")
        if "image_data" not in columns:
            connection.execute("ALTER TABLE messages ADD COLUMN image_data BLOB")
        if "image_thumb" not in columns:
            connection.execute("ALTER TABLE messages ADD COLUMN image_thumb BLOB")
        if "image_width" not in columns:
            connection.execute("ALTER TABLE messages ADD COLUMN image_width INTEGER NOT NULL DEFAULT 0")
        if "image_height" not in columns:
            connection.execute("ALTER TABLE messages ADD COLUMN image_height INTEGER NOT NULL DEFAULT 0")
        connection.execute(
            """CREATE TABLE IF NOT EXISTS profiles (
                   user_id TEXT PRIMARY KEY,
                   username TEXT NOT NULL DEFAULT '',
                   nickname TEXT NOT NULL DEFAULT '',
                   bio TEXT NOT NULL DEFAULT '',
                   description TEXT NOT NULL DEFAULT '',
                   photo_data BLOB,
                   photo_thumb BLOB,
                   created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
                   updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
               )"""
        )
        profile_columns = {row[1] for row in connection.execute("PRAGMA table_info(profiles)")}
        if "photo_thumb" not in profile_columns:
            connection.execute("ALTER TABLE profiles ADD COLUMN photo_thumb BLOB")
        if "created_at" not in profile_columns:
            # Los perfiles antiguos no guardaban alta: updated_at es la mejor fecha disponible.
            connection.execute("ALTER TABLE profiles ADD COLUMN created_at TEXT")
            connection.execute("UPDATE profiles SET created_at = updated_at WHERE created_at IS NULL")
        connection.execute("""CREATE TRIGGER IF NOT EXISTS profiles_created_at
            AFTER INSERT ON profiles WHEN NEW.created_at IS NULL
            BEGIN UPDATE profiles SET created_at = CURRENT_TIMESTAMP
                  WHERE user_id = NEW.user_id; END""")
        for name, definition in (("password_salt", "BLOB"), ("password_hash", "BLOB"),
                                 ("show_in_search", "INTEGER NOT NULL DEFAULT 0"),
                                 ("meet_people", "INTEGER NOT NULL DEFAULT 0"),
                                 ("chat_color", "INTEGER NOT NULL DEFAULT 0"),
                                 ("background_color", "INTEGER NOT NULL DEFAULT 0"),
                                 ("text_color", "INTEGER NOT NULL DEFAULT 0"),
                                 ("nickname_color", "INTEGER NOT NULL DEFAULT 0"),
                                 ("name_color", "INTEGER NOT NULL DEFAULT 0")):
            if name not in profile_columns:
                connection.execute("ALTER TABLE profiles ADD COLUMN " + name + " " + definition)


class MensingerServer(ThreadingHTTPServer):
    """Servidor HTTP concurrente que comparte la ruta SQLite y la versión."""
    daemon_threads = True
    allow_reuse_address = True

    def __init__(self, address, database_path, version, key_path=None,
                 encrypted_transport=True, transport_key_path=None):
        """Configura los datos que consultarán los manejadores de cada petición."""
        super().__init__(address, MensingerHandler)
        self.database_path = database_path
        self.app_version = version
        self.encrypted_transport = encrypted_transport
        self.secure_channel = (SecureChannel(transport_key_path or
                               database_path.with_name("transport.key"))
                               if encrypted_transport else None)
        self.sessions = {}
        self.presence = {}
        self.presence_lock = threading.Lock()
        self.cipher = ContentCipher.from_file(key_path or default_key_path(database_path),
                                              database_path, create=True)
        migrate_content(database_path, self.cipher)

    def issue_session(self, user_id):
        """Emite un token bearer aleatorio en memoria para una identidad validada."""
        token = secrets.token_urlsafe(32)
        self.sessions[token] = user_id
        return token


class MensingerHandler(BaseHTTPRequestHandler):
    """Implementa las rutas HTTP que consume la app de Nintendo 3DS.

    La clase base registra cada petición en la consola con cliente, ruta y
    estado HTTP; los métodos de esta clase no imprimen el contenido del chat.
    """
    protocol_version = "HTTP/1.1"
    server_version = "MensingerExample/1.4"
    sys_version = ""

    def send_bytes(self, status, payload, content_type="text/plain; charset=utf-8"):
        """Envía cabeceras y cuerpo juntos para el lector HTTP sencillo de 3DS."""
        if getattr(self, "_inside_secure", False):
            self._secure_result = (status, payload)
            return
        # Envía cabeceras con el primer bloque y limita el tamaño de cada envío
        # para que las respuestas grandes de fotos no bloqueen el socket.
        header = (
            "HTTP/1.1 {} {}\r\n"
            "Content-Type: {}\r\n"
            "Content-Length: {}\r\n"
            "Cache-Control: no-store\r\n"
            "Connection: close\r\n\r\n"
        ).format(status, HTTPStatus(status).phrase, content_type, len(payload)).encode("ascii")
        self.close_connection = True
        try:
            self.connection.sendall(header + payload[:16384])
            for offset in range(16384, len(payload), 16384):
                self.connection.sendall(payload[offset:offset + 16384])
        except (BrokenPipeError, ConnectionResetError):
            pass

    def send_text(self, status, text):
        """Codifica una respuesta textual UTF-8 y delega el envío común."""
        self.send_bytes(status, text.encode("utf-8"))

    def authorized(self, user_id):
        """Permite perfiles libres o exige un token válido en perfiles protegidos.

        Si un POST falla autorización, consume su cuerpo acotado antes de
        responder para que el socket no conserve bytes residuales de la carga.
        """
        with open_database(self.server.database_path) as connection:
            row = connection.execute("SELECT password_hash FROM profiles WHERE user_id = ?",
                                     (user_id,)).fetchone()
        if row is None or row[0] is None:
            return True
        authorization = self.headers.get("Authorization", "")
        token = authorization[7:] if authorization.startswith("Bearer ") else ""
        if token and hmac.compare_digest(self.server.sessions.get(token, ""), user_id):
            return True
        if self.command == "POST" and not getattr(self, "_body_consumed", False):
            try:
                remaining = int(self.headers.get("Content-Length", "0"))
            except ValueError:
                remaining = 0
            if 0 < remaining <= MAX_AUDIO_BYTES + 65536:
                while remaining:
                    chunk = self.rfile.read(min(16384, remaining))
                    if not chunk:
                        break
                    remaining -= len(chunk)
        self.send_text(401, "Se requiere contrasena")
        return False

    def read_json(self):
        """Lee un objeto JSON pequeño y comunica el error HTTP concreto al fallar."""
        if self.headers.get_content_type() != "application/json":
            self.send_text(415, "Se requiere application/json")
            return None
        try:
            length = int(self.headers.get("Content-Length", ""))
        except ValueError:
            self.send_text(411, "Falta Content-Length")
            return None
        if length < 1 or length > MAX_REQUEST_BYTES:
            self.send_text(413, "Solicitud demasiado grande")
            return None
        try:
            raw = self.rfile.read(length)
            self._body_consumed = True
            data = json.loads(raw.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError):
            self.send_text(400, "JSON invalido")
            return None
        if not isinstance(data, dict):
            self.send_text(400, "Se requiere un objeto JSON")
            return None
        return data

    def get_access(self, user_id):
        """Crea un perfil vacío si hace falta y anuncia si pide contraseña."""
        with open_database(self.server.database_path) as connection:
            connection.execute("INSERT OR IGNORE INTO profiles(user_id) VALUES (?)", (user_id,))
            protected = connection.execute("SELECT password_hash IS NOT NULL FROM profiles "
                                           "WHERE user_id = ?", (user_id,)).fetchone()[0]
        self.send_text(200, "PROTEGIDO" if protected else "LIBRE")

    def post_access(self, user_id):
        """Verifica contraseña con PBKDF2 y entrega una sesión temporal."""
        data = self.read_json()
        if data is None:
            return
        password = data.get("password")
        if not isinstance(password, str) or len(password.encode("utf-8")) > 128:
            self.send_text(400, "Contrasena invalida")
            return
        with open_database(self.server.database_path) as connection:
            row = connection.execute("SELECT password_salt, password_hash FROM profiles "
                                     "WHERE user_id = ?", (user_id,)).fetchone()
        if row is None or row[1] is None:
            self.send_text(400, "Esta cuenta no requiere contrasena")
            return
        candidate = hashlib.pbkdf2_hmac("sha256", password.encode("utf-8"), row[0], 200000)
        if not hmac.compare_digest(candidate, row[1]):
            self.send_text(401, "Contrasena incorrecta")
            return
        self.send_text(200, self.server.issue_session(user_id))

    def post_password(self, user_id):
        """Define o quita contraseña e invalida tokens anteriores del usuario."""
        if not self.authorized(user_id):
            return
        data = self.read_json()
        if data is None:
            return
        password = data.get("password")
        if not isinstance(password, str) or len(password.encode("utf-8")) > 128:
            self.send_text(400, "Contrasena invalida")
            return
        salt = os.urandom(16) if password else None
        digest = hashlib.pbkdf2_hmac("sha256", password.encode("utf-8"), salt, 200000) if salt else None
        with open_database(self.server.database_path) as connection:
            connection.execute("INSERT INTO profiles(user_id, password_salt, password_hash) "
                               "VALUES (?, ?, ?) ON CONFLICT(user_id) DO UPDATE SET "
                               "password_salt=excluded.password_salt, "
                               "password_hash=excluded.password_hash", (user_id, salt, digest))
        self.server.sessions = {token: owner for token, owner in self.server.sessions.items()
                                if owner != user_id}
        self.send_text(200, self.server.issue_session(user_id) if password else "LIBRE")

    def search_profiles(self, query):
        """Filtra perfiles opt-in por subcadena normalizada de nombre o apodo."""
        params = parse_qs(query)
        mode = params.get("modo", ["nombre"])[0]
        term = params.get("q", [""])[0].strip()
        if mode not in ("nombre", "conocer") or len(term) > 32 or "\x00" in term:
            self.send_text(400, "Busqueda invalida")
            return
        if mode == "nombre" and not term:
            self.send_text(200, "")
            return
        field = "meet_people" if mode == "conocer" else "show_in_search"
        order = "updated_at DESC, user_id" if mode == "conocer" else "username COLLATE NOCASE, user_id"
        with open_database(self.server.database_path) as connection:
            candidates = connection.execute(
                "SELECT user_id, username, nickname, bio FROM profiles WHERE " + field +
                " = 1 ORDER BY " + order).fetchall()
        needle = search_key(term)
        rows = [row for row in candidates if not needle or
                needle in search_key(row[1]) or needle in search_key(row[2])][:20]
        lines = ["{}\t{}\t{}\t{}\n".format(*row) for row in rows]
        self.send_text(200, "".join(lines))

    def post_secure_channel(self):
        """Descifra una petición completa y cifra su respuesta sin exponer rutas.

        Solo /clave-servidor y /canal son visibles en HTTP. El despachador
        interno reutiliza las validaciones y autorizaciones de todas las rutas.
        """
        if self.headers.get_content_type() != "application/octet-stream":
            self.send_text(415, "Se requiere sobre binario")
            return
        try:
            length = int(self.headers.get("Content-Length", ""))
        except ValueError:
            self.send_text(411, "Falta Content-Length")
            return
        if not 68 <= length <= MAX_ENVELOPE:
            self.send_text(413, "Sobre demasiado grande")
            return
        packet = self.rfile.read(length)
        if len(packet) != length:
            self.send_text(400, "Sobre incompleto")
            return
        try:
            request, key = self.server.secure_channel.open_request(packet)
        except ValueError:
            self.send_text(400, "Sobre o desafío inválido")
            return
        method, path, content_type, user_id, token, body = request
        old = (self.command, self.path, self.headers, self.rfile)
        self.command, self.path, self.rfile = method, path, io.BytesIO(body)
        headers = Message()
        if content_type:
            headers["Content-Type"] = content_type
        headers["Content-Length"] = str(len(body))
        if user_id:
            headers["X-User-ID"] = user_id
        if token:
            headers["Authorization"] = "Bearer " + token
        self.headers = headers
        self._inside_secure = True
        self._secure_result = None
        self._body_consumed = False
        try:
            if method == "GET":
                self.do_GET()
            else:
                self.do_POST()
            status, response = self._secure_result or (500, b"Error interno")
        except Exception:
            self.log_error("Error interno en petición cifrada")
            status, response = 500, b"Error interno"
        finally:
            self.command, self.path, self.headers, self.rfile = old
            self._inside_secure = False
        self.send_bytes(200, self.server.secure_channel.seal_response(status, response, key),
                        "application/octet-stream")

    def do_GET(self):
        """Selecciona una ruta de lectura y valida sus segmentos antes de usarla."""
        if not getattr(self, "_inside_secure", False) and self.server.encrypted_transport:
            if self.path == "/clave-servidor":
                try:
                    self.send_bytes(200, self.server.secure_channel.issue_challenge(),
                                    "application/octet-stream")
                except ValueError:
                    self.send_text(503, "Intenta de nuevo")
            else:
                self.send_text(403, "Se requiere canal cifrado")
            return
        parsed = urlsplit(self.path)
        parts = parsed.path.strip("/").split("/")
        try:
            if parts == ["version"]:
                self.send_text(200, self.server.app_version)
            elif parts == ["perfiles"]:
                self.get_profiles(parse_qs(parsed.query).get("ids", [""])[0])
            elif parts == ["perfil-miniaturas"]:
                self.get_profile_thumbnails(parse_qs(parsed.query).get("ids", [""])[0])
            elif parts == ["buscar"]:
                self.search_profiles(parsed.query)
            elif len(parts) == 2 and parts[0] == "acceso" and valid_id(parts[1]):
                self.get_access(parts[1])
            elif len(parts) == 2 and parts[0] == "notificaciones" and valid_id(parts[1]):
                self.get_notification(parts[1])
            elif len(parts) == 2 and parts[0] == "remitentes" and valid_id(parts[1]):
                self.get_senders(parts[1])
            elif len(parts) == 2 and parts[0] == "presencias" and valid_id(parts[1]):
                self.get_presence(parts[1], parse_qs(parsed.query).get("ids", [""])[0])
            elif (len(parts) == 3 and parts[0] == "conteo"
                  and valid_id(parts[1]) and valid_id(parts[2])):
                self.get_count(parts[1], parts[2])
            elif (len(parts) == 3 and parts[0] == "obtener_chat"
                  and valid_id(parts[1]) and valid_id(parts[2])):
                self.get_chat(parts[1], parts[2])
            elif (len(parts) == 3 and parts[0] == "historial"
                  and valid_id(parts[1]) and valid_id(parts[2])):
                self.get_history(parts[1], parts[2], parsed.query)
            elif len(parts) == 2 and parts[0] == "audio" and parts[1].isdigit():
                self.get_audio(int(parts[1]))
            elif len(parts) == 2 and parts[0] == "foto" and parts[1].isdigit():
                self.get_photo(int(parts[1]))
            elif len(parts) == 2 and parts[0] == "foto-miniatura" and parts[1].isdigit():
                self.get_photo_thumbnail(int(parts[1]))
            elif len(parts) == 2 and parts[0] == "perfil" and valid_id(parts[1]):
                self.get_profile(parts[1])
            elif len(parts) == 3 and parts[0] == "perfil" and valid_id(parts[1]) and parts[2] == "foto":
                self.get_profile_photo(parts[1])
            else:
                self.send_text(404, "Ruta no encontrada")
        except (sqlite3.Error, ValueError):
            self.log_error("Error de base de datos en GET %s", self.path)
            self.send_text(500, "Error interno")

    def do_POST(self):
        """Distribuye las cargas binarias y JSON a su ruta de escritura."""
        if not getattr(self, "_inside_secure", False) and self.server.encrypted_transport:
            if self.path == "/canal":
                self.post_secure_channel()
            else:
                self.send_text(403, "Se requiere canal cifrado")
            return
        path = urlsplit(self.path).path
        audio_parts = path.strip("/").split("/")
        if len(audio_parts) == 2 and audio_parts[0] == "perfil" and valid_id(audio_parts[1]):
            self.post_profile(audio_parts[1])
            return
        if len(audio_parts) == 2 and audio_parts[0] == "acceso" and valid_id(audio_parts[1]):
            self.post_access(audio_parts[1])
            return
        if len(audio_parts) == 2 and audio_parts[0] == "contrasena" and valid_id(audio_parts[1]):
            self.post_password(audio_parts[1])
            return
        if len(audio_parts) == 2 and audio_parts[0] == "presencia" and valid_id(audio_parts[1]):
            self.post_presence(audio_parts[1])
            return
        if (len(audio_parts) == 3 and audio_parts[0] == "perfil"
                and valid_id(audio_parts[1]) and audio_parts[2] == "foto"):
            self.post_profile_photo(audio_parts[1])
            return
        if (len(audio_parts) == 3 and audio_parts[0] == "audio"
                and valid_id(audio_parts[1]) and valid_id(audio_parts[2])):
            self.post_audio(audio_parts[1], audio_parts[2])
            return
        if (len(audio_parts) == 3 and audio_parts[0] == "foto"
                and valid_id(audio_parts[1]) and valid_id(audio_parts[2])):
            self.post_photo(audio_parts[1], audio_parts[2])
            return
        if path != "/enviar":
            self.send_text(404, "Ruta no encontrada")
            return

        if self.headers.get_content_type() != "application/json":
            self.send_text(415, "Se requiere application/json")
            return

        try:
            length = int(self.headers.get("Content-Length", ""))
        except ValueError:
            self.send_text(411, "Falta Content-Length")
            return
        if length < 1 or length > MAX_REQUEST_BYTES:
            self.send_text(413, "Mensaje demasiado grande")
            return

        try:
            raw = self.rfile.read(length)
            self._body_consumed = True
            data = json.loads(raw.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError):
            self.send_text(400, "JSON invalido")
            return

        if not isinstance(data, dict):
            self.send_text(400, "Se requiere un objeto JSON")
            return
        sender, recipient, body = data.get("de"), data.get("para"), data.get("texto")
        if not valid_id(sender) or not valid_id(recipient):
            self.send_text(400, "IDs invalidos")
            return
        if not self.authorized(sender):
            return
        if not isinstance(body, str) or not body.strip() or len(body) > 120:
            self.send_text(400, "El texto debe tener entre 1 y 120 caracteres")
            return
        if any(ord(character) < 32 for character in body):
            self.send_text(400, "El texto no puede contener saltos de linea ni controles")
            return

        try:
            with open_database(self.server.database_path) as connection:
                cursor = connection.execute(
                    "INSERT INTO messages(sender, recipient, body) VALUES (?, ?, '')",
                    (sender, recipient))
                connection.execute("UPDATE messages SET body=? WHERE id=?", (
                    self.server.cipher.seal_text("messages", cursor.lastrowid, "body", body),
                    cursor.lastrowid))
        except (sqlite3.Error, ValueError):
            self.log_error("Error de base de datos en POST /enviar")
            self.send_text(500, "Error interno")
            return
        self.send_text(200, "OK")

    def get_notification(self, recipient):
        """Devuelve el remitente pendiente más antiguo y marca esa pareja como leída."""
        if not self.authorized(recipient):
            return
        with open_database(self.server.database_path) as connection:
            row = connection.execute(
                "SELECT sender FROM messages "
                "WHERE recipient = ? AND notification_delivered = 0 "
                "ORDER BY id LIMIT 1",
                (recipient,),
            ).fetchone()
            if row is None:
                sender = "NINGUNO"
            else:
                sender = row[0]
                connection.execute(
                    "UPDATE messages SET notification_delivered = 1 "
                    "WHERE recipient = ? AND sender = ? AND notification_delivered = 0",
                    (recipient, sender),
                )
        self.send_text(200, sender)

    def get_senders(self, recipient):
        """Devuelve todos los remitentes recientes sin consumir sus avisos.

        Cada fila TSV contiene ID, mensajes sin leer, total y último ID. La
        lista de contactos existe solo en la SD del cliente, así que este
        resumen permite separar contactos de desconocidos allí.
        """
        if not self.authorized(recipient):
            return
        with open_database(self.server.database_path) as connection:
            rows = connection.execute(
                "SELECT sender, SUM(notification_delivered = 0), COUNT(*), MAX(id) "
                "FROM messages WHERE recipient = ? AND sender != ? "
                "GROUP BY sender ORDER BY MAX(id) DESC LIMIT 80",
                (recipient, recipient)).fetchall()
        self.send_text(200, "".join("{}\t{}\t{}\t{}\n".format(*row) for row in rows))

    def post_presence(self, user_id):
        """Renueva el latido de una sesión; deja de verse al cerrar o perder red."""
        if not self.authorized(user_id):
            return
        with self.server.presence_lock:
            self.server.presence[user_id] = time.monotonic()
        self.send_text(200, "OK")

    def get_presence(self, user_id, ids_text):
        """Consulta en lote estados efímeros, con caducidad de 60 segundos."""
        if not self.authorized(user_id):
            return
        ids = ids_text.split(",") if ids_text else []
        if len(ids) > 50 or any(not valid_id(item) for item in ids):
            self.send_text(400, "IDs invalidos")
            return
        now = time.monotonic()
        with self.server.presence_lock:
            active = {item for item in ids if now - self.server.presence.get(item, -1000) < 60}
            if len(self.server.presence) > 1000:
                self.server.presence = {key: value for key, value in self.server.presence.items()
                                        if now - value < 60}
        self.send_text(200, "".join("{}\t{}\n".format(item, int(item in active)) for item in ids))

    def get_count(self, user_id, friend_id):
        """Cuenta mensajes en ambas direcciones para la pareja solicitada."""
        if not self.authorized(user_id):
            return
        with open_database(self.server.database_path) as connection:
            count = connection.execute(
                "SELECT COUNT(*) FROM messages WHERE (sender = ? AND recipient = ?) "
                "OR (sender = ? AND recipient = ?)",
                (user_id, friend_id, friend_id, user_id)).fetchone()[0]
        self.send_text(200, str(count))

    def get_chat(self, user_id, friend_id):
        """Entrega el formato de texto histórico compatible con clientes antiguos."""
        if not self.authorized(user_id):
            return
        with open_database(self.server.database_path) as connection:
            rows = connection.execute(
                "SELECT id, sender, body FROM messages "
                "WHERE kind = 'texto' AND ((sender = ? AND recipient = ?) "
                "OR (sender = ? AND recipient = ?)) "
                "ORDER BY id DESC LIMIT 50",
                (user_id, friend_id, friend_id, user_id),
            ).fetchall()
            connection.execute(
                "UPDATE messages SET notification_delivered = 1 "
                "WHERE recipient = ? AND sender = ?",
                (user_id, friend_id),
            )

        latest_lines = []
        used_bytes = 0
        for message_id, sender, encrypted_body in rows:
            body = self.server.cipher.open_text("messages", message_id, "body", encrypted_body)
            line = "[{}]: {}\n".format(sender, body)
            line_bytes = len(line.encode("utf-8"))
            if used_bytes + line_bytes > MAX_CHAT_BYTES:
                break
            latest_lines.append(line)
            used_bytes += line_bytes
        self.send_text(200, "".join(reversed(latest_lines)))

    def get_history(self, user_id, friend_id, query):
        """Devuelve una página cronológica de texto, audio y fotos en formato TSV."""
        if not self.authorized(user_id):
            return
        params = parse_qs(query)
        try:
            before = int(params.get("before", ["0"])[0])
            limit = int(params.get("limit", ["20"])[0])
        except ValueError:
            self.send_text(400, "Cursor invalido")
            return
        if before < 0 or not 1 <= limit <= 20:
            self.send_text(400, "Cursor invalido")
            return
        with open_database(self.server.database_path) as connection:
            rows = connection.execute(
                "SELECT id, sender, kind, duration_ms, body FROM messages "
                "WHERE ((sender = ? AND recipient = ?) OR (sender = ? AND recipient = ?)) "
                "AND (? = 0 OR id < ?) ORDER BY id DESC LIMIT ?",
                (user_id, friend_id, friend_id, user_id, before, before, limit),
            ).fetchall()
            connection.execute(
                "UPDATE messages SET notification_delivered = 1 "
                "WHERE recipient = ? AND sender = ?",
                (user_id, friend_id),
            )
        lines = ["{}\t{}\t{}\t{}\t{}\n".format(
            row[0], row[1], row[2], row[3],
            self.server.cipher.open_text("messages", row[0], "body", row[4]))
            for row in reversed(rows)]
        self.send_text(200, "".join(lines))

    def post_audio(self, sender, recipient):
        """Valida un WAV PCM de hasta 30 segundos y lo guarda como mensaje."""
        if not self.authorized(sender):
            return
        if self.headers.get_content_type() != "audio/wav":
            self.send_text(415, "Se requiere audio/wav")
            return
        try:
            length = int(self.headers.get("Content-Length", ""))
        except ValueError:
            self.send_text(411, "Falta Content-Length")
            return
        if length < 44 or length > MAX_AUDIO_BYTES:
            # Consumir cargas pequeñas rechazadas evita un TCP RST al responder
            # a clientes que ya terminaron de enviar el cuerpo.
            if 0 < length <= MAX_AUDIO_BYTES + 65536:
                self.rfile.read(length)
            self.send_text(413, "Audio demasiado grande")
            return
        audio = self.rfile.read(length)
        duration_ms = wav_duration_ms(audio)
        if duration_ms is None:
            self.send_text(400, "WAV invalido o mayor de 30 segundos")
            return
        try:
            with open_database(self.server.database_path) as connection:
                cursor = connection.execute(
                    "INSERT INTO messages(sender, recipient, body, kind, duration_ms, audio_data) "
                    "VALUES (?, ?, '', 'audio', ?, NULL)",
                    (sender, recipient, duration_ms),
                )
                message_id = cursor.lastrowid
                connection.execute("UPDATE messages SET body=?, audio_data=? WHERE id=?", (
                    self.server.cipher.seal_text("messages", message_id, "body", ""),
                    self.server.cipher.seal_blob("messages", message_id, "audio_data", audio),
                    message_id))
        except (sqlite3.Error, ValueError):
            self.log_error("Error de base de datos en POST /audio")
            self.send_text(500, "Error interno")
            return
        self.send_text(200, str(message_id))

    def get_audio(self, message_id):
        """Recupera los bytes WAV asociados a un identificador de mensaje."""
        with open_database(self.server.database_path) as connection:
            row = connection.execute(
                "SELECT audio_data, sender, recipient FROM messages WHERE id = ? AND kind = 'audio'",
                (message_id,),
            ).fetchone()
        if row is None:
            self.send_text(404, "Audio no encontrado")
        elif not self.authorized_media(row[1], row[2]):
            return
        else:
            self.send_bytes(200, self.server.cipher.open_blob("messages", message_id,
                            "audio_data", row[0]), "audio/wav")

    def authorized_media(self, sender, recipient):
        """Exige que X-User-ID pertenezca al chat y autoriza su perfil protegido."""
        requester = self.headers.get("X-User-ID", "")
        if requester not in (sender, recipient):
            self.send_text(403, "No perteneces a esta conversacion")
            return False
        return self.authorized(requester)

    def post_photo(self, sender, recipient):
        """Guarda una foto RGB565 de 400×240 enviada por la cámara de la 3DS."""
        if not self.authorized(sender):
            return
        if self.headers.get_content_type() != "application/x-mensinger-bgr565":
            self.send_text(415, "Se requiere application/x-mensinger-bgr565")
            return
        try:
            length = int(self.headers.get("Content-Length", ""))
        except ValueError:
            self.send_text(411, "Falta Content-Length")
            return
        if length != PHOTO_BYTES:
            if 0 < length <= PHOTO_BYTES + 65536:
                self.rfile.read(length)
            self.send_text(413, "La foto debe medir 400x240 RGB565")
            return
        photo = self.rfile.read(length)
        if len(photo) != PHOTO_BYTES:
            self.send_text(400, "Foto incompleta")
            return
        try:
            with open_database(self.server.database_path) as connection:
                cursor = connection.execute(
                    "INSERT INTO messages(sender, recipient, body, kind, image_data, image_thumb, "
                    "image_width, image_height) VALUES (?, ?, '', 'foto', NULL, NULL, ?, ?)",
                    (sender, recipient, PHOTO_WIDTH, PHOTO_HEIGHT),
                )
                message_id = cursor.lastrowid
                connection.execute("UPDATE messages SET body=?, image_data=?, image_thumb=? "
                                   "WHERE id=?", (
                    self.server.cipher.seal_text("messages", message_id, "body", "400x240"),
                    self.server.cipher.seal_blob("messages", message_id, "image_data", photo),
                    self.server.cipher.seal_blob("messages", message_id, "image_thumb",
                                                 photo_thumbnail(photo)), message_id))
        except (sqlite3.Error, ValueError):
            self.log_error("Error de base de datos en POST /foto")
            self.send_text(500, "Error interno")
            return
        self.send_text(200, str(message_id))

    def get_photo(self, message_id):
        """Devuelve los bytes de foto del mensaje, conservando el formato RGB565."""
        with open_database(self.server.database_path) as connection:
            row = connection.execute(
                "SELECT image_data, sender, recipient FROM messages WHERE id = ? AND kind = 'foto'",
                (message_id,),
            ).fetchone()
        if row is None:
            self.send_text(404, "Foto no encontrada")
        elif not self.authorized_media(row[1], row[2]):
            return
        else:
            self.send_bytes(200, self.server.cipher.open_blob("messages", message_id,
                            "image_data", row[0]), "application/x-mensinger-bgr565")

    def get_photo_thumbnail(self, message_id):
        """Devuelve una vista previa 80×48 solo a participantes del chat."""
        with open_database(self.server.database_path) as connection:
            row = connection.execute(
                "SELECT image_data, image_thumb, sender, recipient FROM messages "
                "WHERE id = ? AND kind = 'foto'", (message_id,)
            ).fetchone()
        if row is None:
            self.send_text(404, "Foto no encontrada")
            return
        if not self.authorized_media(row[2], row[3]):
            return
        thumbnail = (self.server.cipher.open_blob("messages", message_id, "image_thumb", row[1])
                     if row[1] is not None else None)
        if thumbnail is None or len(thumbnail) != CHAT_THUMB_BYTES:
            photo = self.server.cipher.open_blob("messages", message_id, "image_data", row[0])
            thumbnail = photo_thumbnail(photo)
            with open_database(self.server.database_path) as connection:
                connection.execute("UPDATE messages SET image_thumb = ? WHERE id = ?",
                                   (self.server.cipher.seal_blob("messages", message_id,
                                    "image_thumb", thumbnail), message_id))
        self.send_bytes(200, thumbnail, "application/x-mensinger-bgr565")

    def get_profile(self, user_id):
        """Devuelve los campos de un perfil y si existe una foto asociada."""
        with open_database(self.server.database_path) as connection:
            row = connection.execute(
                "SELECT username, nickname, bio, description, photo_data, updated_at, "
                "chat_color, background_color, text_color, nickname_color, name_color "
                "FROM profiles WHERE user_id = ?", (user_id,)
            ).fetchone()
        if row is None:
            self.send_text(404, "Perfil no encontrado")
            return
        payload = json.dumps({"user_id": user_id, "username": row[0],
                              "nickname": row[1], "bio": row[2],
                              "description": row[3], "has_photo": row[4] is not None,
                              "updated_at": row[5], "chat_color": row[6],
                              "background_color": row[7], "text_color": row[8],
                              "nickname_color": row[9], "name_color": row[10]}, ensure_ascii=False).encode("utf-8")
        self.send_bytes(200, payload, "application/json; charset=utf-8")

    def get_profiles(self, ids_text):
        """Devuelve varios perfiles como TSV para sincronizar contactos en bloque."""
        ids = list(dict.fromkeys(ids_text.split(",")))
        if not ids_text or len(ids) > 50 or any(not valid_id(user_id) for user_id in ids):
            self.send_text(400, "Lista de IDs invalida")
            return
        placeholders = ",".join("?" for _ in ids)
        with open_database(self.server.database_path) as connection:
            rows = connection.execute(
                "SELECT user_id, username, nickname, bio, description, photo_data, "
                "chat_color, background_color, text_color, nickname_color, name_color "
                "FROM profiles WHERE user_id IN (" + placeholders + ")", ids
            ).fetchall()
        lines = ["{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\n".format(
            row[0], row[1], row[2], row[3], row[4], "1" if row[5] is not None else "0",
            row[6], row[7], row[8], row[9], row[10]
        ) for row in rows]
        self.send_text(200, "".join(lines))

    def post_profile(self, user_id):
        """Valida y crea o actualiza los campos de texto de un perfil."""
        if not self.authorized(user_id):
            return
        if self.headers.get_content_type() != "application/json":
            self.send_text(415, "Se requiere application/json")
            return
        try:
            length = int(self.headers.get("Content-Length", ""))
        except ValueError:
            self.send_text(411, "Falta Content-Length")
            return
        if length < 1 or length > MAX_REQUEST_BYTES:
            self.send_text(413, "Perfil demasiado grande")
            return
        try:
            data = json.loads(self.rfile.read(length).decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError):
            self.send_text(400, "JSON invalido")
            return
        limits = {"username": 32, "nickname": 32, "bio": 120, "description": 120}
        if not isinstance(data, dict) or any(
            not isinstance(data.get(key), str) or len(data[key]) > limit or
            any(ord(char) < 32 for char in data[key])
            for key, limit in limits.items()
        ):
            self.send_text(400, "Campos de perfil invalidos")
            return
        if not isinstance(data.get("show_in_search", False), bool) or not isinstance(data.get("meet_people", False), bool):
            self.send_text(400, "Visibilidad invalida")
            return
        color_keys = ("chat_color", "background_color", "text_color", "nickname_color", "name_color")
        if any(type(data.get(key, 0)) is not int or not 0 <= data[key] <= 5
               for key in color_keys if key in data):
            self.send_text(400, "Color invalido")
            return
        values = tuple(data[key].strip() for key in limits)
        with open_database(self.server.database_path) as connection:
            connection.execute(
                "INSERT INTO profiles(user_id, username, nickname, bio, description, show_in_search, meet_people, "
                "chat_color, background_color, text_color, nickname_color, name_color) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) ON CONFLICT(user_id) DO UPDATE SET "
                "username=excluded.username, nickname=excluded.nickname, "
                "bio=excluded.bio, description=excluded.description, "
                "show_in_search=excluded.show_in_search, meet_people=excluded.meet_people, "
                "chat_color=excluded.chat_color, background_color=excluded.background_color, "
                "text_color=excluded.text_color, nickname_color=excluded.nickname_color, "
                "name_color=excluded.name_color, "
                "updated_at=CURRENT_TIMESTAMP", (user_id, *values,
                int(data.get("show_in_search", False)), int(data.get("meet_people", False)),
                *(data.get(key, 0) for key in color_keys))
            )
        self.send_text(200, "OK")

    def get_profile_photo(self, user_id):
        """Descarga la foto original del perfil solicitado."""
        with open_database(self.server.database_path) as connection:
            row = connection.execute(
                "SELECT photo_data FROM profiles WHERE user_id = ?", (user_id,)
            ).fetchone()
        if row is None or row[0] is None:
            self.send_text(404, "Foto de perfil no encontrada")
        else:
            self.send_bytes(200, self.server.cipher.open_blob("profiles", user_id,
                            "photo_data", row[0]), "application/x-mensinger-bgr565")

    def get_profile_thumbnails(self, ids_text):
        """Devuelve miniaturas binarias para un máximo de tres contactos visibles."""
        ids = list(dict.fromkeys(ids_text.split(",")))
        if not ids_text or len(ids) > 3 or any(not valid_id(user_id) for user_id in ids):
            self.send_text(400, "Lista de IDs invalida")
            return
        placeholders = ",".join("?" for _ in ids)
        with open_database(self.server.database_path) as connection:
            rows = connection.execute(
                "SELECT user_id, photo_data, photo_thumb FROM profiles WHERE user_id IN (" +
                placeholders + ")", ids
            ).fetchall()
            by_id = {}
            for user_id, photo, thumbnail in rows:
                if photo is None:
                    continue
                thumbnail = (self.server.cipher.open_blob("profiles", user_id, "photo_thumb",
                             thumbnail) if thumbnail is not None else None)
                if thumbnail is None or len(thumbnail) != PROFILE_THUMB_BYTES:
                    photo = self.server.cipher.open_blob("profiles", user_id, "photo_data", photo)
                    thumbnail = profile_thumbnail(photo)
                    connection.execute("UPDATE profiles SET photo_thumb = ? WHERE user_id = ?",
                                       (self.server.cipher.seal_blob("profiles", user_id,
                                        "photo_thumb", thumbnail), user_id))
                by_id[user_id] = thumbnail
        records = [(user_id, by_id[user_id]) for user_id in ids if user_id in by_id]
        payload = bytearray((len(records),))
        for user_id, thumbnail in records:
            encoded_id = user_id.encode("ascii")
            payload.append(len(encoded_id))
            payload.extend(encoded_id)
            payload.extend(thumbnail)
        self.send_bytes(200, bytes(payload), "application/x-mensinger-profile-thumbnails")

    def post_profile_photo(self, user_id):
        """Guarda la foto de perfil y su miniatura cuadrada derivada."""
        if not self.authorized(user_id):
            return
        if self.headers.get_content_type() != "application/x-mensinger-bgr565":
            self.send_text(415, "Se requiere application/x-mensinger-bgr565")
            return
        try:
            length = int(self.headers.get("Content-Length", ""))
        except ValueError:
            self.send_text(411, "Falta Content-Length")
            return
        if length != PHOTO_BYTES:
            if 0 < length <= PHOTO_BYTES + 65536:
                self.rfile.read(length)
            self.send_text(413, "La foto debe medir 400x240 RGB565")
            return
        photo = self.rfile.read(length)
        if len(photo) != PHOTO_BYTES:
            self.send_text(400, "Foto incompleta")
            return
        thumbnail = profile_thumbnail(photo)
        with open_database(self.server.database_path) as connection:
            connection.execute(
                "INSERT INTO profiles(user_id, photo_data, photo_thumb) VALUES (?, ?, ?) "
                "ON CONFLICT(user_id) DO UPDATE SET photo_data=excluded.photo_data, "
                "photo_thumb=excluded.photo_thumb, "
                "updated_at=CURRENT_TIMESTAMP", (user_id,
                self.server.cipher.seal_blob("profiles", user_id, "photo_data", photo),
                self.server.cipher.seal_blob("profiles", user_id, "photo_thumb", thumbnail))
            )
        self.send_text(200, "OK")


def valid_id(value):
    """Indica si el ID es texto compuesto por entre uno y diez dígitos."""
    return isinstance(value, str) and ID_PATTERN.fullmatch(value) is not None


def search_key(value):
    """Normaliza mayúsculas y acentos para buscar coincidencias parciales."""
    return "".join(character for character in unicodedata.normalize("NFKD", value.casefold())
                   if not unicodedata.combining(character))


def profile_thumbnail(photo):
    """Recorta al centro una foto 400×240 y muestrea una miniatura RGB565 de 32×32."""
    if len(photo) != PHOTO_BYTES:
        raise ValueError("La foto de perfil debe medir 400x240 RGB565.")
    thumb = bytearray(PROFILE_THUMB_BYTES)
    crop_left = (PHOTO_WIDTH - PHOTO_HEIGHT) // 2
    for y in range(PROFILE_THUMB_SIZE):
        source_y = ((2 * y + 1) * PHOTO_HEIGHT) // (2 * PROFILE_THUMB_SIZE)
        for x in range(PROFILE_THUMB_SIZE):
            source_x = crop_left + ((2 * x + 1) * PHOTO_HEIGHT) // (2 * PROFILE_THUMB_SIZE)
            source = (source_y * PHOTO_WIDTH + source_x) * 2
            destination = (y * PROFILE_THUMB_SIZE + x) * 2
            thumb[destination:destination + 2] = photo[source:source + 2]
    return bytes(thumb)


def photo_thumbnail(photo):
    """Muestrea la foto completa en RGB565 a 80×48 sin recortarla."""
    if len(photo) != PHOTO_BYTES:
        raise ValueError("La foto debe medir 400x240 RGB565.")
    thumb = bytearray(CHAT_THUMB_BYTES)
    for y in range(CHAT_THUMB_HEIGHT):
        source_y = ((2 * y + 1) * PHOTO_HEIGHT) // (2 * CHAT_THUMB_HEIGHT)
        for x in range(CHAT_THUMB_WIDTH):
            source_x = ((2 * x + 1) * PHOTO_WIDTH) // (2 * CHAT_THUMB_WIDTH)
            source = (source_y * PHOTO_WIDTH + source_x) * 2
            destination = (y * CHAT_THUMB_WIDTH + x) * 2
            thumb[destination:destination + 2] = photo[source:source + 2]
    return bytes(thumb)


def wav_duration_ms(payload):
    """Comprueba el encabezado WAV esperado y devuelve su duración en milisegundos."""
    if len(payload) < 44:
        return None
    try:
        riff, riff_size, wave, fmt, fmt_size, codec, channels, rate, byte_rate, \
            block_align, bits, data, data_size = struct.unpack_from("<4sI4s4sIHHIIHH4sI", payload)
    except struct.error:
        return None
    if (riff != b"RIFF" or riff_size != len(payload) - 8 or wave != b"WAVE"
            or fmt != b"fmt " or fmt_size != 16 or codec != 1 or channels != 1
            or rate != SAMPLE_RATE or byte_rate != SAMPLE_RATE * 2
            or block_align != 2 or bits != 16 or data != b"data"
            or data_size != len(payload) - 44 or data_size % 2 != 0
            or data_size == 0 or data_size > MAX_AUDIO_SAMPLES * 2):
        return None
    return data_size * 1000 // (SAMPLE_RATE * 2)


def main():
    """Lee opciones, prepara la base y mantiene el servidor atendiendo peticiones.

    En consola, las dos primeras líneas confirman la dirección y la base activa.
    Después, BaseHTTPRequestHandler imprime una línea por petición con cliente,
    ruta y estado HTTP; Ctrl+C cierra el servidor de forma ordenada.
    """
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="0.0.0.0", help="Interfaz de escucha (por defecto: 0.0.0.0)")
    parser.add_argument("--port", type=int, default=8080, help="Puerto de escucha (por defecto: 8080)")
    parser.add_argument("--db", type=Path, default=Path(__file__).with_name("messages.sqlite3"))
    parser.add_argument("--key", type=Path, help="Clave local de la base (por defecto, junto a la base)")
    parser.add_argument("--cifrado-conexion", choices=("si", "no"), default="si",
                        help="Cifrar peticiones sobre HTTP (por defecto: si)")
    parser.add_argument("--transport-key", type=Path,
                        help="Clave privada X25519 del servidor (por defecto, junto a la base)")
    parser.add_argument("--version", default="v1.4.4", help="Valor de GET /version")
    args = parser.parse_args()

    if not 1 <= args.port <= 65535:
        parser.error("El puerto debe estar entre 1 y 65535")
    database_path = args.db.expanduser().resolve()
    create_database(database_path)
    server = MensingerServer((args.host, args.port), database_path, args.version, args.key,
                             args.cifrado_conexion == "si", args.transport_key)
    print("Mensinger 3DS escuchando en http://{}:{}/".format(args.host, args.port), flush=True)
    print("Base de datos: {}".format(database_path), flush=True)
    print("Clave de la base: {}".format(args.key or default_key_path(database_path)), flush=True)
    if args.cifrado_conexion == "si":
        print("Canal cifrado: SI (X25519 y ChaCha20-Poly1305 sobre HTTP)", flush=True)
        print("Clave privada de transporte: {}".format(
            args.transport_key or database_path.with_name("transport.key")), flush=True)
        print("Huella de clave publica SHA-256: {}".format(
            hashlib.sha256(server.secure_channel.public_key).hexdigest()), flush=True)
    else:
        print("Canal cifrado: NO; tráfico HTTP en claro", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nServidor detenido")
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
