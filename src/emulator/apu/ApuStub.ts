const CPU_HZ_NTSC = 1789773

const LENGTH_TABLE: number[] = [
  10, 254, 20, 2, 40, 4, 80, 6, 160, 8, 60, 10, 14, 12, 26, 14,
  12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30,
]

const NOISE_PERIOD_TABLE: number[] = [
  4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068,
]

const DUTY_RATIOS: number[] = [0.125, 0.25, 0.5, 0.75]

// NTSC DMC rates in CPU cycles (nesdev)
const DMC_RATE_TABLE: number[] = [
  428, 380, 340, 320, 286, 254, 226, 214,
  190, 160, 142, 128, 106, 85, 72, 54,
]

type EnvelopeState = {
  loop: boolean
  constantVolume: boolean
  volume: number
  period: number
  start: boolean
  divider: number
  decayLevel: number
}

function envelopeClock(env: EnvelopeState): void {
  if (env.start) {
    env.start = false
    env.decayLevel = 15
    env.divider = env.period
    return
  }

  if (env.divider > 0) {
    env.divider--
    return
  }

  env.divider = env.period
  if (env.decayLevel > 0) {
    env.decayLevel--
  } else if (env.loop) {
    env.decayLevel = 15
  }
}

function envelopeOutput(env: EnvelopeState): number {
  return env.constantVolume ? (env.volume & 0x0f) : (env.decayLevel & 0x0f)
}

type PulseChannel = {
  enabled: boolean
  duty: number
  lengthCounter: number
  lengthHalt: boolean
  timerPeriod: number
  sweepEnabled: boolean
  sweepPeriod: number
  sweepNegate: boolean
  sweepShift: number
  sweepReload: boolean
  sweepDivider: number
  phase: number
  env: EnvelopeState
}

type TriangleChannel = {
  enabled: boolean
  lengthCounter: number
  timerPeriod: number
  phase: number
  controlFlag: boolean
  linearReloadValue: number
  linearCounter: number
  linearReloadFlag: boolean
}

type NoiseChannel = {
  enabled: boolean
  lengthCounter: number
  lengthHalt: boolean
  mode: boolean
  periodIndex: number
  phase: number
  lfsr: number
  env: EnvelopeState
}

type DmcChannel = {
  enabled: boolean
  irqEnabled: boolean
  loop: boolean
  rateIndex: number
  outputLevel: number // 0..127

  sampleAddrReg: number
  sampleLenReg: number

  currentAddr: number
  bytesRemaining: number

  sampleBuffer: number
  sampleBufferFull: boolean

  shiftReg: number
  bitsRemaining: number

  irqFlag: boolean
  accCycles: number
}

export class ApuStub {
  private sampleRate: number | null = null

  private dmcReader: ((addr: number) => number) | null = null

  private pulse1: PulseChannel
  private pulse2: PulseChannel
  private tri: TriangleChannel
  private noise: NoiseChannel
  private dmc: DmcChannel

  private quarterAcc = 0
  private halfAcc = 0

  private sampleAcc = 0

  // Simple DC blocker to remove offset after mixing.
  private dcXPrev = 0
  private dcYPrev = 0

  // Stereo interleaved ring buffer (L,R,L,R...)
  // Increase buffering to tolerate AudioWorklet startup latency without dropping short SFX.
  // 65536 frames @ 48kHz ≈ 1.36s.
  private readonly outCapacityFrames = 65536
  private readonly out = new Float32Array(this.outCapacityFrames * 2)
  private outRead = 0
  private outWrite = 0
  private outQueued = 0

