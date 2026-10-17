"""Comprueba que Monocypher (la 3DS) y cryptography (Python) coinciden."""

import ctypes
import gc
import hashlib
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from cryptography.hazmat.primitives.asymmetric import x25519
from cryptography.hazmat.primitives.ciphers.aead import ChaCha20Poly1305
from cryptography.hazmat.primitives import serialization


class AeadContext(ctypes.Structure):
    _fields_ = [("counter", ctypes.c_uint64), ("key", ctypes.c_uint8 * 32),
                ("nonce", ctypes.c_uint8 * 8)]


class CryptoCompatibilityTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which("gcc"), "GCC necesario para probar Monocypher nativo")
    def test_x25519_blake2b_and_chacha20_poly1305(self):
        source = Path(__file__).resolve().parents[1] / "source" / "crypto" / "monocypher.c"
        with tempfile.TemporaryDirectory() as directory:
            library_path = Path(directory) / ("monocypher.dll" if sys.platform == "win32" else
                                             "libmonocypher.so")
            subprocess.run(["gcc", "-O2", "-shared", "-fPIC", str(source), "-o",
                            str(library_path)], check=True, capture_output=True)
            library = ctypes.CDLL(str(library_path))
            private = bytes(range(1, 33))
            server_private = bytes(range(33, 65))
            server_public = x25519.X25519PrivateKey.from_private_bytes(
                server_private).public_key().public_bytes(
                    serialization.Encoding.Raw, serialization.PublicFormat.Raw)
            native_public = ctypes.create_string_buffer(32)
            native_shared = ctypes.create_string_buffer(32)
            library.crypto_x25519_public_key(native_public, private)
            library.crypto_x25519(native_shared, private, server_public)
            self.assertEqual(native_public.raw,
                             x25519.X25519PrivateKey.from_private_bytes(
                                 private).public_key().public_bytes(
                                     serialization.Encoding.Raw, serialization.PublicFormat.Raw))
            shared = x25519.X25519PrivateKey.from_private_bytes(private).exchange(
                x25519.X25519PublicKey.from_public_bytes(server_public))
            self.assertEqual(native_shared.raw, shared)
            material = shared + native_public.raw + server_public + bytes(16) + \
                b"Mensinger3DS-channel-v1"
            key_buffer = ctypes.create_string_buffer(32)
            library.crypto_blake2b(key_buffer, 32, material, len(material))
            key = hashlib.blake2b(material, digest_size=32).digest()
            self.assertEqual(key_buffer.raw, key)
            plain = b"Mensaje, clave y foto \x00\x01\x02"
            nonce = bytes(12)
            aad = b"M3DS1Q"
            context = AeadContext()
            cipher = ctypes.create_string_buffer(len(plain))
            mac = ctypes.create_string_buffer(16)
            library.crypto_aead_init_ietf(ctypes.byref(context), key, nonce)
            library.crypto_aead_write(ctypes.byref(context), cipher, mac, aad, len(aad),
                                      plain, len(plain))
            self.assertEqual(cipher.raw + mac.raw,
                             ChaCha20Poly1305(key).encrypt(nonce, plain, aad))
            if sys.platform == "win32":
                # Windows bloquea el archivo de una DLL cargada hasta FreeLibrary.
                ctypes.windll.kernel32.FreeLibrary.argtypes = [ctypes.c_void_p]
                self.assertTrue(ctypes.windll.kernel32.FreeLibrary(library._handle))
            del library
            gc.collect()


if __name__ == "__main__":
    unittest.main()
