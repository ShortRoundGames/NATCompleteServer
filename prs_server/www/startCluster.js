var root_dir = __dirname;
global.__rootdir = root_dir;

require('./init/init');

var cluster = require('./sr_lib/server/cluster');

if (process.platform != 'win32')
    root_dir = '/mnt/prs/current/';

cluster.startCluster(root_dir, 'prs');