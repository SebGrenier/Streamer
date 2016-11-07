varying highp vec2 vTextureCoord;
uniform sampler2D uSampler;
uniform bool uSwizzle;

void main(void) {
    if (uSwizzle) {
        gl_FragColor = texture2D(uSampler, vec2(vTextureCoord.s, vTextureCoord.t)).bgra;
    } else {
        gl_FragColor = texture2D(uSampler, vec2(vTextureCoord.s, vTextureCoord.t));
    }

}