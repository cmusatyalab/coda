# TLS support

> *Note* this is mostly a braindump of the current thinking behind TLS
> support, this document and the ideas behind it still need to be more
> structured but we need to start using the system with TLS to get a
> better sense of what is useful, secure and feasible.

When `codatunnel` is enabled, we start a helper process that intercepts
all RPC2 messages and attempts to establish TCP connections to the
servers. One reason is that TCP connections are more reliable when
passing through masquerading firewalls.  Currently we fall back on UDP
when the TCP connection cannot be established, but the goal is to end up
disabling this fallback and remove obsoleted functionality in RPC2, such
as retransmissions.

The initial codatunnel implementation used plain TCP connections and
relied on RPC2's encryption for confidentiality. But the current
implementation leverages GnuTLS to get a secure end-to-end connection
between a Coda client and server, as well as between Coda servers. As a
result we can eventually remove the homegrown encryption implementation
in RPC2.

## Certificate hierarchy

Because Coda servers are quite different from websites, our plan is to
use our own signing hierarchy with our own Coda root certificate
authority.

The Coda Root Certificate Authority will be implemented on an encrypted
partition of a system that is normally disconnected from the network. It
will only connect to the network when the encrypted partition is
'locked' and only to install security updates. Aside from running the
authority software from the encrypted partition, we can put the actual
key onto a smartcard or usbkey like the Nitrokey
(https://github.com/sektioneins/micro-ca-tool) which can be kept
disconnected from the system during software updates.

The Coda Root CA public key will be included in the Coda source and
installed into /etc/coda/ssl/.

The Coda Root CA is used to sign only realm-specific CAs. These
certificates will be limited to only sign for dns names within the
realm/limited ip address ranges using the X509v3 Name Constraints
extension. We really only want to constrain based on dns names, but
because existing Coda clients/servers only know each other by their IPv4
address we probably will have to add ip address constraints as well. The
problem is that we get into untested territory here because web
certificates tend to sign domain names only.

The realm specific CA (f.i. 'coda.cs.cmu.edu CA') is then used to sign
the actual server certificates within a realm. The server certificate
and private key will be stored as /etc/coda/ssl/{server.crt,server.key}.

As people are deploying their own Coda servers/realm, they can ramp up
by initially creating a self-signed server certificate. By virtue of
being stored in /etc/coda/ssl it should be automatically accepted by the
locally running Coda client. The /etc/coda/server.crt file can be copied
to other systems as /etc/coda/ssl/server.name.crt. As the deployment
scales up this would lead to many certificates needing to get copied so
they can create a local realm certificate to sign individual server
certificates and either copy the realm certificate to /etc/coda/ssl on
all clients/servers, or have it signed by the Coda CA.

As far as certificate lifetimes, we aim to have the Coda root CA use a
10 year lifetime, but we could update the public cert whenever a new
Coda release is made, as a result any client should be able to validate
certificates for up to 10 years after the last release. If we
semi-automate the signing infrastructure (letsencrypt/ACME?) we may need
an additional intermediate CA certificate with a shorter (1 year?)
lifetime.

The realm specific CA certificate should probably have at most a 1 year
lifetime, since we don't know how well managed it is by realms. If the
signing infrastructure is automated it could possibly be as short as a
90-day cert.

Server certificates ideally would get rotated every 24 hours, we should
have a cronjob on the realm CA server that periodically checks if a
server certificate will expire in the next 8 hours and then tries to
push a new one out to the server. Or we could implement something
similar to letsencrypt where the servers have a cronjob to do the
renewals. I've added a sighup handler to codatunneld that forces a
reload of any certificates in /etc/coda/ssl/ so that the server does not
have to be restarted.
