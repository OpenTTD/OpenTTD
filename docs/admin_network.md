# OpenTTD's admin network

Last updated:    2011-01-20


## Table of contents

- 1.0) [Preface](#10-preface)
- 2.0) [Joining the network](#20-joining-the-network)
- 3.0) [Asking for updates](#30-asking-for-updates)
    - 3.1) [Polling manually](#31-polling-manually)
- 4.0) [Sending rcon commands](#40-sending-rcon-commands)
- 5.0) [Sending chat](#50-sending-chat)
    - 5.1) [Receiving chat](#51-receiving-chat)
- 6.0) [Disconnecting](#60-disconnecting)
- 7.0) [Certain packet information](#70-certain-packet-information)


## 1.0) Preface

  The admin network provides a dedicated network protocol designed for other
  applications to communicate with OpenTTD. Connected applications can execute
  console commands remotely (rcon commands) with no further authentication.
  Furthermore information about clients and companies can be provided.

  Admin applications remain connected when starting a new game or loading a saved
  game in contrast to normal OpenTTD clients that get disconnected.

  This document describes the admin network and its protocol.

  Please refer to the mentioned enums in `src/network/core/tcp_admin.h`

  Please also note that further improvements to the admin protocol can mean that
  more packet types will be sent by the server. For forward compatibility it is
  therefore wise to ignore unknown packets. Future improvements might also add
  additional data to packets. This data should be ignored. Data will never be
  removed from packets in later versions, except the possibility that complete
  packets are dropped in favour of a new packet.

  This though will be reflected in the protocol version as announced in the
  `ADMIN_PACKET_SERVER_PROTOCOL` in section 2.0).

  A reference implementation in Java for a client connecting to the admin interface
  can be found at: [http://dev.openttdcoop.org/projects/joan](http://dev.openttdcoop.org/projects/joan)


## 2.0) Joining the network

  Create a TCP connection to the server on port 3977. The application is
  expected to authenticate within 10 seconds.

  To authenticate send a `ADMIN_PACKET_ADMIN_JOIN` packet.

  The server will reply with `ADMIN_PACKET_SERVER_PROTOCOL` followed directly by
  `ADMIN_PACKET_SERVER_WELCOME`.

  `ADMIN_PACKET_SERVER_PROTOCOL` contains details about the protocol version.
  It is the job of your application to check this number and decide whether
  it will remain connected or not.
  Furthermore, this packet holds details on every `AdminUpdateType` and the
  supported `AdminFrequencyTypes` (bitwise representation).

  `ADMIN_PACKET_SERVER_WELCOME` contains details on the server and the map,
  e.g. if the server is dedicated, its NetworkLanguage, size of the Map, etc.

  Once you have received `ADMIN_PACKET_SERVER_WELCOME` you are connected and
  authorized to do your thing.

  The server will not provide any game related updates unless you ask for them.
  There are four packets the server will none the less send, if applicable:

    - ADMIN_PACKET_SERVER_ERROR
    - ADMIN_PACKET_SERVER_WELCOME
    - ADMIN_PACKET_SERVER_NEWGAME
    - ADMIN_PACKET_SERVER_SHUTDOWN

  However, `ADMIN_PACKET_SERVER_WELCOME` only after a `ADMIN_PACKET_SERVER_NEWGAME`


## 3.0) Asking for updates

  Asking for updates is done with `ADMIN_PACKET_ADMIN_UPDATE_FREQUENCY`.
  With this packet you define which update you wish to receive at which
  frequency.

  Note: not every update type supports every frequency. If in doubt, you can
  verify against the data received in `ADMIN_PACKET_SERVER_PROTOCOL`.

  Please note the potential gotcha in the "Certain packet information" section below
  when using the `ADMIN_UPDATE_FREQUENCY` packet.

  The server will not confirm your registered update. However, asking for an
  invalid `AdminUpdateType` or a not supported `AdminUpdateFrequency` you will be
  disconnected from the server with `NETWORK_ERROR_ILLEGAL_PACKET`.

  Additional debug information can be found with a debug level of `net=3`.

  `ADMIN_UPDATE_DATE` results in the server sending:

    - ADMIN_PACKET_SERVER_DATE

  `ADMIN_UPDATE_CLIENT_INFO` results in the server sending:

    - ADMIN_PACKET_SERVER_CLIENT_JOIN
    - ADMIN_PACKET_SERVER_CLIENT_INFO
    - ADMIN_PACKET_SERVER_CLIENT_UPDATE
    - ADMIN_PACKET_SERVER_CLIENT_QUIT
    - ADMIN_PACKET_SERVER_CLIENT_ERROR

  `ADMIN_UPDATE_COMPANY_INFO` results in the server sending:

    - ADMIN_PACKET_SERVER_COMPANY_NEW
    - ADMIN_PACKET_SERVER_COMPANY_INFO
    - ADMIN_PACKET_SERVER_COMPANY_UPDATE
    - ADMIN_PACKET_SERVER_COMPANY_REMOVE

  `ADMIN_UPDATE_COMPANY_ECONOMY` results in the server sending:

    - ADMIN_PACKET_SERVER_COMPANY_ECONOMY

  `ADMIN_UPDATE_COMPANY_STATS` results in the server sending:

    - ADMIN_PACKET_SERVER_COMPANY_STATS

  `ADMIN_UPDATE_CHAT` results in the server sending:

    - ADMIN_PACKET_SERVER_CHAT

  `ADMIN_UPDATE_CONSOLE` results in the server sending:

    - ADMIN_PACKET_SERVER_CONSOLE


  `ADMIN_UPDATE_CMD_LOGGING` results in the server sending:

    - ADMIN_PACKET_SERVER_CMD_LOGGING

## 3.1) Polling manually

  Certain `AdminUpdateTypes` can also be polled:

    - ADMIN_UPDATE_DATE
    - ADMIN_UPDATE_CLIENT_INFO
    - ADMIN_UPDATE_COMPANY_INFO
    - ADMIN_UPDATE_COMPANY_ECONOMY
    - ADMIN_UPDATE_COMPANY_STATS
    - ADMIN_UPDATE_CMD_NAMES

  Please note the potential gotcha in the "Certain packet information" section below
  when using the `ADMIN_POLL` packet.

  `ADMIN_UPDATE_CLIENT_INFO` and `ADMIN_UPDATE_COMPANY_INFO` accept an additional
  parameter. This parameter is used to specify a certain client or company.
  Setting this parameter to `UINT32_MAX (0xFFFFFFFF)` will tell the server you
  want to receive updates for all clients or companies.

  Not supported `AdminUpdateType` in the poll will result in the server
  disconnecting the application with `NETWORK_ERROR_ILLEGAL_PACKET`.

  Additional debug information can be found with a debug level of `net=3`.


## 4.0) Sending rcon commands

  Rcon runs separate from the `ADMIN_UPDATE_CONSOLE` `AdminUpdateType`. Requesting
  the execution of a remote console command is done with the packet
  `ADMIN_PACKET_ADMIN_RCON`.

  Note: No additional authentication is required for rcon commands.

  The server will reply with one or more `ADMIN_PACKET_SERVER_RCON` packets.
  Finally an `ADMIN_PACKET_ADMIN_RCON_END` packet will be sent. Applications
  will not receive the answer twice if they have asked for the `AdminUpdateType`
  `ADMIN_UPDATE_CONSOLE`, as the result is not printed on the servers console
  (just like clients rcon commands).

  Furthermore, sending a `say` command (or any similar command) will not
  be sent back into the admin network.
  Chat from the server itself will only be sent to the admin network when it
  was not sent from the admin network.

  Note that when content is queried or updated via rcon, the processing
  happens asynchronously. But the `ADMIN_PACKET_ADMIN_RCON_END` packet is sent
  already right after the content is requested as there's no immediate output.
  Thus other packages and the output of content rcon command may be sent at
  an arbitrary later time, mixing into the output of other console activity,
  e.g. also of possible subsequent other rcon commands sent.


## 5.0) Sending chat

  Sending a `ADMIN_PACKET_ADMIN_CHAT` results in chat originating from the server.

  Currently four types of chat are supported:

    - NETWORK_ACTION_CHAT
    - NETWORK_ACTION_CHAT_CLIENT
    - NETWORK_ACTION_CHAT_COMPANY
    - NETWORK_ACTION_SERVER_MESSAGE

  `NETWORK_ACTION_SERVER_MESSAGE` can be sent to a single client or company
  using the respective `DestType` and ID.
  This is a message prefixed with the 3 stars, e.g. `*** foo has joined the game`

## 5.1) Receiving chat

  Register `ADMIN_UPDATE_CHAT` at `ADMIN_FREQUENCY_AUTOMATIC` to receive chat.
  The application will be able to receive all chat the server can see.

  The configuration option `network.server_admin_chat` specifies whether
  private chat for to the server is distributed into the admin network.


## 6.0) Disconnecting

  It is a kind thing to say good bye before leaving. Do this by sending the
  `ADMIN_PACKET_ADMIN_QUIT` packet.


## 7.0) Certain packet information

  `ADMIN_PACKET_ADMIN_UPDATE_FREQUENCY` and `ADMIN_PACKET_ADMIN_POLL`

    Potential gotcha: the AdminUpdateType integer type used is a
    uint16 for `UPDATE_FREQUENCY`, and a uint8 for `POLL`.
    This is due to boring legacy reasons.
    It is safe to cast between the two when sending
    (i.e cast from a uint8 to a uint16).

  All `ADMIN_PACKET_SERVER_*` packets have an enum value greater 100.

  `ADMIN_PACKET_SERVER_WELCOME`

    Either directly follows `ADMIN_PACKET_SERVER_PROTOCOL` or is sent
    after a new game has been started or a map loaded, i.e. also
    after ADMIN_PACKET_SERVER_NEWGAME.

  `ADMIN_PACKET_SERVER_CLIENT_JOIN` and `ADMIN_PACKET_SERVER_COMPANY_NEW`

    These packets directly follow their respective INFO packets. If you receive
    a CLIENT_JOIN / COMPANY_NEW packet without having received the INFO packet
    it may be a good idea to POLL for the specific ID.

  `ADMIN_PACKET_SERVER_CMD_NAMES` and `ADMIN_PACKET_SERVER_CMD_LOGGING`

    Data provided with these packets is not stable and will not be
    treated as such. Do not rely on IDs or names to be constant
    across different versions / revisions of OpenTTD.
    Data provided in this packet is for logging purposes only.
