"use strict";

var _ws = new WebSocket("ws://" + "127.0.0.1" + ":" + "54321");
var _utf8Decoder = new TextDecoder("utf-8");


// WebGL
// var canvas = document.querySelector('canvas');
// var gl = null;
// var _width = 1280, _height = 720;
// var _viewportTextureId = null;
// var _uSamplerLocation = null;
// var _uSwizzleLocation = null;
// var _shaderProgram = null;
// var _vertexPositionAttribute = null;
// var _textureCoordAttribute = null;
// var _squareVerticesBuffer = null;
// var _squareVerticesTextureCoordBuffer = null;
//
// function _initShaders() {
//     var shaders = [];
//     shaders.push(getShader(gl, "shader-fs"));
//     shaders.push(getShader(gl, "shader-vs"));
//     var fragmentShader = shaders[0];
//     var vertexShader = shaders[1];
//     var shaderProgram = gl.createProgram();
//     gl.attachShader(shaderProgram, vertexShader);
//     gl.attachShader(shaderProgram, fragmentShader);
//     gl.linkProgram(shaderProgram);
//
//     if (!gl.getProgramParameter(shaderProgram, gl.LINK_STATUS)) {
//         throw new Error("Can't link shader");
//     }
//
//     gl.useProgram(shaderProgram);
//
//     _vertexPositionAttribute = gl.getAttribLocation(shaderProgram, "aVertexPosition");
//     gl.enableVertexAttribArray(_vertexPositionAttribute);
//
//     _textureCoordAttribute = gl.getAttribLocation(shaderProgram, "aTextureCoord");
//     gl.enableVertexAttribArray(_textureCoordAttribute);
//
//     _shaderProgram = shaderProgram;
// }
//
// function _initBuffers() {
//     _squareVerticesBuffer = gl.createBuffer();
//     gl.bindBuffer(gl.ARRAY_BUFFER, _squareVerticesBuffer);
//
//     var vertices = [
//         1.0,  1.0,  0.0,
//         -1.0, 1.0,  0.0,
//         1.0,  -1.0, 0.0,
//         -1.0, -1.0, 0.0
//     ];
//
//     gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(vertices), gl.STATIC_DRAW);
//
//     _squareVerticesTextureCoordBuffer = gl.createBuffer();
//     gl.bindBuffer(gl.ARRAY_BUFFER, _squareVerticesTextureCoordBuffer);
//
//     var texCoord = [
//         1.0, 0.0,
//         0.0, 0.0,
//         1.0, 1.0,
//         0.0, 1.0
//     ];
//
//     gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(texCoord), gl.STATIC_DRAW);
// }
//
// function _initWebGL() {
//     gl = canvas.getContext("webgl");
//
//     gl.clearColor(0.152, 0.156, 0.160, 1.0);
//     gl.enable(gl.DEPTH_TEST);
//     gl.depthFunc(gl.LEQUAL);
//     gl.clear(gl.COLOR_BUFFER_BIT|gl.DEPTH_BUFFER_BIT);
//
//     _viewportTextureId = gl.createTexture();
//
//     gl.bindTexture(gl.TEXTURE_2D, _viewportTextureId);
//     gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
//     gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
//     gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
//     gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
// }
//
// function _initScene() {
//     var perspectiveMatrix = makeOrtho(-1, 1, -1, 1, 0.0, 1.0);
//     var mvMatrix = Matrix.I(4);
//
//     var pUniform = gl.getUniformLocation(_shaderProgram, "uPMatrix");
//     var mvUniform = gl.getUniformLocation(_shaderProgram, "uMVMatrix");
//
//     gl.uniformMatrix4fv(pUniform, false, new Float32Array(perspectiveMatrix.flatten()));
//     gl.uniformMatrix4fv(mvUniform, false, new Float32Array(mvMatrix.flatten()));
//
//     _uSamplerLocation = gl.getUniformLocation(_shaderProgram, "uSampler");
//     _uSwizzleLocation = gl.getUniformLocation(_shaderProgram, "uSwizzle");
// }

/**
 * Draw the engine viewport to texture.
 * @private
 * @param {object|Image} image - Image data to be used as the viewport render.
 */
