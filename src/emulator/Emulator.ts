import { FrameBuffer } from './video/FrameBuffer'
import { BreakpointManager } from './debug/BreakpointManager'
import { TraceBuffer } from './debug/TraceBuffer'
import { ApuStub } from './apu/ApuStub'
import { Bus } from './bus/Bus'
import { Cpu6502 } from './cpu/Cpu6502'
import { Cartridge } from './cart/Cartridge'
import { Disassembler } from './cpu/Disassembler'
import { PpuStub } from './ppu/PpuStub'
import { hex16, hex8 } from './util/hex'
import type { JoypadButton } from './input/Joypad'

export type EmulatorRunState = 'stopped' | 'running' | 'paused'

export class Emulator {
  public readonly frameBuffer = new FrameBuffer(256, 240)
  public readonly breakpoints = new BreakpointManager()
  // nestest.log is ~8991 lines; keep some headroom so we can compare whole runs.
  public readonly trace = new TraceBuffer(20000)

  private cpuTraceEnabled = false

  private readonly apu = new ApuStub()
  private readonly ppu = new PpuStub()
  private readonly bus = new Bus(this.apu, this.ppu)
  private readonly cpu = new Cpu6502(this.bus)
  private readonly disasm = new Disassembler(this.bus)

  private cart: Cartridge | null = null

  private state: EmulatorRunState = 'stopped'
  private rafId: number | null = null

  // Timing: lock emulation stepping to NTSC frame rate.
  // Many displays run at 120/144Hz; without a lock, games run too fast.
  private readonly frameMs = 1000 / 60.0988
  private lastRafTsMs = 0
  private accMs = 0

  public get runState(): EmulatorRunState {
    return this.state
  }

  public getPpuDebugInfo(): {
    readonly ppuctrl: number
    readonly ppumask: number
    readonly ppustatus: number
    readonly oamaddr: number
    readonly oamDmaCount: number
    readonly lastOamDmaPage: number
    readonly lastOamDmaNonZero: number
    readonly lastVisibleSpriteCount: number
    readonly lastSpriteDrawnCount: number
    readonly lastSpritePixelsDrawn: number
    readonly lastSprite0Hit: boolean
    readonly scrollX: number
    readonly scrollY: number
    readonly scrollNt: number
    readonly frameStartScrollX: number
    readonly frameStartScrollY: number
    readonly frameStartScrollNt: number
    readonly nmiCount: number
    readonly frameId: number
    readonly frameWrite2000: number
    readonly frameWrite2005: number
    readonly frameWrite2006: number
    readonly frameWrite2007: number
    readonly lastWrite2000: number
    readonly lastWrite2005_1: number
    readonly lastWrite2005_2: number
    readonly frameMaxWrite2005_1: number
    readonly frameMaxWrite2005_2: number
    readonly frame2005PairCount: number
    readonly frameFirst2005_1: number
    readonly frameFirst2005_2: number
    readonly frameLast2005_1: number
    readonly frameLast2005_2: number
    readonly frameLastNonZero2005_1: number
    readonly frameLastNonZero2005_2: number
    readonly v: number
    readonly t: number
    readonly xFine: number
    readonly wLatch: number
    readonly oamPreview: string
  } {
    return this.ppu.getDebugInfo()
  }

  public getCartDebugInfo(): {
    readonly loaded: boolean
    readonly mapperNumber: number
    readonly mirroring: string
    readonly prgRomSizeBytes: number
    readonly chrRomSizeBytes: number
    readonly chrIsRam: boolean
  } {
    if (!this.cart) {
      return {
        loaded: false,
        mapperNumber: -1,
        mirroring: 'none',
        prgRomSizeBytes: 0,
        chrRomSizeBytes: 0,
        chrIsRam: false,
      }
    }

    return {
      loaded: true,
      mapperNumber: this.cart.mapperNumber,
      mirroring: this.cart.mirroring,
      prgRomSizeBytes: this.cart.prgRomSizeBytes,
      chrRomSizeBytes: this.cart.chrRomSizeBytes,
      chrIsRam: this.cart.hasChrRam,
    }
  }

  public setCpuTraceEnabled(enabled: boolean): void {
    this.cpuTraceEnabled = enabled
  }

