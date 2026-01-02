import type { FrameSource } from '../../platform/video/Webgl2VideoSink'

export class FrameBuffer implements FrameSource {
  public readonly width: number
  public readonly height: number
  public readonly rgba: Uint8Array

  public constructor(width: number, height: number) {
    this.width = width
    this.height = height
    this.rgba = new Uint8Array(width * height * 4)
  }

  public clearRgb(r: number, g: number, b: number): void {
    const rr = r & 0xff
    const gg = g & 0xff
    const bb = b & 0xff

    for (let i = 0; i < this.rgba.length; i += 4) {
      this.rgba[i] = rr
      this.rgba[i + 1] = gg
      this.rgba[i + 2] = bb
      this.rgba[i + 3] = 0xff
    }
  }
}
