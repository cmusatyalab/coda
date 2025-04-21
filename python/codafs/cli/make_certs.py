#!/usr/bin/env python3
#
#                          Coda File System
#                             Release 8
#
#         Copyright (c) 2020-2021 Carnegie Mellon University
#
# This  code  is  distributed "AS IS" without warranty of any kind under
# the terms of the GNU General Public License Version 2, as shown in the
# file  LICENSE.  The  technical and financial  contributors to Coda are
# listed in the file CREDITS.
#
#
"""coda-make-certs - Generate X509 certificates for Coda realms and servers.

depends on pyyaml for parsing the config file.
depends on 'certtool' from gnutls-bin to perform the X509 related operations.
optional dependencies on jsonschema and ipaddress (for validation)
"""

import argparse
import logging
import pathlib
import socket
import sys
import tempfile
from contextlib import closing

import yaml
from codafs.optional import ValidationError, jsonschema_validate
from codafs.util import DEVNULL, run

# parse arguments
parser = argparse.ArgumentParser(
    formatter_class=argparse.RawDescriptionHelpFormatter,
    description="""Generate X509 certificates for Coda realms and servers.

Minimal configuration for a single realm with a single server is as simple as,

    realm: server.example.org

This generates private realm and server keys, a certificate request and a
self-signed certificate for the realm and a signed server certificate.

If you want to use the self-signed realm certificate it should be copied to
/etc/coda/ssl/<realmname>.crt on all Coda clients and servers. You can also
send the certificate request to <coda-ca@coda.cs.cmu.edu> to get it signed
by the Coda_CA key in which case any client should be able to verify your
server certificates.

The server certificate is copied to the right location on your server
when you add '--scp' as argument.

A more complete configuration may have multiple realms, specify a list of
servers with or without ip addresses and cnames, and override default realm
and server certificate lifetimes. It would end up looking something like this,

    realm: realm1.example.org
    servers:
    - server1.example.org
    - server2.example.org
    ---
    realm: realm2.example.org
    realm_expiration_days: 365
    server_expiration_days: 7
    servers:
    - name: server3.example.org
      address: 127.0.0.3
      cnames:
      - realm2-server.example.org
""",
)
parser.add_argument(
    "--dump-config",
    action="store_true",
    help="write complete config to stdout and exit",
)
parser.add_argument(
    "-q",
    "--quiet",
    action="store_true",
    help="only display error messages",
)
parser.add_argument(
    "-n",
    "--dry-run",
    action="store_true",
    help="avoid running commands that alter disk-state",
)
parser.add_argument(
    "--scp",
    action="store_true",
    help="copy generated certificates to servers",
)
parser.add_argument("config", metavar="realm_config.yaml", type=open)
args = parser.parse_args()

logging.basicConfig(
    format="%(levelname)s - %(message)s",
    level=logging.ERROR if args.quiet else logging.INFO,
)


# jsonschema for validation
REALM_CONFIG_SCHEMA = {
    "$schema": "http://json-schema.org/draft-07/schema#",
    "$id": "http://coda.cs.cmu.edu/schemas/realm_config.json",
    "$defs": {
        "dnsname": {"type": "string", "format": "hostname"},
        "ipaddress": {"type": "string", "format": "ipv4"},
        "iprange": {
            "type": "string",
            "format": "ipv4network",
            "pattern": "^[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+/[0-9]+$",
        },
        "dnsname_list": {
            "type": "array",
            "uniqueItems": True,
            "items": {"$ref": "#/$defs/dnsname"},
        },
        "iprange_list": {
            "type": "array",
            "uniqueItems": True,
            "items": {"$ref": "#/$defs/iprange"},
        },
        "server": {
            "type": "object",
            "properties": {
                "name": {"$ref": "#/$defs/dnsname"},
                "address": {"$ref": "#/$defs/ipaddress"},
                "cnames": {"$ref": "#/$defs/dnsname_list"},
            },
            "required": ["name"],
        },
        "server_list": {
            "type": "array",
            "items": {
                "anyOf": [{"$ref": "#/$defs/server"}, {"$ref": "#/$defs/dnsname"}],
            },
        },
    },
    "type": "object",
    "properties": {
        "realm": {"$ref": "#/$defs/dnsname"},
        "servers": {"$ref": "#/$defs/server_list"},
        "permit_dns": {"$ref": "#/$defs/dnsname_list"},
        "permit_ip": {"$ref": "#/$defs/iprange_list"},
        "exclude_dns": {"$ref": "#/$defs/dnsname_list"},
        "exclude_ip": {"$ref": "#/$defs/iprange_list"},
        "realm_expiration_days": {"type": "integer", "default": 365},
        "server_expiration_days": {"type": "integer", "default": 7},
    },
    "required": ["realm"],
}


def validate(configuration):
    try:
        jsonschema_validate(configuration, REALM_CONFIG_SCHEMA)
    except ValidationError as exception:
        print(exception)
        sys.exit(1)

    # jsonschema doesn't set defaults because it avoids modifying the validated
    # datastructure. It seems easier to fold them in ourselves than to override
    # the validator behavior.
    realm_config_defaults = dict(realm_expiration_days=365, server_expiration_days=7)
    return {**realm_config_defaults, **configuration}


# paths and templates for realm and server certificates
REALMDIR = pathlib.Path("realms")
SERVERDIR = pathlib.Path("servers")

REALM_TEMPLATE = """\
cn = "{realm} Realm CA"
expiration_days = {expiration}
challenge_password = 123456
ca
cert_signing_key
crl_signing_key
"""

