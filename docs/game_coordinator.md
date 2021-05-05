# Game Coordinator

To allow two players to play together, OpenTTD uses a Game Coordinator to
facilitate this.

When a server starts, it can register itself to the Game Coordinator. This
happens when `server_game_type` is set to either `invite-only` or `public`.
Upon registration, the Game Coordinator probes the network of the server, and
assigns the server an unique code, called an `invite code`.

When a client wants to join a server, it asks the Game Coordinator to help
with this by giving it the `invite code` of the server. The Game Coordinator
will, in this order, attempt several ways to connect the client and server
together:

## 1) Via direct IPv4 / IPv6

Upon registration, the Game Coordinator probes the server to see if a
direction connection to the server is possible based on the game port. It
tries this over the public IPv4 and IPv6, as announced by the server.
If either (or both) are successful, a client will always be asked to try
these direct IPs first when it wants to connect to this server.

- If the server is IPv4 only, the client is only asked to connect via IPv4.
- If the server is IPv6 only, the client is only asked to connect via IPv6.
- If the server is both IPv4 and IPv6, the client is asked to connect to to
  one of those first. If that fails, it is asked to connect to the other.
  Whether it tries IPv4 or IPv6 first, strongly depends on several network
  infrastructure related events. The biggest influence is the network
  latency over both protocols to the OpenTTD infrastructure.

In the end, if either the server is not reachable directly from the Internet
or the client fails to connect to either one of them, the connection attempt
continues with the next available method.

## 2) Via STUN

When a client wants to join a server via STUN, both the client and server
are asked to create a connection to the STUN server (normally
`stun.openttd.org`) via both IPv4 and IPv6. If the client or server has no
IPv4 or IPv6, it will not make a connection attempt via that protocol.

The STUN server collects the public IPv4 and/or IPv6 of the client and server,
together with the port the connection came in from. For most NAT gateways it
holds true that as long as you use the same local IP + port, your public
IP + port will remain the same, and allow for bi-directional communication.
And this fact is used to later on pair the client and server.

The STUN server reports this information directly to the Game Coordinator
(this in contrast to most STUN implementation, where this information is
first reported back to the peer itself, which has to relay it back to the
coordinator. OpenTTD skips this step, as the STUN server can directly talk to
the Game Coordinator). When the Game Coordinator sees a matching pair (in
terms of IPv4 / IPv6), it will ask both the client and server to connect to
their peer based on this public IP + port.

As the NAT gateway forwards the traffic on the public IP + port to the local
port, this creates a bi-directional socket between client and server. The
connection to the STUN server can now safely be closed, and the client and
server can continue to talk to each other.

Some NAT gateways do not allow this method; for those this attempt will fail,
and this also means that it is not possible to create a connection between
the client and server.

## 3) Via TURN

As a last resort, the Game Coordinator can decide to connect the client and
server together via TURN. TURN is a relay service, relaying the messages
between client and server.

As the client and server can already connect to the Game Coordinator, it is
very likely this is successful.

It is important to note that a relay service has full view of the traffic
send between client and server, and as such it is important that you trust
the relay service used.
For official binaries, this relay service is hosted by openttd.org. The relay
service as hosted by openttd.org only validates it is relaying valid OpenTTD
packets and does no further inspection of the payload itself.
Although in our experience most patch-packs also use the services as offered
by openttd.org, it is possible they use different services. Please be mindful
about this.
