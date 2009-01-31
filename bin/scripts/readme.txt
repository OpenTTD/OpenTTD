Scripting
---------

OpenTTD supports scripts.

local scripts:
 - 'autoexec.scr' is executed on gamestart [all - use this for custom aliases per ex.]

+network scripts:
   should be used to set client optimization settings:
 - 'on_client.scr' is executed when you join a server [all clients]

 - 'on_server_connect.scr' is executed on the server when a client has joined (MOTD)

   should be used to set the servers port/ip and/or server optimization settings/patches:
 - 'pre_server.scr' is executed before the servers tcp stack is started [in-game only]
 - 'pre_dedicated.scr' is executed before the servers tcp stack is started [dedicated only]

   should be used to set the servers name, password and so on:
 - 'on_server.scr' is executed after starting a server [dedicated and in-game]
 - 'on_dedicated.scr' is additionally executed after starting a server [dedicated only]

For examples how a script can look, check the .example examples.