// function _writeFrame(image) {
//     if (_width !== image.width || _height !== image.height) {
//         canvas.width = _width = image.width;
//         canvas.height = _height = image.height;
//     }
//     gl.bindTexture(gl.TEXTURE_2D, _viewportTextureId);
//     gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, image.width, image.height, 0, gl.RGBA, gl.UNSIGNED_BYTE, image.blob);
//
//     requestAnimationFrame(_renderViewport);
// }
//
// function _renderViewport() {
//     gl.activeTexture(gl.TEXTURE0);
//     gl.bindTexture(gl.TEXTURE_2D, _viewportTextureId);
//     gl.uniform1i(_uSamplerLocation, 0);
//     gl.uniform1i(_uSwizzleLocation, 0);
//
//     gl.bindBuffer(gl.ARRAY_BUFFER, _squareVerticesBuffer);
//     gl.vertexAttribPointer(_vertexPositionAttribute, 3, gl.FLOAT, false, 0, 0);
//
//     gl.bindBuffer(gl.ARRAY_BUFFER, _squareVerticesTextureCoordBuffer);
//     gl.vertexAttribPointer(_textureCoordAttribute, 2, gl.FLOAT, false, 0, 0);
//
//     gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);
// }

// _initWebGL();
// _initBuffers();
// _initShaders();
// _initScene();

// Broadway
// var avc = new Decoder({rgb: true});
// avc.onPictureDecoded = function (buffer, width, height) {
//     _writeFrame({blob: buffer, width: width, height: height});
// };

// look up the elements we want to affect
var bwElement = document.getElementById("bandwidth");
var dpsElement = document.getElementById("decode");

// Create text nodes to save some time for the browser.
var bwNode = document.createTextNode("");
var dpsNode = document.createTextNode("");

// Add those text nodes where they need to go
bwElement.appendChild(bwNode);
dpsElement.appendChild(dpsNode);

var lastFrameTime = null;
var startTime = null;
var timesToDecode = [];
var bufferLengths = [];
function H264Player(){
    console.log('using', this);
    var p = new Player({
        useWorker: true,
        workerFile: "Decoder.js",
    });

    p.onPictureDecoded = function (buffer, width, height, infos) {
        var now = Date.now();
        if (startTime) {
            timesToDecode.push(now - startTime);
            if (timesToDecode.length > 10) {
                timesToDecode.shift();
            }
            var total = 0;
            for (var time of timesToDecode) {
                total += time;
            }
            total /= timesToDecode.length;
            dpsNode.nodeValue = total.toFixed(0);
        }
        startTime = null;
    }

    document.body.appendChild(p.canvas);
    var parser = new nalParser(p);
    this.play = function(buffer){
        if (!startTime)
            startTime = Date.now();
        parser.parse(buffer);
    };
}

setInterval(function () {
    var total = 0;
    for (var size of bufferLengths) {
        total += size;
    }
    bufferLengths.length = 0;
    bwNode.nodeValue = (total / 1024).toFixed(0);
}, 1000);

var h264p = new H264Player();

function nalParser(player){
    var bufferAr = [];
    var concatUint8 = function(parAr) {
        if (!parAr || !parAr.length){
            return new Uint8Array(0);
        };

        if (parAr.length === 1){
            return parAr[0];
        };

        var completeLength = 0;
        var i = 0;
        var l = parAr.length;
        for (i; i < l; ++i){
            completeLength += parAr[i].byteLength;
        };

        var res = new Uint8Array(completeLength);
        var filledLength = 0;

        for (i = 0; i < l; ++i){
            res.set(new Uint8Array(parAr[i]), filledLength);
            filledLength += parAr[i].byteLength;
        };
        return res;
    };
    this.parse = function(buffer){
        if (!(buffer && buffer.byteLength)){
            return;
        };
        var data = new Uint8Array(buffer);
        var hit = function(subarray){
            if (subarray){
                bufferAr.push(subarray);
            };
            var buff = concatUint8(bufferAr);
            player.decode(buff);
            bufferAr = [];
        };

        var b = 0;
        var lastStart = 0;

        var l = data.length;
        var zeroCnt = 0;

        for (b = 0; b < l; ++b){
            if (data[b] === 0){
                zeroCnt++;
            }else{
                if (data[b] == 1){
                    if (zeroCnt >= 3){
                        if (lastStart < b - 3){
                            hit(data.subarray(lastStart, b - 3));
                            lastStart = b - 3;
                        }else if (bufferAr.length){
                            hit();
                        }
                    };
                };
                zeroCnt = 0;
            };
        };
        if (lastStart < data.length){
            bufferAr.push(data.subarray(lastStart));
        };
    };
}

function onMessage(evt) {
    var message = null;
    var binaryData = null;
    if (evt.data instanceof ArrayBuffer) {
        binaryData = new Uint8Array(evt.data);
        bufferLengths.push(binaryData.length);
        //avc.decode(binaryData);
        h264p.play(binaryData);
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