'use strict';

const HELPER_BASE = process.env.HELPER_BASE || '../../helpers/';
const Response = require(HELPER_BASE + 'response');

const BEEBOTTE_CHANNEL = process.env.BEEBOTTE_CHANNEL || '【Beebotteのチャネル名】';
const BEEBOTTE_APIKEY = process.env.BEEBOTTE_APIKEY || '【BeebotteのAPIキー】';
const BEEBOTTE_SECRETKEY = process.env.BEEBOTTE_SECRETKEY || '【Beebotteのシークレットキー】';

const BASE_PATH = './public';
const folders = [
	{
		name: 'dashboard',
		path: '/dashboard/'
	}
];

const fs = require('fs');
const bbt = require('beebotte');
const bclient = new bbt.Connector({
  apiKey: BEEBOTTE_APIKEY,
  secretKey: BEEBOTTE_SECRETKEY,
});

exports.handler = async (event, context, callback) => {
	if( event.path == '/beebotte-auth' ){
		var params = event.queryStringParameters;
		if( params.channel != ('private-' + BEEBOTTE_CHANNEL) )
			throw 'channel mismatch';
		var to_sign = params.sid + ':' + params.channel + '.' + params.resource + ':ttl=' + params.ttl + ':read=true:write=false';
		var auth = bclient.sign( to_sign );

		return new Response(auth);
	}else
	if( event.path == '/beebotte-update'){
		var body = JSON.parse(event.body);
		console.log(body);

		var folder = folders.find(folder => folder.name == body.name);
		if( !folder )
			throw 'not found';

		fs.writeFileSync(BASE_PATH + folder.path + 'layout.json', JSON.stringify(body.data, null, "\t") );

		return new Response({ path : folder.path });
	}else
	if( event.path == '/beebotte-get'){
		var body = JSON.parse(event.body);
		console.log(body);

		var folder = folders.find(folder => folder.name == body.name);
		if( !folder )
			throw 'not found';

		var text = fs.readFileSync(BASE_PATH + folder.path + 'layout.json', { encoding: 'utf-8' })
		return new Response({ path : folder.path, data: JSON.parse(text) });
	}	
};
