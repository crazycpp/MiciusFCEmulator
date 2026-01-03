export interface FrameSource {
  readonly width: number
  readonly height: number
  /** RGBA8888 pixel data, row-major, length = width*height*4 */
  readonly rgba: Uint8Array
}

export interface VideoSink {
  initialize(canvas: HTMLCanvasElement): void
  present(frame: FrameSource): void
  dispose(): void
}

export class Webgl2VideoSink implements VideoSink {
  private gl: WebGL2RenderingContext | null = null
  private program: WebGLProgram | null = null
  private vao: WebGLVertexArrayObject | null = null
  private texture: WebGLTexture | null = null
  private uTexLoc: WebGLUniformLocation | null = null
  private uScaleLoc: WebGLUniformLocation | null = null

  private frameWidth = 0
  private frameHeight = 0

  initialize(canvas: HTMLCanvasElement): void {
    const gl = canvas.getContext('webgl2', { alpha: false, antialias: false })
    if (!gl) {
      throw new Error('WebGL2 not supported')
    }

    this.gl = gl

    const vsSource = `#version 300 es
      precision highp float;
      layout(location = 0) in vec2 aPos;
      layout(location = 1) in vec2 aUv;
      out vec2 vUv;
      uniform vec2 uScale;
      void main() {
        vUv = aUv;
        gl_Position = vec4(aPos * uScale, 0.0, 1.0);
      }
    `

    const fsSource = `#version 300 es
      precision highp float;
      in vec2 vUv;
      out vec4 outColor;
      uniform sampler2D uTex;
      void main() {
        outColor = texture(uTex, vUv);
      }
    `

    const program = this.createProgram(gl, vsSource, fsSource)
    this.program = program

    this.uTexLoc = gl.getUniformLocation(program, 'uTex')
    this.uScaleLoc = gl.getUniformLocation(program, 'uScale')

    const vao = gl.createVertexArray()
    if (!vao) throw new Error('Failed to create VAO')
    this.vao = vao

    const vbo = gl.createBuffer()
    if (!vbo) throw new Error('Failed to create VBO')

    gl.bindVertexArray(vao)
    gl.bindBuffer(gl.ARRAY_BUFFER, vbo)

    // Fullscreen quad (two triangles) with UVs.
    // x, y, u, v
    // Crop a small horizontal overscan region so left-edge masking doesn't show as a vertical strip.
    const u0 = 8 / 256
    const u1 = 1 - u0
    const verts = new Float32Array([
      -1, -1, u0, 1,
      1, -1, u1, 1,
      -1, 1, u0, 0,
      -1, 1, u0, 0,
      1, -1, u1, 1,
      1, 1, u1, 0,
    ])

    gl.bufferData(gl.ARRAY_BUFFER, verts, gl.STATIC_DRAW)

    gl.enableVertexAttribArray(0)
    gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 16, 0)
    gl.enableVertexAttribArray(1)
    gl.vertexAttribPointer(1, 2, gl.FLOAT, false, 16, 8)

    gl.bindVertexArray(null)
    gl.bindBuffer(gl.ARRAY_BUFFER, null)

    const texture = gl.createTexture()
    if (!texture) throw new Error('Failed to create texture')
    this.texture = texture

