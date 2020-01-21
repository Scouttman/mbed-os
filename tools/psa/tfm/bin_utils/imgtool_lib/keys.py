# Copyright (c) 2017,2019 Linaro Limited.
# Copyright (c) 2017-2019, Arm Limited.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""
Cryptographic key management for imgtool.
"""

from __future__ import print_function
from cryptography.hazmat.backends import default_backend
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.hashes import SHA256
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.hazmat.primitives.asymmetric.padding import PSS, PKCS1v15
from cryptography.hazmat.primitives.asymmetric.padding import MGF1
import hashlib
from pyasn1.type import namedtype, univ
from pyasn1.codec.der.encoder import encode

# Sizes that bootutil will recognize
RSA_KEY_SIZES = [2048, 3072]

# Public exponent
PUBLIC_EXPONENT = 65537

# By default, we use RSA-PSS (PKCS 2.1).  That can be overridden on
# the command line to support the older (less secure) PKCS1.5
sign_rsa_pss = True

AUTOGEN_MESSAGE = "/* Autogenerated by imgtool.py, do not edit. */"

class RSAUsageError(Exception):
    pass

class RSAutil():
    def __init__(self, key, public_key_format='hash'):
        """Construct an RSA key with the given key data"""
        self.key = key
        self.public_key_format = public_key_format

    def key_size(self):
        return self.key.key_size

    def get_public_key_format(self):
        return self.public_key_format

    @staticmethod
    def generate(key_size=2048):
        if key_size not in RSA_KEY_SIZES:
            raise RSAUsageError("Key size {} is not supported by MCUboot"
                                .format(key_size))
        return RSAutil(rsa.generate_private_key(
                public_exponent=PUBLIC_EXPONENT,
                key_size=key_size,
                backend=default_backend()))

    def export_private(self, path):
        with open(path, 'wb') as f:
            f.write(self.key.private_bytes(
                    encoding=serialization.Encoding.PEM,
                    format=serialization.PrivateFormat.TraditionalOpenSSL,
                    encryption_algorithm=serialization.NoEncryption()))

    def get_public_bytes(self):
        return self.key.public_key().public_bytes(
                encoding=serialization.Encoding.DER,
                format=serialization.PublicFormat.PKCS1)

    def emit_c(self):
        print(AUTOGEN_MESSAGE)
        print("const unsigned char rsa_pub_key[] = {", end='')
        encoded = self.get_public_bytes()
        for count, b in enumerate(encoded):
            if count % 8 == 0:
                print("\n\t", end='')
            else:
                print(" ", end='')
            print("0x{:02x},".format(b), end='')
        print("\n};")
        print("const unsigned int rsa_pub_key_len = {};".format(len(encoded)))

    def sig_type(self):
        """Return the type of this signature (as a string)"""
        if sign_rsa_pss:
            return "PKCS1_PSS_RSA{}_SHA256".format(self.key_size())
        else:
            return "PKCS15_RSA{}_SHA256".format(self.key_size())

    def sig_len(self):
        return 256 if self.key_size() == 2048 else 384

    def sig_tlv(self):
        return "RSA2048" if self.key_size() == 2048 else "RSA3072"

    def sign(self, payload):
        if sign_rsa_pss:
            signature = self.key.sign(
                data=payload,
                padding=PSS(
                    mgf=MGF1(SHA256()),
                    salt_length=32
                ),
                algorithm=SHA256()
            )
        else:
            signature = self.key.sign(
                data=payload,
                padding=PKCS1v15(),
                algorithm=SHA256()
            )
        assert len(signature) == self.sig_len()
        return signature

def load(path, public_key_format='hash'):
    with open(path, 'rb') as f:
        pem = f.read()
    try:
        key = serialization.load_pem_private_key(
            pem,
            password=None,
            backend=default_backend()
        )
        return RSAutil(key, public_key_format)
    except ValueError:
        raise Exception("Unsupported RSA key file")
