// AudioWorklet processor for playing interleaved stereo float32 samples.
// This is a minimal ring-buffer based player.

class InterleavedStereoRing {
  constructor(capacityFrames) {
    this.capacityFrames = Math.max(256, capacityFrames | 0)
    this.buf = new Float32Array(this.capacityFrames * 2)
    this.read = 0
    this.write = 0
    this.queued = 0
  }

  push(samplesInterleaved) {
    if (!samplesInterleaved || samplesInterleaved.length < 2) return
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

  popFrame() {
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
  constructor() {
    super()
    this.ring = new InterleavedStereoRing(32768)

    this.port.onmessage = (ev) => {
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

  process(_inputs, outputs, _parameters) {
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
