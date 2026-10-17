"""Servidor HTTP de ejemplo compatible con el cliente Mensinger 3DS."""

import argparse
import json
import re
import sqlite3
import struct
from contextlib import contextmanager
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlsplit


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
                   updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
               )"""
        )
        profile_columns = {row[1] for row in connection.execute("PRAGMA table_info(profiles)")}
        if "photo_thumb" not in profile_columns:
            connection.execute("ALTER TABLE profiles ADD COLUMN photo_thumb BLOB")


class MensingerServer(ThreadingHTTPServer):
    """Servidor HTTP concurrente que comparte la ruta SQLite y la versión."""
    daemon_threads = True
    allow_reuse_address = True

    def __init__(self, address, database_path, version):
        """Configura los datos que consultarán los manejadores de cada petición."""
        super().__init__(address, MensingerHandler)
        self.database_path = database_path
        self.app_version = version


class MensingerHandler(BaseHTTPRequestHandler):
    """Implementa las rutas HTTP que consume la app de Nintendo 3DS.

    La clase base registra cada petición en la consola con cliente, ruta y
    estado HTTP; los métodos de esta clase no imprimen el contenido del chat.
    """
    protocol_version = "HTTP/1.1"
    server_version = "MensingerExample/1.2"
    sys_version = ""

    def send_bytes(self, status, payload, content_type="text/plain; charset=utf-8"):
        """Envía cabeceras y cuerpo juntos para el lector HTTP sencillo de 3DS."""
        # Un solo envio reduce la posibilidad de que el cliente 3DS lea
        # las cabeceras y el cuerpo en llamadas recv separadas.
        header = (
            "HTTP/1.1 {} {}\r\n"
            "Content-Type: {}\r\n"
            "Content-Length: {}\r\n"
            "Cache-Control: no-store\r\n"
            "Connection: close\r\n\r\n"
        ).format(status, HTTPStatus(status).phrase, content_type, len(payload)).encode("ascii")
        self.close_connection = True
        try:
            self.connection.sendall(header + payload)
        except (BrokenPipeError, ConnectionResetError):
            pass

    def send_text(self, status, text):
        """Codifica una respuesta textual UTF-8 y delega el envío común."""
        self.send_bytes(status, text.encode("utf-8"))

    def do_GET(self):
        """Selecciona una ruta de lectura y valida sus segmentos antes de usarla."""
        parsed = urlsplit(self.path)
        parts = parsed.path.strip("/").split("/")
        try:
            if parts == ["version"]:
                self.send_text(200, self.server.app_version)
            elif parts == ["perfiles"]:
                self.get_profiles(parse_qs(parsed.query).get("ids", [""])[0])
            elif parts == ["perfil-miniaturas"]:
                self.get_profile_thumbnails(parse_qs(parsed.query).get("ids", [""])[0])
            elif len(parts) == 2 and parts[0] == "notificaciones" and valid_id(parts[1]):
                self.get_notification(parts[1])
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
            elif len(parts) == 2 and parts[0] == "perfil" and valid_id(parts[1]):
                self.get_profile(parts[1])
            elif len(parts) == 3 and parts[0] == "perfil" and valid_id(parts[1]) and parts[2] == "foto":
                self.get_profile_photo(parts[1])
            else:
                self.send_text(404, "Ruta no encontrada")
        except sqlite3.Error:
            self.log_error("Error de base de datos en GET %s", self.path)
            self.send_text(500, "Error interno")

    def do_POST(self):
        """Distribuye las cargas binarias y JSON a su ruta de escritura."""
        path = urlsplit(self.path).path
        audio_parts = path.strip("/").split("/")
        if len(audio_parts) == 2 and audio_parts[0] == "perfil" and valid_id(audio_parts[1]):
            self.post_profile(audio_parts[1])
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
            data = json.loads(self.rfile.read(length).decode("utf-8"))
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
        if not isinstance(body, str) or not body.strip() or len(body) > 120:
            self.send_text(400, "El texto debe tener entre 1 y 120 caracteres")
            return
        if any(ord(character) < 32 for character in body):
            self.send_text(400, "El texto no puede contener saltos de linea ni controles")
            return

        try:
            with open_database(self.server.database_path) as connection:
                connection.execute(
                    "INSERT INTO messages(sender, recipient, body) VALUES (?, ?, ?)",
                    (sender, recipient, body),
                )
        except sqlite3.Error:
            self.log_error("Error de base de datos en POST /enviar")
            self.send_text(500, "Error interno")
            return
        self.send_text(200, "OK")

    def get_notification(self, recipient):
        """Devuelve el remitente pendiente más antiguo y marca esa pareja como leída."""
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

    def get_chat(self, user_id, friend_id):
        """Entrega el formato de texto histórico compatible con clientes antiguos."""
        with open_database(self.server.database_path) as connection:
            rows = connection.execute(
                "SELECT sender, body FROM messages "
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
        for sender, body in rows:
            line = "[{}]: {}\n".format(sender, body)
            line_bytes = len(line.encode("utf-8"))
            if used_bytes + line_bytes > MAX_CHAT_BYTES:
                break
            latest_lines.append(line)
            used_bytes += line_bytes
        self.send_text(200, "".join(reversed(latest_lines)))

    def get_history(self, user_id, friend_id, query):
        """Devuelve una página cronológica de texto, audio y fotos en formato TSV."""
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
        lines = ["{}\t{}\t{}\t{}\t{}\n".format(*row) for row in reversed(rows)]
        self.send_text(200, "".join(lines))

    def post_audio(self, sender, recipient):
        """Valida un WAV PCM de hasta 30 segundos y lo guarda como mensaje."""
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
                    "VALUES (?, ?, '', 'audio', ?, ?)",
                    (sender, recipient, duration_ms, audio),
                )
                message_id = cursor.lastrowid
        except sqlite3.Error:
            self.log_error("Error de base de datos en POST /audio")
            self.send_text(500, "Error interno")
            return
        self.send_text(200, str(message_id))

    def get_audio(self, message_id):
        """Recupera los bytes WAV asociados a un identificador de mensaje."""
        with open_database(self.server.database_path) as connection:
            row = connection.execute(
                "SELECT audio_data FROM messages WHERE id = ? AND kind = 'audio'",
                (message_id,),
            ).fetchone()
        if row is None:
            self.send_text(404, "Audio no encontrado")
        else:
            self.send_bytes(200, row[0], "audio/wav")

    def post_photo(self, sender, recipient):
        """Guarda una foto RGB565 de 400×240 enviada por la cámara de la 3DS."""
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
                    "INSERT INTO messages(sender, recipient, body, kind, image_data, image_width, image_height) "
                    "VALUES (?, ?, '400x240', 'foto', ?, ?, ?)",
                    (sender, recipient, photo, PHOTO_WIDTH, PHOTO_HEIGHT),
                )
                message_id = cursor.lastrowid
        except sqlite3.Error:
            self.log_error("Error de base de datos en POST /foto")
            self.send_text(500, "Error interno")
            return
        self.send_text(200, str(message_id))

    def get_photo(self, message_id):
        """Devuelve los bytes de foto del mensaje, conservando el formato RGB565."""
        with open_database(self.server.database_path) as connection:
            row = connection.execute(
                "SELECT image_data FROM messages WHERE id = ? AND kind = 'foto'",
                (message_id,),
            ).fetchone()
        if row is None:
            self.send_text(404, "Foto no encontrada")
        else:
            self.send_bytes(200, row[0], "application/x-mensinger-bgr565")

    def get_profile(self, user_id):
        """Devuelve los campos de un perfil y si existe una foto asociada."""
        with open_database(self.server.database_path) as connection:
            row = connection.execute(
                "SELECT username, nickname, bio, description, photo_data, updated_at "
                "FROM profiles WHERE user_id = ?", (user_id,)
            ).fetchone()
        if row is None:
            self.send_text(404, "Perfil no encontrado")
            return
        payload = json.dumps({"user_id": user_id, "username": row[0],
                              "nickname": row[1], "bio": row[2],
                              "description": row[3], "has_photo": row[4] is not None,
                              "updated_at": row[5]}, ensure_ascii=False).encode("utf-8")
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
                "SELECT user_id, username, nickname, bio, description, photo_data "
                "FROM profiles WHERE user_id IN (" + placeholders + ")", ids
            ).fetchall()
        lines = ["{}\t{}\t{}\t{}\t{}\t{}\n".format(
            row[0], row[1], row[2], row[3], row[4], "1" if row[5] is not None else "0"
        ) for row in rows]
        self.send_text(200, "".join(lines))

    def post_profile(self, user_id):
        """Valida y crea o actualiza los campos de texto de un perfil."""
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
        values = tuple(data[key].strip() for key in limits)
        with open_database(self.server.database_path) as connection:
            connection.execute(
                "INSERT INTO profiles(user_id, username, nickname, bio, description) "
                "VALUES (?, ?, ?, ?, ?) ON CONFLICT(user_id) DO UPDATE SET "
                "username=excluded.username, nickname=excluded.nickname, "
                "bio=excluded.bio, description=excluded.description, "
                "updated_at=CURRENT_TIMESTAMP", (user_id, *values)
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
            self.send_bytes(200, row[0], "application/x-mensinger-bgr565")

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
                if thumbnail is None or len(thumbnail) != PROFILE_THUMB_BYTES:
                    thumbnail = profile_thumbnail(photo)
                    connection.execute("UPDATE profiles SET photo_thumb = ? WHERE user_id = ?",
                                       (thumbnail, user_id))
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
                "updated_at=CURRENT_TIMESTAMP", (user_id, photo, thumbnail)
            )
        self.send_text(200, "OK")


def valid_id(value):
    """Indica si el ID es texto compuesto por entre uno y diez dígitos."""
    return isinstance(value, str) and ID_PATTERN.fullmatch(value) is not None


def profile_thumbnail(photo):
    """Recorta al centro una foto 400×240 y muestrea una miniatura BGR565 de 32×32."""
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
    parser.add_argument("--port", type=int, default=8080, help="Puerto HTTP (por defecto: 8080)")
    parser.add_argument("--db", type=Path, default=Path(__file__).with_name("messages.sqlite3"))
    parser.add_argument("--version", default="v1.2.4", help="Valor de GET /version")
    args = parser.parse_args()

    if not 1 <= args.port <= 65535:
        parser.error("El puerto debe estar entre 1 y 65535")
    database_path = args.db.expanduser().resolve()
    create_database(database_path)
    server = MensingerServer((args.host, args.port), database_path, args.version)
    print("Mensinger 3DS escuchando en http://{}:{}/".format(args.host, args.port), flush=True)
    print("Base de datos: {}".format(database_path), flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nServidor detenido")
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
