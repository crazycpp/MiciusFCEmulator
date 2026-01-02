import type { ApuStub } from '../apu/ApuStub'
import type { Cartridge } from '../cart/Cartridge'
import type { PpuStub } from '../ppu/PpuStub'
import type { JoypadButton } from '../input/Joypad'
import { Joypad } from '../input/Joypad'

export interface CpuAddressSpace {
  read(addr: number): number
  write(addr: number, value: number): void
}

export class Bus implements CpuAddressSpace {
  private readonly ram = new Uint8Array(0x800)
  // Simple PRG-RAM window at $6000-$7FFF (8KB). Many mappers provide this; even when absent,
  // mapping it helps common test ROMs that report results through PRG-RAM.
  private readonly prgRam = new Uint8Array(0x2000)
  private cart: Cartridge | null = null

  private readonly joypad1 = new Joypad()
  private readonly joypad2 = new Joypad()

  public constructor(
    private readonly apu: ApuStub,
    private readonly ppu: PpuStub,
  ) {}

  public setCartridge(cart: Cartridge | null): void {
    this.cart = cart
  }

  public setJoypad1Button(button: JoypadButton, pressed: boolean): void {
    this.joypad1.setButton(button, pressed)
  }

  public setJoypad2Button(button: JoypadButton, pressed: boolean): void {
    this.joypad2.setButton(button, pressed)
  }

  public getJoypad1DebugInfo(): { readonly buttons: number; readonly strobe: boolean; readonly shift: number } {
    return this.joypad1.getDebugInfo()
  }

  public getJoypad2DebugInfo(): { readonly buttons: number; readonly strobe: boolean; readonly shift: number } {
    return this.joypad2.getDebugInfo()
  }

  public peek(addr: number): number {
    const a = addr & 0xffff

    if (a <= 0x1fff) {
      return this.ram[a & 0x7ff] ?? 0
    }

    // PPU regs ($2000-$3FFF) mirrored every 8 bytes.
    if (a >= 0x2000 && a <= 0x3fff) {
      return this.ppu.peekRegister(0x2000 | (a & 0x0007))
    }

    if (a >= 0x4000 && a <= 0x4017) {
      // APU stub read has no meaningful side effects for now.
      if (a === 0x4016) return this.joypad1.peek()
      if (a === 0x4017) return this.joypad2.peek()
      return this.apu.readRegister(a)
    }

    if (a >= 0x6000 && a <= 0x7fff) {
      return this.prgRam[a & 0x1fff] ?? 0
    }

    if (a >= 0x8000) {
      const v = this.cart?.cpuRead(a)
      if (v !== null && v !== undefined) return v & 0xff
    }

    return 0
  }

  public read(addr: number): number {
    const a = addr & 0xffff

    if (a <= 0x1fff) {
      return this.ram[a & 0x7ff] ?? 0
    }

    // PPU regs ($2000-$3FFF) mirrored every 8 bytes.
    if (a >= 0x2000 && a <= 0x3fff) {
      return this.ppu.readRegister(0x2000 | (a & 0x0007))
    }

    // Controllers ($4016/$4017), cartridge ($4020+)

    if (a === 0x4016) {
      return this.joypad1.read()
    }

    if (a === 0x4017) {
      return this.joypad2.read()
    }

    if (a >= 0x4000 && a <= 0x4017) {
      return this.apu.readRegister(a)
    }

    if (a >= 0x6000 && a <= 0x7fff) {
      return this.prgRam[a & 0x1fff] ?? 0
    }

    if (a >= 0x8000) {
      const v = this.cart?.cpuRead(a)
      if (v !== null && v !== undefined) return v & 0xff
    }

    return 0
  }

  public write(addr: number, value: number): void {
    const a = addr & 0xffff
    const v = value & 0xff

    if (a <= 0x1fff) {
      this.ram[a & 0x7ff] = v
      return
    }

    // PPU regs ($2000-$3FFF) mirrored every 8 bytes.
    if (a >= 0x2000 && a <= 0x3fff) {
      this.ppu.writeRegister(0x2000 | (a & 0x0007), v)
      return
    }

    // Controllers ($4016/$4017), cartridge ($4020+)

    if (a === 0x4016) {
      this.joypad1.writeStrobe(v)
      this.joypad2.writeStrobe(v)
      return
    }

    // OAM DMA ($4014): copy 256 bytes from CPU page into PPU OAM.
    if (a === 0x4014) {
      this.ppu.oamDma(v, (x) => this.peek(x))
      return
    }

    if (a >= 0x4000 && a <= 0x4017) {
      this.apu.writeRegister(a, v)
      return
    }

    if (a >= 0x6000 && a <= 0x7fff) {
      this.prgRam[a & 0x1fff] = v
      return
    }

    if (a >= 0x8000) {
      if (this.cart?.cpuWrite(a, v)) return
    }
  }

  public reset(): void {
    this.ram.fill(0)
    this.prgRam.fill(0)
    this.apu.reset()
    this.ppu.reset()
    this.joypad1.reset()
    this.joypad2.reset()
    this.cart?.reset()
  }
}
