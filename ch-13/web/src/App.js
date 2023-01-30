import { useRef } from "react"
let path = __non_webpack_require__("node:path") // eslint-disable-line
let { CV } = __non_webpack_require__(path.join(path.dirname(process.execPath), "cv.node")) // eslint-disable-line
function App() {
  const canvasElement = useRef()
  const inputElement = useRef();
  function loadShader(gl, shaderSource, shaderType, opt_errorCallback) {
    const errFn = opt_errorCallback || console.error;
    const shader = gl.createShader(shaderType);
    gl.shaderSource(shader, shaderSource);
    gl.compileShader(shader);
    const compiled = gl.getShaderParameter(shader, gl.COMPILE_STATUS);
    if (!compiled) {
      const lastError = gl.getShaderInfoLog(shader);
      errFn('*** Error compiling shader \'' + shader + '\':' + lastError + `\n` + shaderSource.split('\n').map((l, i) => `${i + 1}: ${l}`).join('\n'));
      gl.deleteShader(shader);
      return null;
    }

    return shader;
  }
  let cv = new CV();
  const loadImage = async () => {
    let canvas = canvasElement.current
    let path = inputElement.current.value
    let img = await cv.imread(path)
    const vertexShaderSource = `
      // 顶点属性
      // 顶点的三维齐次坐标[x, y, z,]
      attribute vec4 a_position;
      attribute vec2 a_texcoord;
      varying vec2 v_texcoord;
      void main() {
        gl_Position = a_position;
        v_texcoord = a_texcoord;
    }`
    const fragmentShaderSource = `
      precision highp float;
      varying vec2 v_texcoord;
      uniform sampler2D u_texture;
      void main() {
        gl_FragColor = texture2D(u_texture, vec2(v_texcoord.x, 1.0 - v_texcoord.y));
      }
    `
    let gl = canvas.getContext("webgl");
    let vertexShader = loadShader(gl, vertexShaderSource, gl.VERTEX_SHADER)
    let fragmentShader = loadShader(gl, fragmentShaderSource, gl.FRAGMENT_SHADER)
    const program = gl.createProgram();
    gl.attachShader(program, vertexShader);
    gl.attachShader(program, fragmentShader);
    gl.linkProgram(program);
    gl.useProgram(program);
    var positionBuffer = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, positionBuffer);
    var positions = [
      -1, -1,
      -1, 1,
      1, -1,
      1, -1,
      -1, 1,
      1, 1,
    ];
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(positions), gl.STATIC_DRAW);
    var positionLocation = gl.getAttribLocation(program, "a_position");
    gl.vertexAttribPointer(positionLocation, 2, gl.FLOAT, false, 0, 0);
    gl.enableVertexAttribArray(positionLocation);

    var a_texcoordBuffer = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, a_texcoordBuffer);
    var coords = [
      0, 0,
      0, 1,
      1, 0,
      1, 0,
      0, 1,
      1, 1,
    ];
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(coords), gl.STATIC_DRAW);
    let a_texcoord = gl.getAttribLocation(program, "a_texcoord");
    gl.vertexAttribPointer(a_texcoord, 2, gl.FLOAT, false, 0, 0);
    gl.enableVertexAttribArray(a_texcoord);

    let tex = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, tex);
    if (img.type() === img.FORMAT_RGB) {
      gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGB, img.cols(), img.rows(), 0, gl.RGB, gl.UNSIGNED_BYTE, new Uint8Array(img.data));
    } else if (img.type() === img.FORMAT_GRAY) {
      gl.texImage2D(gl.TEXTURE_2D, 0, gl.LUMINANCE, img.cols(), img.rows(), 0, gl.LUMINANCE, gl.UNSIGNED_BYTE, new Uint8Array(img.data));
    } else if(img.type() === img.FORMAT_RGBA){
      gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, img.cols(), img.rows(), 0, gl.RGBA, gl.UNSIGNED_BYTE, new Uint8Array(img.data));
    }
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);  
    let textureLocation = gl.getUniformLocation(program, "u_texture");
    gl.uniform1i(textureLocation, 0);
    gl.drawArrays(gl.TRIANGLES, 0, 6);
    gl.deleteProgram(program)
    gl.deleteShader(vertexShader)
    gl.deleteShader(fragmentShader)
    gl.deleteBuffer(positionBuffer)
    gl.deleteBuffer(a_texcoordBuffer)
    gl.deleteTexture(tex)
  };
  let defaultImagePath = path.join(path.dirname(process.execPath), "xitu.png")
  return (
    <div className="App">
      <header className="App-header" style={{display:"flex", alignItems:"center", flexDirection:"column"}}>
        <canvas width="400" height="300" style={{ "width": "400px", height: "300px" }} ref={canvasElement}></canvas>
        <p>
          <input type="text" ref={inputElement} defaultValue={defaultImagePath} />
          <button onClick={loadImage}>加载图片</button>
        </p>
      </header>
    </div>
  );
}

export default App;
