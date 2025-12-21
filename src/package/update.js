if (typeof glMatrix !== 'undefined') {
    window.vec2 = glMatrix.vec2;
    window.vec3 = glMatrix.vec3;
    window.vec4 = glMatrix.vec4;
    window.mat2 = glMatrix.mat2;
    window.mat3 = glMatrix.mat3;
    window.mat4 = glMatrix.mat4;
    window.quat = glMatrix.quat;
} else {
    console.error('glMatrix is not loaded')
}