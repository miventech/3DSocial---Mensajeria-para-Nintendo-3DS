"""Canal de aplicación cifrado sobre HTTP para Mensinger 3DS.

El servidor crea una identidad X25519 una sola vez. Cada petición recibe un
desafío de un solo uso; la consola genera una clave X25519 efímera. Ambas partes
derivan una clave con BLAKE2b y usan ChaCha20-Poly1305 (RFC 8439). El servidor
nunca recibe por HTTP la clave privada de la consola. La clave pública del
servidor se fija en la SD de la consola tras el primer contacto (TOFU).
"""

import hashlib
import os
import secrets
import struct
import threading
import time
from pathlib import Path

from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import x25519
from cryptography.hazmat.primitives.ciphers.aead import ChaCha20Poly1305


KEY_MAGIC = b"M3K1"
REQUEST_MAGIC = b"M3Q1"
RESPONSE_MAGIC = b"M3R1"
KDF_LABEL = b"Mensinger3DS-channel-v1"
REQUEST_AAD = b"M3DS1Q"
RESPONSE_AAD = b"M3DS1R"
REQUEST_NONCE = bytes(12)
RESPONSE_NONCE = bytes(11) + b"\x01"
MAX_INNER_BODY = 720000
MAX_ENVELOPE = MAX_INNER_BODY + 2048
CHALLENGE_LIFETIME = 180


class SecureChannel:
    """Identidad persistente y desafíos efímeros, compartidos por los hilos HTTP."""

    def __init__(self, key_path):
        self.key_path = Path(key_path)
        self.key_path.parent.mkdir(parents=True, exist_ok=True)
        try:
            secret = self.key_path.read_bytes()
        except FileNotFoundError:
            secret = os.urandom(32)
            try:
                descriptor = os.open(str(self.key_path), os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
            except FileExistsError:
                secret = self.key_path.read_bytes()
            else:
                with os.fdopen(descriptor, "wb") as output:
                    output.write(secret)
        if len(secret) != 32:
            raise ValueError("Clave de transporte inválida; restaura la original")
        self.private_key = x25519.X25519PrivateKey.from_private_bytes(secret)
        self.public_key = self.private_key.public_key().public_bytes(
            serialization.Encoding.Raw, serialization.PublicFormat.Raw)
        self.challenges = {}
        self.lock = threading.Lock()

    def issue_challenge(self):
        """Emite un desafío aleatorio que caduca y solo se puede consumir una vez."""
        with self.lock:
            now = time.monotonic()
            self.challenges = {value: expiry for value, expiry in self.challenges.items()
                               if expiry > now}
            if len(self.challenges) >= 4096:
                raise ValueError("Demasiados desafíos pendientes")
            challenge = secrets.token_bytes(16)
            self.challenges[challenge] = now + CHALLENGE_LIFETIME
        return KEY_MAGIC + self.public_key + challenge

    def consume_challenge(self, challenge):
        with self.lock:
            expiry = self.challenges.pop(challenge, 0)
        if expiry <= time.monotonic():
            raise ValueError("Desafío caducado o repetido")

    def open_request(self, packet):
        """Autentica un sobre y devuelve petición interna y clave para responder."""
        if not (68 <= len(packet) <= MAX_ENVELOPE) or packet[:4] != REQUEST_MAGIC:
            raise ValueError("Sobre cifrado inválido")
        challenge, client_public, mac = packet[4:20], packet[20:52], packet[52:68]
        self.consume_challenge(challenge)
        try:
            shared = self.private_key.exchange(
                x25519.X25519PublicKey.from_public_bytes(client_public))
            key = hashlib.blake2b(shared + client_public + self.public_key + challenge +
                                  KDF_LABEL, digest_size=32).digest()
            plaintext = ChaCha20Poly1305(key).decrypt(
                REQUEST_NONCE, packet[68:] + mac, REQUEST_AAD)
        except Exception as exc:
            raise ValueError("Autenticación del sobre fallida") from exc
        return decode_request(plaintext), key

    @staticmethod
    def seal_response(status, body, key):
        plain = struct.pack(">HI", status, len(body)) + body
        sealed = ChaCha20Poly1305(key).encrypt(RESPONSE_NONCE, plain, RESPONSE_AAD)
        return RESPONSE_MAGIC + sealed[-16:] + sealed[:-16]


def decode_request(plain):
    """Lee exactamente una petición binaria acotada; no acepta bytes sobrantes."""
    if len(plain) < 10:
        raise ValueError("Petición incompleta")
    method, path_len, type_len, user_len, token_len, body_len = struct.unpack(">BHBBBI", plain[:10])
    if (method not in (0, 1) or not 1 <= path_len <= 1024 or type_len > 96 or
            user_len > 10 or token_len > 128 or body_len > MAX_INNER_BODY or
            len(plain) != 10 + path_len + type_len + user_len + token_len + body_len):
        raise ValueError("Longitudes de petición inválidas")
    offset = 10
    values = []
    for size in (path_len, type_len, user_len, token_len):
        try:
            values.append(plain[offset:offset + size].decode("ascii"))
        except UnicodeDecodeError as exc:
            raise ValueError("Cabecera interna inválida") from exc
        offset += size
    path, content_type, user_id, token = values
    if (not path.startswith("/") or any(char in path for char in "\r\n\x00") or
            any(char in content_type for char in "\r\n\x00") or
            not (not user_id or user_id.isascii() and user_id.isdigit()) or
            any(char in token for char in "\r\n\x00") or
            path.split("?", 1)[0] in ("/canal", "/clave-servidor")):
        raise ValueError("Campos internos inválidos")
    return ("GET" if method == 0 else "POST", path, content_type,
            user_id, token, plain[offset:])