  public constructor() {
    const mkEnv = (): EnvelopeState => ({
      loop: false,
      constantVolume: false,
      volume: 0,
      period: 0,
      start: false,
      divider: 0,
      decayLevel: 0,
    })

    const mkPulse = (): PulseChannel => ({
      enabled: false,
      duty: 0,
      lengthCounter: 0,
      lengthHalt: false,
      timerPeriod: 0,
      sweepEnabled: false,
      sweepPeriod: 0,
      sweepNegate: false,
      sweepShift: 0,
      sweepReload: false,
      sweepDivider: 0,
      phase: 0,
      env: mkEnv(),
    })

    this.pulse1 = mkPulse()
    this.pulse2 = mkPulse()

    this.tri = {
      enabled: false,
      lengthCounter: 0,
      timerPeriod: 0,
      phase: 0,
      controlFlag: false,
      linearReloadValue: 0,
      linearCounter: 0,
      linearReloadFlag: false,
    }

    this.noise = {
      enabled: false,
      lengthCounter: 0,
      lengthHalt: false,
      mode: false,
      periodIndex: 0,
      phase: 0,
      lfsr: 1,
      env: mkEnv(),
    }

    this.dmc = {
      enabled: false,
      irqEnabled: false,
      loop: false,
      rateIndex: 0,
      outputLevel: 0,

      sampleAddrReg: 0,
      sampleLenReg: 0,

      currentAddr: 0,
      bytesRemaining: 0,

      sampleBuffer: 0,
      sampleBufferFull: false,

      shiftReg: 0,
      bitsRemaining: 0,

      irqFlag: false,
      accCycles: 0,
    }
  }

  /** Provide a side-effect-free CPU memory reader for DMC sample fetches. */
  public setDmcReader(reader: ((addr: number) => number) | null): void {
    this.dmcReader = reader
  }

  public setSampleRate(sampleRate: number | null): void {
    if (sampleRate !== null && (!Number.isFinite(sampleRate) || sampleRate <= 0)) return
    this.sampleRate = sampleRate
  }

  public drainSamplesInterleaved(maxFrames = 4096): Float32Array {
    const frames = Math.min(maxFrames | 0, this.outQueued)
    if (frames <= 0) return new Float32Array(0)

    const out = new Float32Array(frames * 2)
    for (let i = 0; i < frames; i++) {
      const r = this.outRead
      out[i * 2] = this.out[r * 2] ?? 0
      out[i * 2 + 1] = this.out[r * 2 + 1] ?? 0
      this.outRead = (this.outRead + 1) % this.outCapacityFrames
      this.outQueued--
    }
    return out
  }

  private pushSampleStereo(sample: number): void {
    const s = Math.max(-1, Math.min(1, sample))
    const w = this.outWrite
    this.out[w * 2] = s
    this.out[w * 2 + 1] = s
    this.outWrite = (this.outWrite + 1) % this.outCapacityFrames
    if (this.outQueued < this.outCapacityFrames) {
      this.outQueued++
    } else {
      // Drop oldest
      this.outRead = (this.outRead + 1) % this.outCapacityFrames
    }
  }

  public readRegister(addr: number): number {
    const a = addr & 0xffff
    if (a === 0x4015) {
      let v = 0
      if (this.pulse1.lengthCounter > 0) v |= 0x01
      if (this.pulse2.lengthCounter > 0) v |= 0x02
      if (this.tri.lengthCounter > 0) v |= 0x04
      if (this.noise.lengthCounter > 0) v |= 0x08
      if (this.dmc.bytesRemaining > 0) v |= 0x10
      if (this.dmc.irqFlag) v |= 0x80
      return v & 0xff
    }

    // Most APU registers are write-only; reads behave like open-bus.
    if (a >= 0x4000 && a <= 0x4017) return 0xff
    return 0
  }

