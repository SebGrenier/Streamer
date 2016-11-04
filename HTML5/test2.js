"use strict";

var _ws = new WebSocket("ws://" + "127.0.0.1" + ":" + "54321");
var _utf8Decoder = new TextDecoder("utf-8");

var video = document.querySelector('video');
var mimeCodec = 'video/mp4; codecs="avc1.4D401E"';

if ('MediaSource' in window && MediaSource.isTypeSupported(mimeCodec)) {
    var mediaSource = new MediaSource();
    //console.log(mediaSource.readyState); // closed
    video.src = URL.createObjectURL(mediaSource);
    mediaSource.addEventListener('sourceopen', sourceOpen);
} else {
    console.error('Unsupported MIME type or codec: ', mimeCodec);
}

var sourceBuffer = null;
var queue = [];
function sourceOpen (_) {
    //console.log(this.readyState); // open
    var mediaSource = this;
    sourceBuffer = mediaSource.addSourceBuffer(mimeCodec);
    sourceBuffer.mode = 'sequence';

    // sourceBuffer.addEventListener('update', function() { // Note: Have tried 'updateend'
    //     if (queue.length > 0 && !sourceBuffer.updating) {
    //         sourceBuffer.appendBuffer(queue.shift());
    //     }
    // });
    // video.addEventListener('canplay', function () {
    //     video.play();
    // });
    // video.addEventListener('timeupdate', function () {
    //     if (queue.length > 0 && !sourceBuffer.updating) {
    //         sourceBuffer.appendBuffer(queue.shift());
    //     }
    // });
    sourceBuffer.onupdate = function() { // Note: Have tried 'updateend'
        console.log('update');
        // if (queue.length > 0 && !sourceBuffer.updating) {
        //     sourceBuffer.appendBuffer(queue.shift());
        //     video.play();
        // }
    }
    sourceBuffer.onupdateend = function () {
        console.log('update end');
        if (queue.length > 0 && !sourceBuffer.updating) {
            sourceBuffer.appendBuffer(queue.shift());
        }
    }

    // fetchAB("frag_bunny.mp4", function (buf) {
    //     sourceBuffer.addEventListener('updateend', function (_) {
    //         mediaSource.endOfStream();
    //         video.play();
    //         //console.log(mediaSource.readyState); // ended
    //     });
    //     sourceBuffer.appendBuffer(buf);
    // });
};

function fetchAB (url, cb) {
    console.log(url);
    var xhr = new XMLHttpRequest;
    xhr.open('get', url);
    xhr.responseType = 'arraybuffer';
    xhr.onload = function () {
        cb(xhr.response);
    };
    xhr.send();
}

function onMessage(evt) {
    console.log("onMessage");

    var message = null;
    var binaryData = null;
    if (evt.data instanceof ArrayBuffer) {
        binaryData = new Uint8Array(evt.data);
        console.log("received packet of size " + binaryData.length);

        if (sourceBuffer) {
            if (sourceBuffer.updating || queue.length > 0) {
                queue.push(binaryData);
            } else {
                sourceBuffer.appendBuffer(evt.data);
            }
        }
    } else {
        message = JSON.parse(evt.data);
        console.log(JSON.stringify(evt.data));
    }
}

function onOpen() {
    console.log("onOpen");
    _ws.send("open");
}

function onClose() {
    console.log("onClose");
}

function onError() {
    console.log("onError");
}


_ws.binaryType = "arraybuffer";
_ws.onmessage = onMessage;
_ws.onopen = onOpen;
_ws.onclose = onClose;
_ws.onerror = onError;