  public getTestRomStatus(): { readonly status: number; readonly message: string } {
    // Common convention used by many NES test ROMs:
    // - $6000: 0 = running, 1 = pass, other = fail/error
    // - $6004+: null-terminated ASCII message
    const status = this.bus.peek(0x6000) & 0xff

    let message = ''
    if (status !== 0) {
      const chars: number[] = []
      for (let i = 0; i < 200; i++) {
        const b = this.bus.peek((0x6004 + i) & 0xffff) & 0xff
        if (b === 0x00) break
        chars.push(b)
      }

      // Basic ASCII sanity check.
      let printable = 0
      for (const c of chars) {
        if (c === 0x0a || c === 0x0d || (c >= 0x20 && c <= 0x7e)) printable++
      }
      if (chars.length > 0 && printable / chars.length >= 0.9) {
        message = String.fromCharCode(...chars)
      }
    }

    return { status, message }
  }

  public setAudioSampleRate(sampleRate: number | null): void {
    this.apu.setSampleRate(sampleRate)
  }

  public drainAudioSamplesInterleaved(maxFrames = 4096): Float32Array {
    return this.apu.drainSamplesInterleaved(maxFrames)
  }

  public setJoypad1Button(button: JoypadButton, pressed: boolean): void {
    this.bus.setJoypad1Button(button, pressed)
  }

  public setJoypad2Button(button: JoypadButton, pressed: boolean): void {
    this.bus.setJoypad2Button(button, pressed)
  }

  public getJoypad1DebugInfo(): { readonly buttons: number; readonly strobe: boolean; readonly shift: number } {
    return this.bus.getJoypad1DebugInfo()
  }

  public getJoypad2DebugInfo(): { readonly buttons: number; readonly strobe: boolean; readonly shift: number } {
    return this.bus.getJoypad2DebugInfo()
  }

  public reset(): void {
    this.trace.clear()
    this.frameBuffer.clearRgb(0, 0, 0)
    this.bus.reset()
    this.cpu.reset()

    let pc = this.cpu.getRegisters().pc
    this.trace.push(`RESET PC=${hex16(pc)}`)

    // Diagnostics for nestest alignment: confirm the reset vector and the expected entry bytes.
    const vLo = this.bus.peek(0xfffc) & 0xff
    const vHi = this.bus.peek(0xfffd) & 0xff
    const c0 = this.bus.peek(0xc000) & 0xff
    const c1 = this.bus.peek(0xc001) & 0xff
    const c2 = this.bus.peek(0xc002) & 0xff
    this.trace.push(
      `VEC FFFC:${hex8(vLo)} FFFD:${hex8(vHi)} => ${hex16(vLo | (vHi << 8))} | C000:${hex8(c0)} ${hex8(c1)} ${hex8(c2)}`,
    )

    // nestest.nes commonly expects the CPU to start at $C000 (test entrypoint),
    // while the reset vector points into a PPU-wait loop (which we don't emulate yet).
    if (this.isNestestLikeRom(c0, c1, c2)) {
      this.cpu.setProgramCounter(0xc000)
      pc = this.cpu.getRegisters().pc
      this.trace.push(`FORCE PC=${hex16(pc)} (nestest entrypoint)`)
    }

    this.state = 'paused'
  }

  private isNestestLikeRom(c0: number, c1: number, c2: number): boolean {
    // Signature: C000 contains JMP $C5F5 (4C F5 C5) in the common nestest build.
    return c0 === 0x4c && c1 === 0xf5 && c2 === 0xc5
  }

  public loadRom(buffer: ArrayBuffer): void {
    this.cart = Cartridge.fromArrayBuffer(buffer)
    this.bus.setCartridge(this.cart)
    this.ppu.setCartridge(this.cart, this.cart.mirroring)
    this.reset()
  }

  public start(): void {
    if (this.state === 'running') return
    this.state = 'running'
    this.lastRafTsMs = performance.now()
    this.accMs = 0
    this.schedule()
  }

  public pause(): void {
    if (this.state !== 'running') return
    this.state = 'paused'
    if (this.rafId !== null) {
      cancelAnimationFrame(this.rafId)
      this.rafId = null
    }
  }

