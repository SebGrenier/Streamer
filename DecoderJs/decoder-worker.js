(function() {

var decoder;

self.Module = { memoryInitializerRequest: loadMemInitFile() }; // prefetch .mem file
importScripts('decoder.js');

Module.postRun.push(function() {
  // note that emscripten Module is not completely initialized until postRun.
  self.Decoder = Module.Decoder;
  self.CODEC = Module.CODEC;
  decoder = new Module.Decoder(Module.CODEC.H264);
  postMessage({ type: 'load' });
});

onmessage = function(ev) {
  var msg = ev.data;
  switch(msg.type) {
    case 'decode':
      decode();
      break;
    default:
      console.warn('unkown message type: ' + msg.type);
  }
}

function loadMemInitFile() {
  var req = new XMLHttpRequest();
  req.open('GET', 'decoder.js.mem');
  req.responseType = 'arraybuffer';
  req.send();
  return req;
}

function decode(data, size) {
  decoder.decode(data, size);
  if (decoder.hasFrame) {
    var uint8Frame = decoder.getFrame();
    postMessage({type: 'frame', data: uint8Frame}, [uint8Frame]);
  }
}

function onError(type, e) {
	var msg = {
		type: type,
		error: formatError(e)
	};
	postMessage(msg);
}

function formatError(e) {
	return { message: e.message, stack: e.stack };
}

})();

