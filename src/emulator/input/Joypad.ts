export type JoypadButton = 'A' | 'B' | 'Select' | 'Start' | 'Up' | 'Down' | 'Left' | 'Right'

function buttonBit(button: JoypadButton): number {
  switch (button) {
    case 'A':
      return 0
    case 'B':
      return 1
    case 'Select':
      return 2
    case 'Start':
      return 3
    case 'Up':
      return 4
    case 'Down':
      return 5
    case 'Left':
      return 6
    case 'Right':
      return 7
    default:
      return 0
  }
}

/**
 * NES standard controller (joypad) shift-register interface.
 * - Write $4016 bit0=1 then 0 to latch current button states.
 * - Read $4016 returns bit0 (A, B, Select, Start, Up, Down, Left, Right).
 */
export class Joypad {
  private buttons = 0
  private strobe = false
  private shift = 0

  public setButton(button: JoypadButton, pressed: boolean): void {
    const bit = buttonBit(button)
    if (pressed) {
      this.buttons |= 1 << bit
    } else {
      this.buttons &= ~(1 << bit)
    }
    this.buttons &= 0xff

    if (this.strobe) {
      // While strobe is high, reads reflect current A button state.
      this.shift = this.buttons
    }
  }

  public writeStrobe(value: number): void {
    const next = (value & 1) !== 0
    const prev = this.strobe
    this.strobe = next

    if (this.strobe) {
      // Continuous latch.
      this.shift = this.buttons
    } else if (prev && !next) {
      // Falling edge latches shift register.
      this.shift = this.buttons
    }
  }

  public read(): number {
    // Many implementations return bit0 plus open-bus high bits.
    const bit0 = this.strobe ? (this.buttons & 1) : (this.shift & 1)

    if (!this.strobe) {
      // Shift right, shifting in 1s after 8 reads.
      this.shift = ((this.shift >> 1) | 0x80) & 0xff
    }

    return 0x40 | (bit0 & 1)
  }

  /** Side-effect-free read (does not shift). Useful for tracing/peeking. */
  public peek(): number {
    const bit0 = this.strobe ? (this.buttons & 1) : (this.shift & 1)
    return 0x40 | (bit0 & 1)
  }

  public getDebugInfo(): { readonly buttons: number; readonly strobe: boolean; readonly shift: number } {
    return { buttons: this.buttons & 0xff, strobe: this.strobe, shift: this.shift & 0xff }
  }

  public reset(): void {
    this.buttons = 0
    this.strobe = false
    this.shift = 0
  }
}
