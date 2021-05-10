# Multiplayer manual for OpenTTD

Last updated:    2011-02-16


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

 - Make sure that you have your firewall of the computer as well as possible
   routers or modems of the server configured such that:
    - port 3979 is free for both UDP and TCP connections in- and outgoing
    - port 3978 is free outbound for UDP in order to advertise with the master
     server (if desired). Otherwise you'll have to tell players your IP.
    - port 3977 if use of the admin interface is desired (see admin_network.txt)
 - Click "multiplayer" on the startup screen
 - Click "start server"
 - Type in a game name
 - Select the type of game ('LAN/Internet' or 'Internet (advertise)'. With the
   last one other people are able to see you online. Else they need your IP and
   port to join)
 - Click "start game", "load game" or "load scenario"
 - Start playing


## 2.0) Connecting to a server

 - Click "multiplayer" on the startup screen
 - If you want to connect to any network game in your LAN click on 'LAN', then
   on 'Find Server'
 - If you want to see which servers all online on the Internet, click on
   'Internet' and 'Find Server'
 - If there were more than one server
    - select one in the list below the buttons
    - click on 'join game'
 - If you want to play and you have the ip or hostname of the game server you
   want connect to.
    - click add server
    - type in the ip address or hostname
    - if you want to add a port use :<port>
 - Now you can select a company and press: "Join company", to help that company
    - Or you can press "Spectate game", to spectate the game
    - Or you can press "New company", and start your own company (if there are
   slots free)
 - You see a progressbar how far you are with joining the server.
 - Happy playing

## 2.1) Connecting to a server over the console

 - Open the console and type in the following command:
    connect `<ip/host>:<port>#<company-no>`


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
   - [network] frame_freq:
     change it in console with: 'set network.frame_freq <number>'
     the number should be between the 0 and 10, not much higher. It indicates
     the delay between clicking and showing up. The higher, the more you notice
     it, but the less bandwidth you use.
     A good value for Internet-games is 2 or 3.

   - [network] sync_freq:
     change it in console with: 'set network.sync_freq <number>'
     the number should be between the 50 and 1000, not much lower, not much
     higher. It indicates the time between sync-frames. A sync-frame is a frame
     which checks if all clients are still in sync. When the value it too high,
     clients can desync in 1960, but the server detects it in 1970. Not really
     handy. The lower the value, the more bandwidth it uses.

   NB: changing frame_freq has more effect on the bandwidth then sync_freq.


## 4.0) Tips for servers

 - You can launch a dedicated server by adding -D as parameter.
 - In UNIX like systems, you can fork your dedicated server by adding -f as
   parameter.

 - You can automatically clean companies that do not have a client connected to
   them, for, let's say, 3 years. You can do this via: 'set autoclean_companies'
   and 'set autoclean_protected' and 'set autoclean_unprotected'. Unprotected
   removes a password from a company when it is not used for more then the
   defined amount of months. 'set autoclean_novehicles' can be used to remove
   companies without any vehicles quickly.

 - You can also do this manually via the console: 'reset_company'.

 - You can let your server automatically restart a map when, let's say, year 2030
   is reached. See 'set restart_game_date' for detail.

 - If you want to be on the server-list, enable Advertising. To do this, select
   'Internet (advertise)' in the Start Server menu, or type in console:
   'set server_advertise 1'.

 - You can protect your server with a password via the console: 'set server_pw',
   or via the Start Server menu.

 - When you have many clients connected to your server via Internet, watch your
   bandwidth (if you have any limit on it, set by your ISP). One client uses
   about 1.5 kilobytes per second up and down. To decrease this amount, setting
   'frame_freq' to 1 will reduce it to roughly 1 kilobyte per second per client.

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
       #max_clients * #max_clients * bytes_per_frame * 10 KiB.

### 4.1) Imposing landscaping limits

 - You can impose limits on companies by the following 4 settings:
   - terraform_per_64k_frames
   - terraform_frame_burst
   - clear_per_64k_frames
   - clear_frame_burst

 - Explaining 'per_64K_frames' and 'burst'
    - 'burst' defines 3 things, the maximum limit, the limit of a single action,
      and the initial value for the limit assigned to a new company.
      This setting is fairly simple and requires no math.

      A value of 1 means a single tile can be affected by a single action.
      This results in having to click 400 times when wanting to cover an area
      of 20 x 20 tiles.

      The default value 4096 covers an area of 64 x 64 tiles.

    - 'per_64k_frames' defines the number of tiles added to each companies limit
      per frame (however not past the possible maximum value,the 'burst').
      64k rather resembles the exact number of 65536 frames. So setting this
      variable to 65536 means: 65536 / 65536 = 1 tile per frame.
      As a day consists of 74 frames, a company's limit is increased by 74
      tiles during the course of a single day (2.22 seconds).

      To achieve a 1 tile per day increase the following calculation is needed:
      1 / 74 (frames per day) * 65536 (per_64k_frames) = 885.62...
      after rounding: a value of 886 means adding a bit over 1 tile per day.

      There is still enough space to scale this value downwards:
      decreasing this value to 127 results in a bit over 1 tile added to the
      allowance per week (7 days).

      To create a setup in which a company gets an initial allowance only,
      set the value to 0 - no increase of the allowance per frame.

 - Even though construction actions include a clear tile action, they are not
   affected by the above settings.


## 5.0) Some useful things

 - You can protect your company so nobody else can join uninvited. To do this,
   set a password in your Company Screen

 - You can give other players some money via the ClientList (under the 'head'
   in the mainbar).

 - You can chat with other players via ENTER or via SHIFT+T or via the ClientList

 - Servers can now kick players, so don't make them use it!


## 6.0) Troubleshooting

 - My advertising server does not show up in list at servers.openttd.org
     Run openttd with the '-d net=2' parameter. That will show which incoming
     communication is received, whether the replies from the master server or
     communication from an admin tool reach the programme. See section 1
     'Starting a server' further up for the ports and protocols used by OpenTTD.
     The ports can be configured in the config file.

 - My advertising server warns a lot about getaddrinfo taking N seconds
     This could be a transient issue with your (local) DNS server, but if the
     problem persists there is likely a configuration issue in DNS resolving
     on your computer. This seems to be a common configuration issue for
     Docker instances, where the DNS resolving waits for a time out of usually
     5 seconds.