SERVER_TEMPLATE = """\
cn = "{name}"
dns_name = "{name}"
ip_address = "{address}"
expiration_days = {expiration}
challenge_password = 123456
signing_key
encryption_key
"""


# pylint: disable=E1101
class Server(yaml.YAMLObject):
    yaml_tag = "tag:yaml.org,2002:map"

    def __getstate__(self):
        state = self.__dict__.copy()
        del state["realm"]
        return state

    def __init__(self, realm, server):
        if not isinstance(server, dict):
            server = dict(name=server)
        if "address" not in server:
            server["address"] = socket.gethostbyname(server["name"])
        self.__dict__ = server
        self.realm = realm

    def template(self):
        template = tempfile.NamedTemporaryFile()
        template.write(
            SERVER_TEMPLATE.format(
                name=self.name,
                address=self.address,
                expiration=self.realm.server_expiration_days,
            ).encode("ascii"),
        )
        for cname in self.__dict__.get("cnames", []):
            template.write(f'dns_name = "{cname}"\n'.encode("ascii"))
        template.flush()
        return closing(template)

    def copy_certificate(self):
        flags = "-Bq" if args.quiet else "-B"
        run(
            "scp",
            flags,
            self.key,
            f"root@{self.name}:/etc/coda/ssl/server.key",
            dry_run=args.dry_run,
        )
        run(
            "scp",
            flags,
            self.crt,
            f"root@{self.name}:/etc/coda/ssl/server.crt",
            dry_run=args.dry_run,
        )

    @property
    def key(self):
        return SERVERDIR / (self.name + ".key")

    @property
    def crt(self):
        return SERVERDIR / (self.name + ".crt")


# pylint: disable=E1101
class Realm(yaml.YAMLObject):
    yaml_tag = "tag:yaml.org,2002:map"

    def __init__(self, realm):
        self.__dict__ = realm
        servers = []
        # remap server list to Server objects
        for server in realm.get("servers", [realm["realm"]]):
            servers.append(Server(self, server))
        self.servers = servers

    def template(self):
        template = tempfile.NamedTemporaryFile()
        template.write(
            REALM_TEMPLATE.format(
                realm=self.realm,
                expiration=self.realm_expiration_days,
            ).encode("ascii"),
        )
        for key in ["permit_dns", "permit_ip", "exclude_dns", "exclude_ip"]:
            for name in self.__dict__.get(key, []):
                template.write(f'nc_{key} = "{name}"\n'.encode("ascii"))
        template.flush()
        return closing(template)

    def __iter__(self):
        return self.servers.__iter__()

    @property
    def key(self):
        return REALMDIR / (self.realm + ".key")

    @property
    def csr(self):
        return REALMDIR / (self.realm + ".csr")

    @property
    def crt(self):
        return REALMDIR / (self.realm + ".crt")


def generate_privkey(key):
    if not key.exists():
        logging.info("generating %s", key)
        run(
            "certtool",
            "--generate-privkey",
            "--rsa",
            "--sec-param=medium",  # 2048-bit RSA key
            "--outfile",
            key,
            stderr=DEVNULL,
            dry_run=args.dry_run,
        )


def generate_realmcerts(realm):
    with realm.template() as template:
        logging.info("generating %s", realm.csr)
        run(
            "certtool",
            "--generate-request",
            "--template",
            template.name,
            "--load-privkey",
            realm.key,
            "--outfile",
            realm.csr,
            stderr=DEVNULL,
            dry_run=args.dry_run,
        )

        if not realm.crt.exists():
            logging.info("generating self-signed %s", realm.crt)
            run(
                "certtool",
                "--generate-self-signed",
                "--template",
                template.name,
                "--load-privkey",
                realm.key,
                "--outfile",
                realm.crt,
                stderr=DEVNULL,
                dry_run=args.dry_run,
            )
        else:
            logging.info("not replacing %s with self-signed certificate", realm.crt)


def generate_servercert(realm, server):
    logging.info("generating %s", server.crt)
    with server.template() as template:
        run(
            "certtool",
            "--generate-certificate",
            "--template",
            template.name,
            "--load-ca-certificate",
            realm.crt,
            "--load-ca_privkey",
            realm.key,
            "--load-privkey",
            server.key,
            "--outfile",
            server.crt,
            stderr=DEVNULL,
            dry_run=args.dry_run,
        )

    # append realm certificate to server certificate
    if not args.dry_run:
        with server.crt.open("a") as server_cert:
            server_cert.write(realm.crt.read_text())
    else:
        print(f"cat {realm.crt} >> {server.crt}")


def main():
    # Load realms from configuration file
    realms = [validate(realm) for realm in yaml.safe_load_all(args.config)]

    # convert realms to objects
    realms = [Realm(realm) for realm in realms]

    if args.dump_config:
        yaml.dump_all(realms, sys.stdout)
        sys.exit(0)

    # Create directories to store certificates and keys
    if not args.dry_run:
        REALMDIR.mkdir(mode=0o700, exist_ok=True)
        SERVERDIR.mkdir(mode=0o700, exist_ok=True)
    else:
        print(f"mkdir -p {REALMDIR} {SERVERDIR}")

    # create keys and certificates
    for realm in realms:
        generate_privkey(realm.key)
        generate_realmcerts(realm)

        for server in realm:
            generate_privkey(server.key)
            generate_servercert(realm, server)

    # and copy to servers
    if args.scp:
        for realm in realms:
            for server in realm:
                server.copy_certificate()


if __name__ == "__main__":
    main()
