global.__rootdir = __dirname;

//set the working directory to this path
process.chdir(__dirname);

var init = require('./init/init');
var redisObjects = rootRequire('./sr_lib/redisObjects');
var redisLock = rootRequire('./sr_lib/redis/redisLock');
var ip = require('ip');

async.series(
    [
        init.init.bind(init)
    ],
    function(err)
    {
        if (err)
            return srlog.error(err);

        var nconf = require('nconf');
        var server = rootRequire('sr_lib/server/server');
        var redisClientPool = rootRequire('sr_lib/redis/redisClientPool');        
        var url = require('url');

        function health(callback)
        {
            async.series(
                [
                    //Check each redis server
                    redisClientPool.testConnections.bind(redisClientPool)
                ],
                function(err)
                {
                    if (err)
                    {
                        if (err.toString)
                            err = err.toString();

                        srlog.error("FAILED HEALTH", err);
                    }

                    callback(err);
                });
        }

        var redis = redisClientPool.getClient("proxy_reg");
        var id_hash = new redisObjects.Hash("proxy_reg:ids", redis, { marshal: true });

        var url = ip.address();
        var url_id = -1;

        async.series(
            [
                //Check if we already have an address
                function(next)
                {                    
                    id_hash.get(url,
                        function(err, id)
                        {
                            if (err)
                                return next(err);

                            if (id)
                            {
                                url_id = id;
                                return next("SUCCESS");
                            }

                            next();
                        });
                },
                //We need to create an id for this url
                function(next)
                {
                    redisLock.lockedFunction(redis, "proxy_reg:idlock", 5, 120,
                        function(next)
                        {
                            id_hash.length(
                                function(err, num)
                                {
                                    if (err)
                                        return next(err);

                                    url_id = num+1;
                                    id_hash.set(url, url_id, next);
                                });
                        },
                        next);
                }
            ],
            function(err)
            {
                if (err && err != "SUCCESS")
                {
                    srlog.error("Failed to find/create id");
                    exit(1);
                    return;
                }

                console.log("Starting Server");

                var app = server.startServer(
                    null,
                    null,
                    health,
                    nconf.get('server'));


                var heartbeat_set = new redisObjects.SortedSet("proxy_reg:heatbeat", redis, { marshal: true });
                var data = url_id + '#' + url;

                heartbeat_set.set(data, Date.now(), srlog.errorCallback);

                setInterval(
                    function()
                    {
                        heartbeat_set.set(data, Date.now(), srlog.errorCallback);
                    },
                    60 * 1000);

            });
    });