  public stepFrame(): void {
    // Step a full frame bounded by the PPU pre-render boundary.
    // Render once at vblank start (end of visible frame), then continue stepping
    // through vblank so the game's NMI handler can update scroll/VRAM for next frame.

    let rendered = false

    // Safety cap so we never spin forever if something goes wrong.
    let executed = 0
    const maxInstructions = 300000

    while (executed < maxInstructions) {
      const cycBefore = this.cpu.getCycleCount()

      this.cpu.step(({ pc, regs, cyc }) => {
        if (this.breakpoints.has(pc)) {
          this.trace.push(`BREAK @ $${pc.toString(16).padStart(4, '0').toUpperCase()}`)
          this.pause()
          return 'pause'
        }

        if (this.cpuTraceEnabled) {
          const d = this.disasm.disassembleAt(pc, regs)

          const b0 = d.bytes[0] ?? 0
          const b1 = d.bytes.length > 1 ? (d.bytes[1] ?? 0) : null
          const b2 = d.bytes.length > 2 ? (d.bytes[2] ?? 0) : null

          const bytesCol = [hex8(b0), b1 !== null ? hex8(b1) : '  ', b2 !== null ? hex8(b2) : '  '].join(' ')
          const asmCol = d.text.padEnd(34, ' ')

          // For nestest-style traces, PPU dot is commonly shown as CPU_CYC * 3.
          // With reset cycles starting at 7, this yields PPU:  0, 21 at CYC:7.
          const ppuCycle = cyc * 3
          const ppuScanline = Math.floor(ppuCycle / 341) % 262
          const ppuDot = ppuCycle % 341

          const line =
            `${hex16(pc)}  ${bytesCol}  ${asmCol}` +
            `A:${hex8(regs.a)} X:${hex8(regs.x)} Y:${hex8(regs.y)} P:${hex8(regs.p)} SP:${hex8(regs.sp)} ` +
            `PPU:${ppuScanline.toString().padStart(3, ' ')},${ppuDot.toString().padStart(3, ' ')} ` +
            `CYC:${cyc}`

          this.trace.push(line)
        }

        return 'continue'
      })

      const cycAfter = this.cpu.getCycleCount()
      const delta = Math.max(0, cycAfter - cycBefore)
      this.apu.tickCpuCycles(delta)
      this.ppu.tick(delta * 3)
      if (this.ppu.pollNmi()) {
        this.cpu.requestNmi()
      }

      if (!rendered && this.ppu.pollVblankStart()) {
        // Render the just-completed visible frame before the NMI handler mutates state.
        this.ppu.renderFrame(this.frameBuffer)
        rendered = true
      }

      if (rendered && this.ppu.pollFrameStart()) {
        break
      }

      if (this.state !== 'running' && this.state !== 'paused') {
        // Shouldn't happen, but avoid looping if state is changed unexpectedly.
        break
      }

      executed++
    }
  }

  public stepInstruction(): void {
    const cycBefore = this.cpu.getCycleCount()

    this.cpu.step(({ pc, regs, cyc }) => {
      if (this.breakpoints.has(pc)) {
        this.trace.push(`BREAK @ $${hex16(pc)}`)
        this.pause()
        return 'pause'
      }

      if (this.cpuTraceEnabled) {
        const d = this.disasm.disassembleAt(pc, regs)
        const b0 = d.bytes[0] ?? 0
        const b1 = d.bytes.length > 1 ? (d.bytes[1] ?? 0) : null
        const b2 = d.bytes.length > 2 ? (d.bytes[2] ?? 0) : null
        const bytesCol = [hex8(b0), b1 !== null ? hex8(b1) : '  ', b2 !== null ? hex8(b2) : '  '].join(' ')
        const asmCol = d.text.padEnd(34, ' ')

        const ppuCycle = cyc * 3
        const ppuScanline = Math.floor(ppuCycle / 341) % 262
        const ppuDot = ppuCycle % 341

        this.trace.push(
          `${hex16(pc)}  ${bytesCol}  ${asmCol}` +
            `A:${hex8(regs.a)} X:${hex8(regs.x)} Y:${hex8(regs.y)} P:${hex8(regs.p)} SP:${hex8(regs.sp)} ` +
            `PPU:${ppuScanline.toString().padStart(3, ' ')},${ppuDot.toString().padStart(3, ' ')} ` +
            `CYC:${cyc}`,
        )
      }

      return 'continue'
    })

    const cycAfter = this.cpu.getCycleCount()
    const delta = Math.max(0, cycAfter - cycBefore)
    this.apu.tickCpuCycles(delta)
    this.ppu.tick(delta * 3)
    if (this.ppu.pollNmi()) {
      this.cpu.requestNmi()
    }
  }

  private schedule(): void {
    if (this.state !== 'running') return

    this.rafId = requestAnimationFrame((ts) => {
      const now = ts
      let dt = now - this.lastRafTsMs
      this.lastRafTsMs = now

      // Avoid huge catch-up spirals after tab was inactive.
      if (dt < 0) dt = 0
      if (dt > 250) dt = 250

      this.accMs += dt

      // Run as many whole frames as we have accumulated.
      // Cap per RAF to keep UI responsive.
      const maxFramesPerRaf = 3
      let ran = 0
      while (this.accMs >= this.frameMs && ran < maxFramesPerRaf) {
        this.stepFrame()
        this.accMs -= this.frameMs
        ran++
      }

      this.schedule()
    })
  }
}
