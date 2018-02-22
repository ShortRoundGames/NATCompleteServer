var _ = require('underscore');
var nconf = require('nconf');

nconf.argv()
     .env()
     .file({ file:'./config/' + (process.env.NODE_ENV || 'development') + '.json'});


