---
title: CODA-MAKE-CERTS(1)
footer: Coda Distributed File System
---

## NAME

coda-make-certs - Generate X509 certificates for Coda realms and servers

## SYNOPSIS

**coda-make-certs** [-n] [-q] [\-\-dump-config] [\-\-scp] realm\_config.yaml

## DESCRIPTION

**coda-make-certs** generates X509 certificates for Coda realms and servers. It
uses `certtool` from gnutls-bin to perform the actual X509 related operations
and optionally `scp` to copy the signed certificates to the Coda servers.

A minimal configuration for a single realm with a single server is as simple
as,

```yaml
realm: server.example.org
```

This generates private realm and server keys, a certificate request and a
self-signed certificate for the realm and a signed server certificate.

If you want to use the self-signed realm certificate it should be copied to
`/etc/coda/ssl/<realmname>.crt` on all Coda clients and servers. You can also
send the certificate request to <coda-ca@coda.cs.cmu.edu> to get it signed
by the Coda\_CA key in which case any Coda client will be able to verify your
server certificates without having to install your self-signed realm certificate.

The server certificate is copied to the right location on your server
when you add **--scp** as argument.

A more complete configuration may have multiple realms, specify a list of
servers with or without ip addresses and cnames, and override default realm
and server certificate lifetimes. It would end up looking something like this,

```yaml
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
```

## OPTIONS

-n, \-\-dry-run

:   Do not perform any actions, just print what would be done.

-q, \-\-quiet

:   Be quiet, only display error messages.

\-\-scp

:   Copy generated certificates to servers.

\-\-dump-config

:   Write complete config to stdout and exit.
