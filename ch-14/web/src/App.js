import React from 'react';
let path = __non_webpack_require__("node:path") // eslint-disable-line
let { AVDecoder } = __non_webpack_require__(path.join(path.dirname(process.execPath), "cv.node")) // eslint-disable-line

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
class AVDecoderComponent extends React.Component {
  constructor(props) {
    super(props)
    this.canvasRef = React.createRef();
    this.inputElement = React.createRef()
    this.decoder = new AVDecoder()
  }
  componentDidMount() {
    let canvas = this.canvasRef.current
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
    this.vertexShader = loadShader(gl, vertexShaderSource, gl.VERTEX_SHADER)
    this.fragmentShader = loadShader(gl, fragmentShaderSource, gl.FRAGMENT_SHADER)
    this.program = gl.createProgram();
    gl.attachShader(this.program, this.vertexShader);
    gl.attachShader(this.program, this.fragmentShader);
    gl.linkProgram(this.program);
    gl.useProgram(this.program);
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
    var positionLocation = gl.getAttribLocation(this.program, "a_position");
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
    let a_texcoord = gl.getAttribLocation(this.program, "a_texcoord");
    gl.vertexAttribPointer(a_texcoord, 2, gl.FLOAT, false, 0, 0);
    gl.enableVertexAttribArray(a_texcoord);
    gl.drawArrays(gl.TRIANGLES, 0, 6);
    this.decoder.on("newFrame(CvMat)", (img)=>{
      let canvas = this.canvasRef.current
      let gl = canvas.getContext("webgl");    
      let tex = gl.createTexture();
      gl.bindTexture(gl.TEXTURE_2D, tex);
      img.lock();
      if (img.type() === img.FORMAT_RGB) {
        gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGB, img.cols(), img.rows(), 0, gl.RGB, gl.UNSIGNED_BYTE, new Uint8Array(img.data));
      } else if (img.type() === img.FORMAT_GRAY) {
        gl.texImage2D(gl.TEXTURE_2D, 0, gl.LUMINANCE, img.cols(), img.rows(), 0, gl.LUMINANCE, gl.UNSIGNED_BYTE, new Uint8Array(img.data));
      } else if(img.type() === img.FORMAT_RGBA){
        gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, img.cols(), img.rows(), 0, gl.RGBA, gl.UNSIGNED_BYTE, new Uint8Array(img.data));
      }
      img.unlock();
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);  
      let textureLocation = gl.getUniformLocation(this.program, "u_texture");
      gl.uniform1i(textureLocation, 0);
      gl.drawArrays(gl.TRIANGLES, 0, 6);
      gl.deleteTexture(tex)
    })    
  }
  componentWillUnmount() {
    let canvas = this.canvasRef.current
    let gl = canvas.getContext("webgl");
    gl.deleteProgram(this.program)
    gl.deleteShader(this.vertexShader)
    gl.deleteShader(this.fragmentShader)
    gl.deleteBuffer(this.positionBuffer)
    gl.deleteBuffer(this.a_texcoordBuffer)
    this.decoder.on("newFrame(CvMat)", null)
  }
  render() {
    let defaultVideoPath = path.join(path.dirname(process.execPath), "video.mp4")
    const loadVideo = ()=>{
      let path = this.inputElement.current.value
      this.decoder.open(path)
    }
    return (
      <>
        <canvas ref={this.canvasRef} width="400" height="300" style={{ "width": "400px", height: "300px" }}></canvas>
        <p>
          <input type="text" ref={this.inputElement} defaultValue={defaultVideoPath} />
          <button onClick={loadVideo}>加载视频</button>
        </p>
      </>
    );
  }
}
function App() {
  return (
    <div className="App">
      <header className="App-header" style={{ display: "flex", alignItems: "center", flexDirection: "column" }}>
        <AVDecoderComponent></AVDecoderComponent>
      </header>
    </div>
  );
}

export default App;
