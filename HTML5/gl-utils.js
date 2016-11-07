/*globals $V, $M*/
'use strict';

Matrix.Translation = function (v) {
    var r;
    if (v.elements.length == 2) {
        r = Matrix.I(3);
        r.elements[2][0] = v.elements[0];
        r.elements[2][1] = v.elements[1];
        return r;
    } else if (v.elements.length == 3) {
        r = Matrix.I(4);
        r.elements[0][3] = v.elements[0];
        r.elements[1][3] = v.elements[1];
        r.elements[2][3] = v.elements[2];
        return r;
    }

    throw "Invalid length for Translation";
};

Matrix.prototype.flatten = function () {
    var result = [];
    if (this.elements.length == 0)
        return [];


    for (var j = 0; j < this.elements[0].length; j++)
        for (var i = 0; i < this.elements.length; i++)
            result.push(this.elements[i][j]);
    return result;
};

Matrix.prototype.ensure4x4 = function () {
    if (this.elements.length == 4 &&
        this.elements[0].length == 4)
        return this;

    if (this.elements.length > 4 ||
        this.elements[0].length > 4)
        return null;

    for (var i = 0; i < this.elements.length; i++) {
        for (var j = this.elements[i].length; j < 4; j++) {
            if (i == j)
                this.elements[i].push(1);
            else
                this.elements[i].push(0);
        }
    }

    for (var i = this.elements.length; i < 4; i++) {
        if (i == 0)
            this.elements.push([1, 0, 0, 0]);
        else if (i == 1)
            this.elements.push([0, 1, 0, 0]);
        else if (i == 2)
            this.elements.push([0, 0, 1, 0]);
        else if (i == 3)
            this.elements.push([0, 0, 0, 1]);
    }

    return this;
};

Matrix.prototype.make3x3 = function () {
    if (this.elements.length != 4 ||
        this.elements[0].length != 4)
        return null;

    return Matrix.create([[this.elements[0][0], this.elements[0][1], this.elements[0][2]],
        [this.elements[1][0], this.elements[1][1], this.elements[1][2]],
        [this.elements[2][0], this.elements[2][1], this.elements[2][2]]]);
};

Vector.prototype.flatten = function () {
    return this.elements;
};

//
// gluLookAt
//
function makeLookAt(ex, ey, ez,
                    cx, cy, cz,
                    ux, uy, uz) {
    var eye = $V([ex, ey, ez]);
    var center = $V([cx, cy, cz]);
    var up = $V([ux, uy, uz]);

    var z = eye.subtract(center).toUnitVector();
    var x = up.cross(z).toUnitVector();
    var y = z.cross(x).toUnitVector();

    var m = $M([[x.e(1), x.e(2), x.e(3), 0],
        [y.e(1), y.e(2), y.e(3), 0],
        [z.e(1), z.e(2), z.e(3), 0],
        [0, 0, 0, 1]]);

    var t = $M([[1, 0, 0, -ex],
        [0, 1, 0, -ey],
        [0, 0, 1, -ez],
        [0, 0, 0, 1]]);
    return m.x(t);
}

//
// gluPerspective
//
function makePerspective(fovy, aspect, znear, zfar) {
    var ymax = znear * Math.tan(fovy * Math.PI / 360.0);
    var ymin = -ymax;
    var xmin = ymin * aspect;
    var xmax = ymax * aspect;

    return makeFrustum(xmin, xmax, ymin, ymax, znear, zfar);
}

//
// glFrustum
//
function makeFrustum(left, right,
                     bottom, top,
                     znear, zfar) {
    var X = 2 * znear / (right - left);
    var Y = 2 * znear / (top - bottom);
    var A = (right + left) / (right - left);
    var B = (top + bottom) / (top - bottom);
    var C = -(zfar + znear) / (zfar - znear);
    var D = -2 * zfar * znear / (zfar - znear);

    return $M([[X, 0, A, 0],
        [0, Y, B, 0],
        [0, 0, C, D],
        [0, 0, -1, 0]]);
}

//
// glOrtho
//
function makeOrtho(left, right, bottom, top, znear, zfar) {
    var tx = -(right + left) / (right - left);
    var ty = -(top + bottom) / (top - bottom);
    var tz = -(zfar + znear) / (zfar - znear);

    return $M([[2 / (right - left), 0, 0, tx],
        [0, 2 / (top - bottom), 0, ty],
        [0, 0, -2 / (zfar - znear), tz],
        [0, 0, 0, 1]]);
}

function getShader(gl, id) {
    var shaderScript, theSource, currentChild, shader;

    shaderScript = document.getElementById(id);

    if (!shaderScript) {
        return null;
    }

    theSource = "";
    currentChild = shaderScript.firstChild;

    while(currentChild) {
        if (currentChild.nodeType == currentChild.TEXT_NODE) {
            theSource += currentChild.textContent;
        }

        currentChild = currentChild.nextSibling;
    }

    if (shaderScript.type == "x-shader/x-fragment") {
        shader = gl.createShader(gl.FRAGMENT_SHADER);
    } else if (shaderScript.type == "x-shader/x-vertex") {
        shader = gl.createShader(gl.VERTEX_SHADER);
    } else {
        // type de shader inconnu
        return null;
    }

    gl.shaderSource(shader, theSource);

    // Compile le programme shader
    gl.compileShader(shader);

    // Vérifie si la compilation s'est bien déroulée
    if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) {
        alert("Une erreur est survenue au cours de la compilation des shaders: " + gl.getShaderInfoLog(shader));
        return null;
    }

    return shader;
}