  public writeRegister(addr: number, value: number): void {
    const a = addr & 0xffff
    const v = value & 0xff

    switch (a) {
      // Pulse 1
      case 0x4000: {
        this.pulse1.duty = (v >> 6) & 0x03
        this.pulse1.lengthHalt = (v & 0x20) !== 0
        this.pulse1.env.loop = this.pulse1.lengthHalt
        this.pulse1.env.constantVolume = (v & 0x10) !== 0
        this.pulse1.env.volume = v & 0x0f
        this.pulse1.env.period = v & 0x0f
        return
      }
      case 0x4001: {
        this.pulse1.sweepEnabled = (v & 0x80) !== 0
        this.pulse1.sweepPeriod = (v >> 4) & 0x07
        this.pulse1.sweepNegate = (v & 0x08) !== 0
        this.pulse1.sweepShift = v & 0x07
        this.pulse1.sweepReload = true
        return
      }
      case 0x4002: {
        this.pulse1.timerPeriod = (this.pulse1.timerPeriod & 0x700) | v
        return
      }
      case 0x4003: {
        this.pulse1.timerPeriod = (this.pulse1.timerPeriod & 0x0ff) | ((v & 0x07) << 8)
        if (this.pulse1.enabled) {
          const idx = (v >> 3) & 0x1f
          this.pulse1.lengthCounter = LENGTH_TABLE[idx] ?? 0
        }
        this.pulse1.env.start = true
        this.pulse1.phase = 0
        return
      }

      // Pulse 2
      case 0x4004: {
        this.pulse2.duty = (v >> 6) & 0x03
        this.pulse2.lengthHalt = (v & 0x20) !== 0
        this.pulse2.env.loop = this.pulse2.lengthHalt
        this.pulse2.env.constantVolume = (v & 0x10) !== 0
        this.pulse2.env.volume = v & 0x0f
        this.pulse2.env.period = v & 0x0f
        return
      }
      case 0x4005: {
        this.pulse2.sweepEnabled = (v & 0x80) !== 0
        this.pulse2.sweepPeriod = (v >> 4) & 0x07
        this.pulse2.sweepNegate = (v & 0x08) !== 0
        this.pulse2.sweepShift = v & 0x07
        this.pulse2.sweepReload = true
        return
      }
      case 0x4006: {
        this.pulse2.timerPeriod = (this.pulse2.timerPeriod & 0x700) | v
        return
      }
      case 0x4007: {
        this.pulse2.timerPeriod = (this.pulse2.timerPeriod & 0x0ff) | ((v & 0x07) << 8)
        if (this.pulse2.enabled) {
          const idx = (v >> 3) & 0x1f
          this.pulse2.lengthCounter = LENGTH_TABLE[idx] ?? 0
        }
        this.pulse2.env.start = true
        this.pulse2.phase = 0
        return
      }

      // Triangle
      case 0x4008: {
        this.tri.controlFlag = (v & 0x80) !== 0
        this.tri.linearReloadValue = v & 0x7f
        return
      }
      case 0x400a: {
        this.tri.timerPeriod = (this.tri.timerPeriod & 0x700) | v
        return
      }
      case 0x400b: {
        this.tri.timerPeriod = (this.tri.timerPeriod & 0x0ff) | ((v & 0x07) << 8)
        if (this.tri.enabled) {
          const idx = (v >> 3) & 0x1f
          this.tri.lengthCounter = LENGTH_TABLE[idx] ?? 0
        }
        this.tri.linearReloadFlag = true
        this.tri.phase = 0
        return
      }

      // Noise
      case 0x400c: {
        this.noise.lengthHalt = (v & 0x20) !== 0
        this.noise.env.loop = this.noise.lengthHalt
        this.noise.env.constantVolume = (v & 0x10) !== 0
        this.noise.env.volume = v & 0x0f
        this.noise.env.period = v & 0x0f
        return
      }
      case 0x400e: {
        this.noise.mode = (v & 0x80) !== 0
        this.noise.periodIndex = v & 0x0f
        return
      }
      case 0x400f: {
        if (this.noise.enabled) {
          const idx = (v >> 3) & 0x1f
          this.noise.lengthCounter = LENGTH_TABLE[idx] ?? 0
        }
        this.noise.env.start = true
        return
      }

      // DMC
      case 0x4010: {
        this.dmc.irqEnabled = (v & 0x80) !== 0
        this.dmc.loop = (v & 0x40) !== 0
        this.dmc.rateIndex = v & 0x0f
        if (!this.dmc.irqEnabled) this.dmc.irqFlag = false
        return
      }
      case 0x4011:
        // Direct load of DAC level.
        this.dmc.outputLevel = v & 0x7f
        return
      case 0x4012:
        this.dmc.sampleAddrReg = v & 0xff
        return
      case 0x4013:
        this.dmc.sampleLenReg = v & 0xff
        return

      case 0x4015: {
        const p1 = (v & 0x01) !== 0
        const p2 = (v & 0x02) !== 0
        const tr = (v & 0x04) !== 0
        const no = (v & 0x08) !== 0
        const dm = (v & 0x10) !== 0

        this.pulse1.enabled = p1
        this.pulse2.enabled = p2
        this.tri.enabled = tr
        this.noise.enabled = no
        if (!p1) this.pulse1.lengthCounter = 0
        if (!p2) this.pulse2.lengthCounter = 0
        if (!tr) this.tri.lengthCounter = 0
        if (!no) this.noise.lengthCounter = 0

        if (!dm) {
          this.dmc.enabled = false
          this.dmc.bytesRemaining = 0
          this.dmc.sampleBufferFull = false
          this.dmc.bitsRemaining = 0
          this.dmc.irqFlag = false
        } else {
          this.dmc.enabled = true
          // If starting and no sample currently active, begin using the configured address/length.
          if (this.dmc.bytesRemaining === 0) {
            this.restartDmcSample()
          }
        }
        return
      }
      case 0x4017: {
        // MVP: we ignore 4-step/5-step differences and IRQ behavior,
        // but writing here should reset sequencer timing.
        this.quarterAcc = 0
        this.halfAcc = 0
        return
      }
      default:
        return
    }
  }

