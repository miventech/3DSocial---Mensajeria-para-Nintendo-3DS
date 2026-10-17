"""Cifrado autenticado por campo para el contenido persistido por ServerExample.

No cifra el archivo SQLite completo: aplica AES-GCM a los campos elegidos,
liga cada celda a su tabla/fila/columna y migra las filas antiguas. El monitor
importa esta misma clase para mantener una sola implementación criptográfica.
"""

import base64
import os
import sqlite3
from contextlib import closing
from pathlib import Path

from cryptography.exceptions import InvalidTag
from cryptography.hazmat.primitives.ciphers.aead import AESGCM


MARK = b"MP1"
TEXT_MARK = "enc:v1:"


def default_key_path(database_path):
    """Devuelve el archivo .key hermano de la base SQLite indicada."""
    return Path(database_path).with_suffix(".key")


def _has_encrypted_content(database_path):
    """Detecta marcadores existentes para no crear otra clave accidentalmente."""
    if not Path(database_path).exists():
        return False
    with closing(sqlite3.connect(database_path)) as db:
        for table, column in (("messages", "body"), ("messages", "audio_data"),
                              ("messages", "image_data"), ("profiles", "photo_data")):
            exists = db.execute("SELECT 1 FROM sqlite_master WHERE type='table' AND name=?",
                                (table,)).fetchone()
            if not exists:
                continue
            if column == "body":
                found = db.execute(f"SELECT 1 FROM {table} WHERE {column} LIKE ? LIMIT 1",
                                   (TEXT_MARK + "%",)).fetchone()
            else:
                found = db.execute(f"SELECT 1 FROM {table} WHERE substr({column},1,3)=? LIMIT 1",
                                   (MARK,)).fetchone()
            if found:
                return True
    return False


class ContentCipher:
    """AES-256-GCM con nonce aleatorio y AAD ligada a fila y columna."""

    def __init__(self, key):
        """Inicializa AES-GCM y rechaza claves que no tengan exactamente 256 bits."""
        if len(key) != 32:
            raise ValueError("La clave de la base debe contener exactamente 32 bytes.")
        self.aead = AESGCM(key)

    @classmethod
    def from_file(cls, key_path, database_path=None, create=False):
        """Lee una clave existente o crea una nueva solo para una base sin cifrar."""
        path = Path(key_path)
        if not path.exists():
            if not create:
                raise FileNotFoundError(f"Falta la clave de cifrado: {path}")
            if database_path and _has_encrypted_content(database_path):
                raise ValueError("La base ya contiene datos cifrados y falta su clave original.")
            path.parent.mkdir(parents=True, exist_ok=True)
            key = os.urandom(32)
            try:
                descriptor = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
            except FileExistsError:
                return cls.from_file(path)
            with os.fdopen(descriptor, "wb") as stream:
                stream.write(key)
        else:
            key = path.read_bytes()
        return cls(key)

    @staticmethod
    def aad(table, identity, column):
        """Ata criptográficamente la celda a tabla, fila, columna y versión."""
        return f"Mensinger3DS:{table}:{identity}:{column}:v1".encode("utf-8")

    def seal_blob(self, table, identity, column, data):
        """Cifra bytes con nonce nuevo; conserva None como ausencia de contenido."""
        if data is None:
            return None
        nonce = os.urandom(12)
        return MARK + nonce + self.aead.encrypt(nonce, bytes(data), self.aad(table, identity, column))

    def open_blob(self, table, identity, column, data):
        """Autentica y descifra un BLOB marcado MP1; rechaza texto plano o alterado."""
        if data is None:
            return None
        if not isinstance(data, bytes) or not data.startswith(MARK) or len(data) < 31:
            raise ValueError(f"Contenido sin cifrar o inválido: {table}.{column} #{identity}")
        try:
            return self.aead.decrypt(data[3:15], data[15:], self.aad(table, identity, column))
        except InvalidTag as exc:
            raise ValueError("Clave incorrecta o contenido alterado.") from exc

    def seal_text(self, table, identity, column, text):
        """Cifra UTF-8 y lo empaqueta en una cadena adecuada para SQLite."""
        blob = self.seal_blob(table, identity, column, text.encode("utf-8"))
        return TEXT_MARK + base64.urlsafe_b64encode(blob).decode("ascii")

    def open_text(self, table, identity, column, text):
        """Desempaqueta el texto y verifica autenticidad antes de decodificar UTF-8."""
        if not isinstance(text, str) or not text.startswith(TEXT_MARK):
            raise ValueError(f"Texto sin cifrar o inválido: {table}.{column} #{identity}")
        try:
            blob = base64.b64decode(text[len(TEXT_MARK):], altchars=b"-_", validate=True)
            return self.open_blob(table, identity, column, blob).decode("utf-8")
        except (UnicodeDecodeError, ValueError) as exc:
            raise ValueError("Clave incorrecta o texto cifrado alterado.") from exc


def migrate_content(database_path, cipher):
    """Convierte filas antiguas a cifrado de forma transaccional y repetible.

    Recorre lotes pequeños, deja intactas las celdas ya marcadas y compacta
    SQLite solo cuando hubo cambios. La transacción evita una migración parcial.
    """
    changed = False
    with closing(sqlite3.connect(database_path, timeout=5)) as db, db:
        db.execute("PRAGMA secure_delete=ON")
        encrypted = db.execute("SELECT id, body FROM messages WHERE body LIKE ? LIMIT 1",
                               (TEXT_MARK + "%",)).fetchone()
        if encrypted:
            cipher.open_text("messages", encrypted[0], "body", encrypted[1])
        last_id = 0
        while True:
            rows = db.execute("SELECT id, body, audio_data, image_data, image_thumb "
                              "FROM messages WHERE id>? ORDER BY id LIMIT 32",
                              (last_id,)).fetchall()
            if not rows:
                break
            for message_id, body, audio, image, thumbnail in rows:
                updates = {}
                if not body.startswith(TEXT_MARK):
                    updates["body"] = cipher.seal_text("messages", message_id, "body", body)
                for column, value in (("audio_data", audio), ("image_data", image),
                                      ("image_thumb", thumbnail)):
                    if value is not None and not value.startswith(MARK):
                        updates[column] = cipher.seal_blob("messages", message_id, column, value)
                if updates:
                    db.execute("UPDATE messages SET " + ", ".join(key + "=?" for key in updates) +
                               " WHERE id=?", (*updates.values(), message_id))
                    changed = True
            last_id = rows[-1][0]
        last_user = ""
        while True:
            rows = db.execute("SELECT user_id, photo_data, photo_thumb FROM profiles "
                              "WHERE user_id>? ORDER BY user_id LIMIT 32",
                              (last_user,)).fetchall()
            if not rows:
                break
            for user_id, photo, thumbnail in rows:
                updates = {}
                for column, value in (("photo_data", photo), ("photo_thumb", thumbnail)):
                    if value is not None and not value.startswith(MARK):
                        updates[column] = cipher.seal_blob("profiles", user_id, column, value)
                if updates:
                    db.execute("UPDATE profiles SET " + ", ".join(key + "=?" for key in updates) +
                               " WHERE user_id=?", (*updates.values(), user_id))
                    changed = True
            last_user = rows[-1][0]
    if changed:
        with closing(sqlite3.connect(database_path, timeout=5, isolation_level=None)) as db:
            db.execute("PRAGMA secure_delete=ON")
            db.execute("VACUUM")
    return changed
