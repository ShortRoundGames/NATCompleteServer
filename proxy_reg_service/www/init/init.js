// This is a bootstrap file that is used solely for its side effects.
process.env.NODE_ENV = process.env.NODE_ENV || 'development';

global.is_development = process.env.NODE_ENV == 'development';
global.is_production = !global.is_development;

var OS = require('os');
global.host_name = OS.hostname() || '';

global.is_staging = global.host_name.indexOf('staging') >= 0;
console.log(global.host_name + " is staging=" + global.is_staging);

require('../sr_lib/core/rootRequire');
require('./initNconf');

var nconf = require('nconf');

//----Global Utils----
global._ = require("underscore");
global.async = require("async");

//----Math----
var srMath = rootRequire('sr_lib/math/srMath');
srMath.extendMath();

//----Async----
var srAsync = rootRequire('sr_lib/core/srAsync');
srAsync.extendAsync();

//----Logging----
global.srlog = rootRequire('sr_lib/log/srlog');
srlog.init(nconf.get('srlog'));

//When running locally we want the server to crash if we hit an error
if (process.env.NODE_ENV == 'development' && global.host_name != 'indy')
    srlog.setOption('exit_on_error', true);

//Bit hacky but lets us manage logs from external modules better
console.error = srlog.error;

exports.init = function(callback)
{
    async.series(
        [
            function(next)
            {
                //----Init Redis----                
                var redisClientPool = rootRequire('sr_lib/redis/redisClientPool');
                var redis_config = nconf.get('redis');
                redisClientPool.init(redis_config.pools, null, redis_config.database_index, next);
            }
        ],
        callback);
}