  public tickCpuCycles(deltaCycles: number): void {
    const cycles = deltaCycles | 0
    if (cycles <= 0) return

    this.clockDmcCpuCycles(cycles)

    // Quarter frame @ 240Hz, half frame @ 120Hz.
    this.quarterAcc += cycles * 240
    while (this.quarterAcc >= CPU_HZ_NTSC) {
      this.quarterAcc -= CPU_HZ_NTSC
      this.clockQuarterFrame()
    }

    this.halfAcc += cycles * 120
    while (this.halfAcc >= CPU_HZ_NTSC) {
      this.halfAcc -= CPU_HZ_NTSC
      this.clockHalfFrame()
    }

    if (!this.sampleRate) return

    this.sampleAcc += cycles * this.sampleRate
    while (this.sampleAcc >= CPU_HZ_NTSC) {
      this.sampleAcc -= CPU_HZ_NTSC
      const s = this.renderSample()
      this.pushSampleStereo(s)
    }
  }

  private clockQuarterFrame(): void {
    envelopeClock(this.pulse1.env)
    envelopeClock(this.pulse2.env)
    envelopeClock(this.noise.env)

    // Triangle linear counter
    if (this.tri.linearReloadFlag) {
      this.tri.linearCounter = this.tri.linearReloadValue & 0x7f
    } else if (this.tri.linearCounter > 0) {
      this.tri.linearCounter--
    }
    if (!this.tri.controlFlag) {
      this.tri.linearReloadFlag = false
    }
  }

  private clockHalfFrame(): void {
    // Length counters
    if (!this.pulse1.lengthHalt && this.pulse1.lengthCounter > 0) this.pulse1.lengthCounter--
    if (!this.pulse2.lengthHalt && this.pulse2.lengthCounter > 0) this.pulse2.lengthCounter--
    if (!this.tri.controlFlag && this.tri.lengthCounter > 0) this.tri.lengthCounter--
    if (!this.noise.lengthHalt && this.noise.lengthCounter > 0) this.noise.lengthCounter--

    this.clockSweep(this.pulse1, true)
    this.clockSweep(this.pulse2, false)
  }

  private clockSweep(ch: PulseChannel, isPulse1: boolean): void {
    if (ch.sweepDivider > 0) {
      ch.sweepDivider--
    }

    const dividerHit = ch.sweepDivider === 0
    if (dividerHit) {
      if (ch.sweepEnabled && ch.sweepShift > 0) {
        const change = ch.timerPeriod >> ch.sweepShift
        let target = ch.timerPeriod
        if (ch.sweepNegate) {
          target = ch.timerPeriod - change - (isPulse1 ? 1 : 0)
        } else {
          target = ch.timerPeriod + change
        }

        if (target >= 0 && target <= 0x7ff) {
          ch.timerPeriod = target & 0x7ff
        }
      }

      ch.sweepDivider = ch.sweepPeriod
    }

    if (ch.sweepReload) {
      ch.sweepReload = false
      ch.sweepDivider = ch.sweepPeriod
    }
  }

