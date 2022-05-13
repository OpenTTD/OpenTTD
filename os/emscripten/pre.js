Module.arguments.push('-mnull', '-snull', '-vsdl:relative_mode');
Module['websocket'] = { url: function(host, port, proto) {
    /* openttd.org hosts a WebSocket proxy for the content service. */
    if (host == "content.openttd.org" && port == 3978 && proto == "tcp") {
        return "wss://content.openttd.org/";
    }

    /* Everything else just tries to make a default WebSocket connection.
     * If you run your own server you can setup your own WebSocket proxy in
     * front of it and let people connect to your server via the proxy. You
     * are best to add another "if" statement as above for this. */

    if (location.protocol === 'https:') {
        /* Insecure WebSockets do not work over HTTPS, so we force
         * secure ones. */
        return 'wss://';
    } else {
        /* Use the default provided by Emscripten. */
        return null;
    }
} };

Module.preRun.push(function() {
    personal_dir = '/home/web_user/.openttd';
    content_download_dir = personal_dir + '/content_download'

    /* Because of the "-c" above, all user-data is stored in /user_data. */
    FS.mkdir(personal_dir);
    FS.mount(IDBFS, {}, personal_dir);

    Module.addRunDependency('syncfs');
    FS.syncfs(true, function (err) {
        /* FS.mkdir() tends to fail if parent folders do not exist. */
        if (!FS.analyzePath(content_download_dir).exists) {
            FS.mkdir(content_download_dir);
        }
        if (!FS.analyzePath(content_download_dir + '/baseset').exists) {
            FS.mkdir(content_download_dir + '/baseset');
        }

        /* Check if the OpenGFX baseset is already downloaded. */
        if (!FS.analyzePath(content_download_dir + '/baseset/opengfx-0.6.0.tar').exists) {
            window.openttd_downloaded_opengfx = true;
            FS.createPreloadedFile(content_download_dir + '/baseset', 'opengfx-0.6.0.tar', 'https://installer.cdn.openttd.org/emscripten/opengfx-0.6.0.tar', true, true);
        } else {
            /* Fake dependency increase, so the counter is stable. */
            Module.addRunDependency('opengfx');
            Module.removeRunDependency('opengfx');
        }

        Module.removeRunDependency('syncfs');
    });

    window.openttd_syncfs_shown_warning = false;
    window.openttd_syncfs = function() {
        /* Copy the virtual FS to the persistent storage. */
        FS.syncfs(false, function (err) { });

        /* On first time, warn the user about the volatile behaviour of
         * persistent storage. */
        if (!window.openttd_syncfs_shown_warning) {
            window.openttd_syncfs_shown_warning = true;
            Module.onWarningFs();
        }
    }

    window.openttd_exit = function() {
        Module.onExit();
    }

    window.openttd_abort = function() {
        Module.onAbort();
    }

    window.openttd_server_list = function() {
        add_server = Module.cwrap("em_openttd_add_server", null, ["string"]);

        /* Add servers that support WebSocket here. Examples:
         *  add_server("localhost");
         *  add_server("localhost:3979");
         *  add_server("127.0.0.1:3979");
         *  add_server("[::1]:3979");
         */
    }

    var leftButtonDown = false;
    document.addEventListener("mousedown", e => {
        if (e.button == 0) {
            leftButtonDown = true;
        }
    });
    document.addEventListener("mouseup", e => {
        if (e.button == 0) {
            leftButtonDown = false;
        }
    });
    window.openttd_open_url = function(url, url_len) {
        const url_string = UTF8ToString(url, url_len);
        function openWindow() {
            document.removeEventListener("mouseup", openWindow);
            window.open(url_string, '_blank');
        }
        /* Trying to open the URL while the mouse is down results in the button getting stuck, so wait for the
         * mouse to be released before opening it. However, when OpenTTD is lagging, the mouse can get released
         * before the button click even registers, so check for that, and open the URL immediately if that's the
         * case. */
        if (leftButtonDown) {
            document.addEventListener("mouseup", openWindow);
        } else {
            openWindow();
        }
    }

    /* https://github.com/emscripten-core/emscripten/pull/12995 implements this
    * properly. Till that time, we use a polyfill. */
   SOCKFS.websocket_sock_ops.createPeer_ = SOCKFS.websocket_sock_ops.createPeer;
   SOCKFS.websocket_sock_ops.createPeer = function(sock, addr, port)
   {
       let func = Module['websocket']['url'];
       Module['websocket']['url'] = func(addr, port, (sock.type == 2) ? 'udp' : 'tcp');
       let ret = SOCKFS.websocket_sock_ops.createPeer_(sock, addr, port);
       Module['websocket']['url'] = func;
       return ret;
   }
});

Module.postRun.push(function() {
    /* Check if we downloaded OpenGFX; if so, sync the virtual FS back to the
     * IDBFS so OpenGFX is stored persistent. */
    if (window['openttd_downloaded_opengfx']) {
        FS.syncfs(false, function (err) { });
    }
});
