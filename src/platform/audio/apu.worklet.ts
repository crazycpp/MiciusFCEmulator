// AudioWorklet processor for playing interleaved stereo float32 samples.
// This is a minimal ring-buffer based player.

// TypeScript doesn't always include AudioWorklet global types in default configs.
// Declare the minimal surface we need for typechecking.
declare abstract class AudioWorkletProcessor {
  public readonly port: MessagePort
  public constructor()
  public abstract process(
    inputs: Float32Array[][],
    outputs: Float32Array[][],
    parameters: Record<string, Float32Array>,
  ): boolean
}

declare function registerProcessor(name: string, processorCtor: typeof AudioWorkletProcessor): void

type PushMessage = { type: 'push'; samples: Float32Array | ArrayBuffer }

type AnyMessage = PushMessage

class InterleavedStereoRing {
  private readonly capacityFrames: number
  private readonly buf: Float32Array
  private read = 0
  private write = 0
  private queued = 0

  public constructor(capacityFrames: number) {
    this.capacityFrames = Math.max(256, capacityFrames | 0)
    this.buf = new Float32Array(this.capacityFrames * 2)
  }

  public push(samplesInterleaved: Float32Array): void {
    if (samplesInterleaved.length < 2) return
    const frames = (samplesInterleaved.length / 2) | 0
    if (frames <= 0) return

    // If incoming chunk is larger than capacity, keep only the tail.
    let startFrame = 0
    let framesToWrite = frames
    if (framesToWrite > this.capacityFrames) {
      startFrame = framesToWrite - this.capacityFrames
      framesToWrite = this.capacityFrames
    }

    for (let i = 0; i < framesToWrite; i++) {
      const srcIndex = (startFrame + i) * 2
      const w = this.write
      this.buf[w * 2] = samplesInterleaved[srcIndex] ?? 0
      this.buf[w * 2 + 1] = samplesInterleaved[srcIndex + 1] ?? 0
      this.write = (this.write + 1) % this.capacityFrames
      if (this.queued < this.capacityFrames) {
        this.queued++
      } else {
        this.read = (this.read + 1) % this.capacityFrames
      }
    }
  }

  public popFrame(): [number, number] {
    if (this.queued <= 0) return [0, 0]
    const r = this.read
    const l = this.buf[r * 2] ?? 0
    const rr = this.buf[r * 2 + 1] ?? 0
    this.read = (this.read + 1) % this.capacityFrames
    this.queued--
    return [l, rr]
  }
}

class ApuPlayerProcessor extends AudioWorkletProcessor {
  private readonly ring = new InterleavedStereoRing(32768)

  public constructor() {
    super()
    this.port.onmessage = (ev: MessageEvent<AnyMessage>) => {
      const msg = ev.data
      if (!msg || msg.type !== 'push') return
      const s = msg.samples
      if (s instanceof Float32Array) {
        this.ring.push(s)
      } else if (s instanceof ArrayBuffer) {
        this.ring.push(new Float32Array(s))
      }
    }
  }

  public process(
    _inputs: Float32Array[][],
    outputs: Float32Array[][],
    _parameters: Record<string, Float32Array>,
  ): boolean {
    const out = outputs[0]
    const left = out?.[0]
    const right = out?.[1]
    if (!left || !right) return true

    for (let i = 0; i < left.length; i++) {
      const [l, r] = this.ring.popFrame()
      left[i] = l
      right[i] = r
    }

    return true
  }
}

registerProcessor('apu-player', ApuPlayerProcessor)