  private pulseOutput(ch: PulseChannel, isPulse1: boolean): number {
    if (!ch.enabled) return 0
    if (ch.lengthCounter <= 0) return 0
    const period = ch.timerPeriod & 0x7ff
    if (period < 8) return 0

    // Sweep mute condition: if enabled and shift > 0 and target would overflow.
    if (ch.sweepEnabled && ch.sweepShift > 0) {
      const change = period >> ch.sweepShift
      let target = period
      if (ch.sweepNegate) {
        target = period - change - (isPulse1 ? 1 : 0)
      } else {
        target = period + change
      }
      if (target > 0x7ff) return 0
    }

    if (!this.sampleRate) return 0
    const freq = CPU_HZ_NTSC / (16 * (period + 1))
    ch.phase += freq / this.sampleRate
    ch.phase -= Math.floor(ch.phase)

    const duty = DUTY_RATIOS[ch.duty & 3] ?? 0.5
    const amp = ch.phase < duty ? 1 : 0
    const vol = envelopeOutput(ch.env)
    return amp * vol
  }

  private triangleOutput(): number {
    if (!this.tri.enabled) return 0
    if (this.tri.lengthCounter <= 0) return 0
    if (this.tri.linearCounter <= 0) return 0
    const period = this.tri.timerPeriod & 0x7ff
    if (period < 2) return 0
    if (!this.sampleRate) return 0

    const freq = CPU_HZ_NTSC / (32 * (period + 1))
    this.tri.phase += freq / this.sampleRate
    this.tri.phase -= Math.floor(this.tri.phase)

    // 0..1 triangle
    const t = this.tri.phase
    const tri01 = t < 0.5 ? t * 2 : (1 - t) * 2
    return tri01 * 15
  }

  private noiseOutput(): number {
    if (!this.noise.enabled) return 0
    if (this.noise.lengthCounter <= 0) return 0
    if (!this.sampleRate) return 0

    const period = NOISE_PERIOD_TABLE[this.noise.periodIndex & 0x0f] ?? 4
    // Noise timer is clocked by the CPU/APU clock; table values are in CPU cycles.
    const freq = CPU_HZ_NTSC / period
    this.noise.phase += freq / this.sampleRate
    while (this.noise.phase >= 1) {
      this.noise.phase -= 1
      const bit0 = this.noise.lfsr & 1
      const tap = this.noise.mode ? ((this.noise.lfsr >> 6) & 1) : ((this.noise.lfsr >> 1) & 1)
      const feedback = bit0 ^ tap
      this.noise.lfsr = (this.noise.lfsr >> 1) | (feedback << 14)
      this.noise.lfsr &= 0x7fff
    }

    const outBit = (this.noise.lfsr & 1) === 0 ? 1 : 0
    const vol = envelopeOutput(this.noise.env)
    return outBit * vol
  }

  private restartDmcSample(): void {
    // Start address: $C000 + (addrReg * 64)
    this.dmc.currentAddr = 0xc000 + ((this.dmc.sampleAddrReg & 0xff) << 6)
    // Length: (lenReg * 16) + 1
    this.dmc.bytesRemaining = ((this.dmc.sampleLenReg & 0xff) << 4) + 1
  }

  private dmcStep(): void {
    if (!this.dmc.enabled) return

    // Refill sample buffer if empty.
    if (!this.dmc.sampleBufferFull) {
      if (this.dmc.bytesRemaining > 0) {
        const read = this.dmcReader
        const b = read ? (read(this.dmc.currentAddr & 0xffff) & 0xff) : 0
        this.dmc.sampleBuffer = b
        this.dmc.sampleBufferFull = true
        this.dmc.currentAddr = (this.dmc.currentAddr + 1) & 0xffff
        if (this.dmc.currentAddr === 0x0000) this.dmc.currentAddr = 0x8000
        this.dmc.bytesRemaining--
      } else {
        if (this.dmc.loop) {
          this.restartDmcSample()
        } else {
          if (this.dmc.irqEnabled) this.dmc.irqFlag = true
        }
      }
    }

    // Load shift register when no bits remaining.
    if (this.dmc.bitsRemaining === 0) {
      if (this.dmc.sampleBufferFull) {
        this.dmc.shiftReg = this.dmc.sampleBuffer & 0xff
        this.dmc.sampleBufferFull = false
        this.dmc.bitsRemaining = 8
      } else {
        // No data available; output holds.
        return
      }
    }

    const bit = this.dmc.shiftReg & 1
    if (bit === 1) {
      if (this.dmc.outputLevel <= 125) this.dmc.outputLevel += 2
    } else {
      if (this.dmc.outputLevel >= 2) this.dmc.outputLevel -= 2
    }

    this.dmc.shiftReg = (this.dmc.shiftReg >> 1) & 0xff
    this.dmc.bitsRemaining = (this.dmc.bitsRemaining - 1) & 0xff
  }

