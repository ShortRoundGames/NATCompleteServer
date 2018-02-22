global.__rootdir = __dirname;

//set the working directory to this path
process.chdir(__dirname);

var init = require('./init/init');
var redisObjects = rootRequire('./sr_lib/redisObjects');
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
                
        console.log("Starting Server");

        var app = server.startServer(
            null,
            null,
            health,
            nconf.get('server'));
        
        var redis = redisClientPool.getClient("proxy_reg");
        var heartbeat_set = new redisObjects.SortedSet("proxy_reg:heatbeat", redis, {marshal:true});        
        
        var url = ip.address()

        setInterval(
            function()
            {
                heartbeat_set.set(url, Date.now(), srlog.errorCallback);
            },
            5 * 1000);
    });


