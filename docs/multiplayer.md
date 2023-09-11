# Multiplayer manual for OpenTTD

## Table of contents

- 1.0) [Starting a server](#10-starting-a-server)
- 2.0) [Connecting to a server](#20-connecting-to-a-server)
  - 2.1) [Connecting to a server over the console](#21-connecting-to-a-server-over-the-console)
- 3.0) [Playing internet games](#30-playing-internet-games)
- 4.0) [Tips for servers](#40-tips-for-servers)
  - 4.1)[Imposing landscaping limits](#41-imposing-landscaping-limits)
- 5.0) [Some useful things](#50-some-useful-things)
- 6.0) [Troubleshooting](#60-troubleshooting)


## 1.0) Starting a server

- Click on "Multiplayer" in the Start Menu.
- Click on "Start Server".
- Give your server a name.
- Select the visibility of your server:
  - "Public": your server will be publicly listed.
  - "Invite Only": only players who have the invite code for your server can
    join.
  - "Local": only players on your local network can join.
- (optional) Set a password for your server.
- Click "New Game", "Load Game", or "Play Scenario".
- Start playing.

## 2.0) Connecting to a server

- Click on "Multiplayer" in the Start Menu.
- There are three ways to join a server:
  - If you want to connect to a local server, click "Search LAN".
  - If you want to connect to a public game, click "Search internet".
  - If the server-owner shared an invite code with you:
    - Click "Add Server".
    - Fill in the invite code, which always starts with a `+`.
    - Click "OK".
- Click on the server you want to join.
- Click "Join Game".
- If the server has a password, it will ask you for this.
- You see a progressbar how far you are with joining the server.
- Happy playing.

## 2.1) Connecting to a server over the console

- Open the console and type `connect` for help how to connect via the console.

## 3.0) Playing internet games

- Servers with a red dot behind it have a different version then you have. You
  will not be able to join those servers.

- Servers with a yellow dot behind it have NewGRFs that you do not have. You
  will not be able to join those servers. However, via "NewGRF Settings" and
  "Find missing content online" you might be able to download the needed
  NewGRFs after which you can join the server.

- It can happen that a connection is that slow, or you have that many clients
  connected to your server, that your clients start to loose their connection.
  Some things you can do about it:
  - `[network] frame_freq`:
    change it in console with: `set network.frame_freq <number>`
    the number should be between the 0 and 10, not much higher. It indicates
    the delay between clicking and showing up. The higher, the more you notice
    it, but the less bandwidth you use.
    A good value for Internet-games is 2 or 3.

  - `[network] sync_freq`:
    change it in console with: `set network.sync_freq <number>`
    the number should be between the 50 and 1000, not much lower, not much
    higher. It indicates the time between sync-frames. A sync-frame is a frame
    which checks if all clients are still in sync. When the value it too high,
    clients can desync in 1960, but the server detects it in 1970. Not really
    handy. The lower the value, the more bandwidth it uses.

   NB: changing `frame_freq` has more effect on the bandwidth then `sync_freq`.

## 4.0) Tips for servers

- You can launch a dedicated server by adding `-D` as parameter.
- In UNIX like systems, you can fork your dedicated server by adding `-f` as
  parameter.

- You can automatically clean companies that do not have a client connected to
  them, for, let's say, 3 years. You can do this via: `set autoclean_companies`
  and `set autoclean_protected` and `set autoclean_unprotected`. Unprotected
  removes a password from a company when it is not used for more then the
  defined amount of months. `set autoclean_novehicles` can be used to remove
  companies without any vehicles quickly.

- You can also do this manually via the console: `reset_company`.

- You can let your server automatically restart a map when, let's say,
  year 2030 is reached. See `set restart_game_date` for detail.

- If you want to be on the server-list, make your server public. You can do
  this either from the Start Server window, via the in-game Online Players
  window, or by typing in the console: `set server_game_type public`.

- You can protect your server with a password via the console: `set server_pw`,
  or via the Start Server menu.

- When you have many clients connected to your server via Internet, watch your
  bandwidth (if you have any limit on it, set by your ISP). One client uses
  about 1.5 kilobytes per second up and down. To decrease this amount, setting
  `frame_freq` to 1 will reduce it to roughly 1 kilobyte per second per client.

- OpenTTD's default settings for maximum number of clients, and amount of data
  from clients to process are chosen to not influence the normal playing of
  people, but to prevent or at least make it less likely that someone can
  perform a (distributed) denial-of-service attack on your server by causing
  an out-of-memory event by flooding the server with data to send to all
  clients. The major factor in this is the maximum number of clients; with
  32 clients "only" sending one chat message causes 1024 messages to be
  distributed in total, with 64 clients that already quadruples to 4096. Given
  that upstream bandwidth is usually the limiting factor, a queue of packets
  that need to be sent will be created.
  To prevent clients from exploiting this "explosion" of packets to send we
  limit the number of incoming data, resulting in effectively limiting the
  amount of data that OpenTTD will send to the clients. Even with the default
  limits it is possible to generate about 70.000 packets per second, or about
  7 megabit per second of traffic.
  Given that OpenTTD kicks clients after they have not reacted within about 9
  seconds from sending a frame update packet it would be possible that OpenTTD
  keeps about 600.000 packets in memory, using about 50 megabytes of memory.
  Given that OpenTTD allows short bursts of packets, you can have slightly
  more packets in memory in case of a distributed denial of service attack.
  When increasing the amount of incoming data, or the maximum number of
  clients the amount of memory OpenTTD needs in case of a distributed denial
  of service attack is linearly related to the amount of incoming data and
  quadratic to the amount of clients. In short, a rule of thumb for, the
  maximum memory usage for packets is:
  `#max_clients * #max_clients * bytes_per_frame * 10 KiB`.

### 4.1) Imposing landscaping limits

- You can impose limits on companies by the following 4 settings:
  - `terraform_per_64k_frames`
  - `terraform_frame_burst`
  - `clear_per_64k_frames`
  - `clear_frame_burst`

- Explaining `NNN_burst` and `NNN_per_64K_frames`
  - `NNN_burst` defines 3 things, the maximum limit, the limit of a single
    action, and the initial value for the limit assigned to a new company.
    This setting is fairly simple and requires no math.

    A value of 1 means a single tile can be affected by a single action.
    This results in having to click 400 times when wanting to cover an area
    of 20 x 20 tiles.

    The default value 4096 covers an area of 64 x 64 tiles.

  - `NNN_per_64K_frames` defines the number of tiles added to each companies
    limit per frame (however not past the possible maximum value,the
    `NNN_burst`). 64k rather resembles the exact number of 65536 frames. So
    setting this variable to 65536 means: `65536 / 65536 = 1 tile per frame`.

    As a day consists of 74 frames, a company's limit is increased by 74
    tiles during the course of a single day (2.22 seconds).
    To achieve a 1 tile per day increase the following calculation is needed:
    `1 / 74 (frames per day) * 65536 (per_64k_frames) = 885.62...`.
    After rounding: a value of 886 means adding a bit over 1 tile per day.

    There is still enough space to scale this value downwards:
    decreasing this value to 127 results in a bit over 1 tile added to the
    allowance per week (7 days).

    To create a setup in which a company gets an initial allowance only,
    set the value to 0 - no increase of the allowance per frame.

- Even though construction actions include a clear tile action, they are not
  affected by the above settings.

## 5.0) Some useful things

- You can protect your company so nobody else can join uninvited. To do this,
  set a password in your Company window.

- You can chat with other players via ENTER or via SHIFT+T or via the Online
  Players window

- Servers can kick players, so don't make them use it!

## 6.0) Troubleshooting

### My server does not show up in the serverlist

Check if the visibility of your server is set to `public`.

If it is, and your server still isn't showing up, start OpenTTD with
`-d net=4` as extra argument. This will show debug message related to the
network, including communication to/from the Game Coordinator.

See the [Game Coordinator documentation](./game_coordinator.md) for more
technical information about the Game Coordinator service.

### My server warns a lot about getaddrinfo taking N seconds

This could be a transient issue with your (local) DNS server, but if the
problem persists there is likely a configuration issue in DNS resolving on
your computer.

#### Running OpenTTD in a Docker container?

This is an issue with dual-stack Docker containers. If there is no default
IPv6 resolver and IPv6 traffic is preferred, DNS requests will time out after
5 seconds. To resolve this, use an IPv4 DNS server for your Docker container,
for example by adding `--dns 1.1.1.1` to your `docker run` command.