  private clockDmcCpuCycles(cycles: number): void {
    if (!this.dmc.enabled) return
    const period = DMC_RATE_TABLE[this.dmc.rateIndex & 0x0f] ?? 428
    this.dmc.accCycles += cycles
    while (this.dmc.accCycles >= period) {
      this.dmc.accCycles -= period
      this.dmcStep()
    }
  }

  private renderSample(): number {
    const p1 = this.pulseOutput(this.pulse1, true)
    const p2 = this.pulseOutput(this.pulse2, false)
    const t = this.triangleOutput()
    const n = this.noiseOutput()
    const d = this.dmc.enabled ? (this.dmc.outputLevel & 0x7f) : 0

    const pulseSum = p1 + p2
    const pulseOut = pulseSum > 0 ? 95.88 / (8128.0 / pulseSum + 100.0) : 0

    const tndDen = t / 8227.0 + n / 12241.0 + d / 22638.0
    const tndOut = tndDen > 0 ? 159.79 / (1.0 / tndDen + 100.0) : 0

    // 0..~1, unipolar; convert to roughly [-1..1] with headroom.
    const x = (pulseOut + tndOut) * 2 - 1

    // DC blocker
    const y = x - this.dcXPrev + 0.995 * this.dcYPrev
    this.dcXPrev = x
    this.dcYPrev = y

    return y * 0.8
  }

  public reset(): void {
    // Keep sampleRate; it's owned by the audio output device (AudioContext).
    // Resetting the APU should not disable audio.
    this.quarterAcc = 0
    this.halfAcc = 0
    this.sampleAcc = 0

    this.dcXPrev = 0
    this.dcYPrev = 0

    this.pulse1.enabled = false
    this.pulse1.lengthCounter = 0
    this.pulse1.timerPeriod = 0
    this.pulse1.phase = 0
    this.pulse1.env.start = false
    this.pulse1.env.divider = 0
    this.pulse1.env.decayLevel = 0

    this.pulse2.enabled = false
    this.pulse2.lengthCounter = 0
    this.pulse2.timerPeriod = 0
    this.pulse2.phase = 0
    this.pulse2.env.start = false
    this.pulse2.env.divider = 0
    this.pulse2.env.decayLevel = 0

    this.tri.enabled = false
    this.tri.lengthCounter = 0
    this.tri.linearCounter = 0
    this.tri.linearReloadFlag = false
    this.tri.timerPeriod = 0
    this.tri.phase = 0

    this.noise.enabled = false
    this.noise.lengthCounter = 0
    this.noise.phase = 0
    this.noise.lfsr = 1
    this.noise.env.start = false
    this.noise.env.divider = 0
    this.noise.env.decayLevel = 0

    this.dmc.enabled = false
    this.dmc.irqEnabled = false
    this.dmc.loop = false
    this.dmc.rateIndex = 0
    this.dmc.outputLevel = 0
    this.dmc.sampleAddrReg = 0
    this.dmc.sampleLenReg = 0
    this.dmc.currentAddr = 0
    this.dmc.bytesRemaining = 0
    this.dmc.sampleBuffer = 0
    this.dmc.sampleBufferFull = false
    this.dmc.shiftReg = 0
    this.dmc.bitsRemaining = 0
    this.dmc.irqFlag = false
    this.dmc.accCycles = 0

    this.outRead = 0
    this.outWrite = 0
    this.outQueued = 0
  }
}
