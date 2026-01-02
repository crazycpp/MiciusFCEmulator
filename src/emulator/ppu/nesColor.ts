export interface Rgb {
  readonly r: number
  readonly g: number
  readonly b: number
}

function clamp01(x: number): number {
  if (x < 0) return 0
  if (x > 1) return 1
  return x
}

function to8bit(x01: number): number {
  return Math.round(clamp01(x01) * 255) & 0xff
}

/**
 * Deterministic, dependency-free approximation of the 2C02 (NTSC) palette.
 *
 * This simulates the PPU's composite waveform generation and a simple YUV decode,
 * producing a reasonable looking palette without embedding a third-party LUT.
 */
function buildNtscPalette(): ReadonlyArray<Rgb> {
  // Terminated voltage levels from nesdev (see NTSC video page).
  // Indexing matches the reference pseudo-code: low/high levels + optional attenuation.
  const levels = [
    0.228, 0.312, 0.552, 0.88, // low
    0.616, 0.84, 1.1, 1.1, // high
    0.192, 0.256, 0.448, 0.712, // low (attenuated)
    0.5, 0.676, 0.896, 0.896, // high (attenuated)
  ] as const

  // Normalization points (signal -> [0,1]).
  const black = 0.312
  const white = 1.1

  // Saturation tweak: 1.0 is "reference"; >1 increases chroma strength.
  const saturation = 1.3

  // Hue tweak (in "sample" units, where 12 samples = 360 degrees).
  // Negative values rotate violet-ish hues toward blue.
  const huePhaseOffset = 0.0

  const out: Rgb[] = new Array<Rgb>(64)

  for (let idx = 0; idx < 64; idx++) {
    const c = idx & 0x3f

    // $xD is "blacker than black" on 2C02 composite; treat as black.
    if ((c & 0x0f) === 0x0d) {
      out[idx] = { r: 0, g: 0, b: 0 }
      continue
    }

    const color = c & 0x0f // 0..15
    let level = (c >> 4) & 0x03 // 0..3
    const emphasis = 0 // ignored for now

    // For colors 14..15, force level 1.
    if (color > 13) level = 1

    const inColorPhase = (phase: number): boolean => ((color + phase) % 12) < 6

    // Emphasis attenuation (ignored by default).
    const attenuation =
      (((emphasis & 1) && ((0x0c + 0) % 12) < 6) ||
        ((emphasis & 2) && ((0x04 + 0) % 12) < 6) ||
        ((emphasis & 4) && ((0x08 + 0) % 12) < 6)) &&
      color < 0x0e
        ? 8
        : 0

    let low = levels[0 + level + attenuation] ?? black
    let high = levels[4 + level + attenuation] ?? white
    if (color === 0) low = high
    if (color > 12) high = low

    // Generate 12 samples (one chroma cycle) and decode to YUV.
    let y = 0
    let u = 0
    let v = 0

    // Reference phase offset for this scanline/pixel.
    // We keep it constant for the flat-color palette generation.
    const basePhase = huePhaseOffset

    for (let p = 0; p < 12; p++) {
      const signal = inColorPhase(p) ? high : low
      const s = clamp01((signal - black) / (white - black))

      const level01 = s / 12
      y += level01

      const angle = (Math.PI * (basePhase + p + 3 - 0.5)) / 6
      u += level01 * Math.sin(angle) * 2
      v += level01 * Math.cos(angle) * 2
    }

    u *= saturation
    v *= saturation

    // YUV -> RGB (SMPTE 170M-ish constants, as shown on nesdev).
    const r = y + 1.139883 * v
    const g = y - 0.394642 * u - 0.580622 * v
    const b = y + 2.032062 * u

    out[idx] = {
      r: to8bit(r),
      g: to8bit(g),
      b: to8bit(b),
    }
  }

  return out
}

const NTSC_PALETTE = buildNtscPalette()

export function nesColorIndexToRgb(colorIndex: number): Rgb {
  return NTSC_PALETTE[colorIndex & 0x3f] ?? { r: 0, g: 0, b: 0 }
}
