"""Generación persistente de la CA privada y certificado TLS del servidor.

La CA firma un certificado servidor con SAN para cada host configurado. La
consola recibe únicamente el DER público de la CA; la clave de firma permanece
en el directorio TLS del servidor.
"""

import hashlib
import ipaddress
import os
import socket
from datetime import datetime, timedelta, timezone
from pathlib import Path

from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.x509.oid import ExtendedKeyUsageOID, NameOID


def local_hosts(extra=()):
    """Reúne localhost, nombre/IP locales y hosts adicionales solicitados."""
    hosts = {"localhost", "127.0.0.1"}
    try:
        machine = socket.gethostname()
        hosts.add(machine)
        hosts.update(socket.gethostbyname_ex(machine)[2])
    except OSError:
        pass
    hosts.update(extra)
    hosts.discard("0.0.0.0")
    return sorted(hosts)


def _subject_alt_name(host):
    """Interpreta un host como SAN IP o DNS y valida su forma básica."""
    try:
        return x509.IPAddress(ipaddress.ip_address(host))
    except ValueError:
        if not host or len(host) > 253 or any(char.isspace() for char in host):
            raise ValueError(f"Host TLS inválido: {host!r}")
        return x509.DNSName(host)


def ensure_tls_files(directory, extra_hosts=()):
    """Crea o valida la identidad TLS y renueva solo el certificado servidor.

    La CA permanece estable para conservar la confianza de consolas ya
    configuradas. Un conjunto parcial o incoherente de archivos es un error que
    exige restaurar la identidad original antes de iniciar el servidor.
    """
    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=True)
    cert_path = directory / "server-cert.pem"
    key_path = directory / "server-key.pem"
    ca_cert_path = directory / "server-ca.pem"
    ca_key_path = directory / "server-ca-key.pem"
    trust_path = directory / "servidor-ca.der"
    existing = [path.exists() for path in (cert_path, key_path, ca_cert_path,
                                          ca_key_path, trust_path)]
    hosts = local_hosts(extra_hosts)
    if any(existing) and not all(existing):
        raise ValueError("Faltan archivos TLS existentes. Recupera la identidad original.")
    if not all(existing):
        ca_key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
        ca_name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, "Mensinger 3DS CA")])
        now = datetime.now(timezone.utc)
        ca_certificate = (x509.CertificateBuilder()
            .subject_name(ca_name).issuer_name(ca_name).public_key(ca_key.public_key())
            .serial_number(x509.random_serial_number())
            .not_valid_before(now - timedelta(days=1))
            .not_valid_after(now + timedelta(days=3650))
            .add_extension(x509.BasicConstraints(ca=True, path_length=0), critical=True)
            .add_extension(x509.KeyUsage(digital_signature=True, key_encipherment=False,
                                         key_cert_sign=True, crl_sign=False,
                                         content_commitment=False, data_encipherment=False,
                                         key_agreement=False, encipher_only=False,
                                         decipher_only=False), critical=True)
            .sign(ca_key, hashes.SHA256()))
        key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
        name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, hosts[0])])
        certificate = (x509.CertificateBuilder()
            .subject_name(name).issuer_name(ca_name).public_key(key.public_key())
            .serial_number(x509.random_serial_number())
            .not_valid_before(now - timedelta(days=1))
            .not_valid_after(now + timedelta(days=1825))
            .add_extension(x509.SubjectAlternativeName([_subject_alt_name(h) for h in hosts]),
                           critical=False)
            .add_extension(x509.BasicConstraints(ca=False, path_length=None), critical=True)
            .add_extension(x509.KeyUsage(digital_signature=True, key_encipherment=True,
                                         key_cert_sign=False, crl_sign=False,
                                         content_commitment=False, data_encipherment=False,
                                         key_agreement=False, encipher_only=False,
                                         decipher_only=False), critical=True)
            .add_extension(x509.ExtendedKeyUsage([ExtendedKeyUsageOID.SERVER_AUTH]),
                           critical=False)
            .sign(ca_key, hashes.SHA256()))
        private_pem = key.private_bytes(serialization.Encoding.PEM,
                                         serialization.PrivateFormat.TraditionalOpenSSL,
                                         serialization.NoEncryption())
        descriptor = os.open(key_path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(private_pem)
        descriptor = os.open(ca_key_path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(ca_key.private_bytes(serialization.Encoding.PEM,
                serialization.PrivateFormat.TraditionalOpenSSL,
                serialization.NoEncryption()))
        cert_path.write_bytes(certificate.public_bytes(serialization.Encoding.PEM))
        ca_cert_path.write_bytes(ca_certificate.public_bytes(serialization.Encoding.PEM))
        trust_path.write_bytes(ca_certificate.public_bytes(serialization.Encoding.DER))
    else:
        certificate = x509.load_pem_x509_certificate(cert_path.read_bytes())
        root = x509.load_pem_x509_certificate(ca_cert_path.read_bytes())
        der = trust_path.read_bytes()
        if der != root.public_bytes(serialization.Encoding.DER):
            raise ValueError("El certificado TLS y servidor-ca.der no coinciden.")
        ca_key = serialization.load_pem_private_key(ca_key_path.read_bytes(), password=None)
        key = serialization.load_pem_private_key(key_path.read_bytes(), password=None)
        public_format = serialization.PublicFormat.SubjectPublicKeyInfo
        if (root.public_key().public_bytes(serialization.Encoding.DER, public_format) !=
                ca_key.public_key().public_bytes(serialization.Encoding.DER, public_format) or
                certificate.public_key().public_bytes(serialization.Encoding.DER, public_format) !=
                key.public_key().public_bytes(serialization.Encoding.DER, public_format) or
                certificate.issuer != root.subject):
            raise ValueError("Las claves y certificados TLS existentes no coinciden.")
        actual = certificate.extensions.get_extension_for_class(
            x509.SubjectAlternativeName).value
        existing_hosts = set(actual.get_values_for_type(x509.DNSName))
        existing_hosts.update(str(ip) for ip in actual.get_values_for_type(x509.IPAddress))
        wanted_hosts = sorted(existing_hosts | set(hosts))
        if any(_subject_alt_name(host) not in actual for host in hosts):
            # Renueva solo el certificado del servidor. La CA fijada por las 3DS no cambia.
            now = datetime.now(timezone.utc)
            renewed = (x509.CertificateBuilder()
                .subject_name(certificate.subject).issuer_name(root.subject)
                .public_key(key.public_key()).serial_number(x509.random_serial_number())
                .not_valid_before(now - timedelta(days=1))
                .not_valid_after(now + timedelta(days=1825))
                .add_extension(x509.SubjectAlternativeName(
                    [_subject_alt_name(host) for host in wanted_hosts]), critical=False)
                .add_extension(x509.BasicConstraints(ca=False, path_length=None), critical=True)
                .add_extension(x509.KeyUsage(digital_signature=True, key_encipherment=True,
                    key_cert_sign=False, crl_sign=False, content_commitment=False,
                    data_encipherment=False, key_agreement=False, encipher_only=False,
                    decipher_only=False), critical=True)
                .add_extension(x509.ExtendedKeyUsage([ExtendedKeyUsageOID.SERVER_AUTH]),
                    critical=False)
                .sign(ca_key, hashes.SHA256()))
            temporary = cert_path.with_suffix(".tmp")
            temporary.write_bytes(renewed.public_bytes(serialization.Encoding.PEM))
            os.replace(temporary, cert_path)
    fingerprint = hashlib.sha256(trust_path.read_bytes()).hexdigest().upper()
    return cert_path, key_path, trust_path, fingerprint