    gl.bindTexture(gl.TEXTURE_2D, texture)
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST)
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST)
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE)
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE)
    gl.bindTexture(gl.TEXTURE_2D, null)

    gl.disable(gl.DEPTH_TEST)
    gl.disable(gl.BLEND)

    this.resizeToCanvas(canvas)
  }

  present(frame: FrameSource): void {
    const gl = this.gl
    const program = this.program
    const vao = this.vao
    const texture = this.texture

    if (!gl || !program || !vao || !texture) return

    if (frame.width !== this.frameWidth || frame.height !== this.frameHeight) {
      this.frameWidth = frame.width
      this.frameHeight = frame.height
      gl.bindTexture(gl.TEXTURE_2D, texture)
      gl.texImage2D(
        gl.TEXTURE_2D,
        0,
        gl.RGBA,
        frame.width,
        frame.height,
        0,
        gl.RGBA,
        gl.UNSIGNED_BYTE,
        frame.rgba,
      )
      gl.bindTexture(gl.TEXTURE_2D, null)
    } else {
      gl.bindTexture(gl.TEXTURE_2D, texture)
      gl.texSubImage2D(gl.TEXTURE_2D, 0, 0, 0, frame.width, frame.height, gl.RGBA, gl.UNSIGNED_BYTE, frame.rgba)
      gl.bindTexture(gl.TEXTURE_2D, null)
    }

    gl.viewport(0, 0, gl.drawingBufferWidth, gl.drawingBufferHeight)
    gl.clearColor(0, 0, 0, 1)
    gl.clear(gl.COLOR_BUFFER_BIT)

    gl.useProgram(program)
    gl.bindVertexArray(vao)

    gl.activeTexture(gl.TEXTURE0)
    gl.bindTexture(gl.TEXTURE_2D, texture)
    gl.uniform1i(this.uTexLoc, 0)

    // Preserve aspect ratio (NES 256x240).
    const targetAspect = frame.width / frame.height
    const canvasAspect = gl.drawingBufferWidth / gl.drawingBufferHeight
    let sx = 1
    let sy = 1
    if (canvasAspect > targetAspect) {
      sx = targetAspect / canvasAspect
    } else {
      sy = canvasAspect / targetAspect
    }
    gl.uniform2f(this.uScaleLoc, sx, sy)

    gl.drawArrays(gl.TRIANGLES, 0, 6)

    gl.bindTexture(gl.TEXTURE_2D, null)
    gl.bindVertexArray(null)
    gl.useProgram(null)
  }

  dispose(): void {
    const gl = this.gl
    if (!gl) return

    if (this.texture) gl.deleteTexture(this.texture)
    if (this.program) gl.deleteProgram(this.program)
    if (this.vao) gl.deleteVertexArray(this.vao)

    this.texture = null
    this.program = null
    this.vao = null
    this.gl = null
  }

  private resizeToCanvas(canvas: HTMLCanvasElement): void {
    // Ensure drawing buffer matches CSS size.
    const dpr = Math.max(1, window.devicePixelRatio || 1)
    const width = Math.floor(canvas.clientWidth * dpr)
    const height = Math.floor(canvas.clientHeight * dpr)
    if (canvas.width !== width) canvas.width = width
    if (canvas.height !== height) canvas.height = height
  }

  private createProgram(gl: WebGL2RenderingContext, vsSource: string, fsSource: string): WebGLProgram {
    const vs = this.compileShader(gl, gl.VERTEX_SHADER, vsSource)
    const fs = this.compileShader(gl, gl.FRAGMENT_SHADER, fsSource)

    const program = gl.createProgram()
    if (!program) throw new Error('Failed to create program')

    gl.attachShader(program, vs)
    gl.attachShader(program, fs)
    gl.linkProgram(program)

    gl.deleteShader(vs)
    gl.deleteShader(fs)

    if (!gl.getProgramParameter(program, gl.LINK_STATUS)) {
      const log = gl.getProgramInfoLog(program) ?? 'Unknown link error'
      gl.deleteProgram(program)
      throw new Error(`Shader link failed: ${log}`)
    }

    return program
  }

  private compileShader(gl: WebGL2RenderingContext, type: number, source: string): WebGLShader {
    const shader = gl.createShader(type)
    if (!shader) throw new Error('Failed to create shader')

    gl.shaderSource(shader, source)
    gl.compileShader(shader)

    if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) {
      const log = gl.getShaderInfoLog(shader) ?? 'Unknown compile error'
      gl.deleteShader(shader)
      throw new Error(`Shader compile failed: ${log}`)
    }

    return shader
  }
}
