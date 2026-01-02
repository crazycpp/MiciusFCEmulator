<script setup lang="ts">
import { onBeforeUnmount, onMounted, ref } from 'vue'
import { Emulator } from './emulator/Emulator'
import { OPCODES } from './emulator/cpu/OpcodeTable'
import { WebAudioWorkletSink } from './platform/audio/WebAudioWorkletSink'
import { Webgl2VideoSink } from './platform/video/Webgl2VideoSink'
import type { JoypadButton } from './emulator/input/Joypad'

const canvasRef = ref<HTMLCanvasElement | null>(null)
const emulator = new Emulator()
const video = new Webgl2VideoSink()
const audio = new WebAudioWorkletSink()

const traceLines = ref<string[]>([])
const traceEnabled = ref(false)
const runState = ref(emulator.runState)
let lastTraceVersion = emulator.trace.version
let uiRafId: number | null = null
let lastTraceUiUpdateMs = 0

const TRACE_VIEW_LINES = 500

const romInputRef = ref<HTMLInputElement | null>(null)

const referenceText = ref('')
const compareResult = ref('')

const ppuDebugText = ref('')

let joypadKeyHandlers: {
  onKeyDown: (e: KeyboardEvent) => void
  onKeyUp: (e: KeyboardEvent) => void
} | null = null

emulator.setCpuTraceEnabled(traceEnabled.value)

async function ensureAudioStarted(): Promise<void> {
  if (!audio.started) {
    await audio.ensureStarted()
  }
  // Re-apply sample rate every time because emulator.reset()/loadRom() resets APU state.
  emulator.setAudioSampleRate(audio.sampleRate)
}

async function handleLoadRomClick(): Promise<void> {
  romInputRef.value?.click()
}

async function handleRomSelected(event: Event): Promise<void> {
  const input = event.target as HTMLInputElement
  const file = input.files?.[0]
  if (!file) return

  const buf = await file.arrayBuffer()
  emulator.loadRom(buf)
  // A file picker is a user gesture in most browsers, so we can also arm audio here.
  // If the browser blocks it, audio will start on Run/Step instead.
  try {
    await ensureAudioStarted()
  } catch {
    // ignore
  }
  presentOnce()

  // Reset the input so selecting the same file again triggers change.
  input.value = ''
}

function handleRunPause(): void {
  if (emulator.runState === 'running') {
    emulator.pause()
  } else {
    void ensureAudioStarted()
    emulator.start()
  }

  runState.value = emulator.runState
}

function handleReset(): void {
  emulator.reset()
  void ensureAudioStarted()
  presentOnce()
  runState.value = emulator.runState
}

function handleTraceToggle(): void {
  emulator.setCpuTraceEnabled(traceEnabled.value)
  lastTraceVersion = emulator.trace.version
  traceLines.value = traceEnabled.value ? emulator.trace.snapshot().slice(-TRACE_VIEW_LINES) : []
}

function handleStepFrame(): void {
  void ensureAudioStarted()
  emulator.stepFrame()
  presentOnce()
  runState.value = emulator.runState
}

function handleStepInstruction(): void {
  void ensureAudioStarted()
  emulator.stepInstruction()
  presentOnce()
  runState.value = emulator.runState
}

function normalizeLine(line: string): string {
  return line.replace(/\r/g, '').replace(/\s+/g, ' ').trim()
}

function filterComparableLines(lines: string[]): string[] {
  return lines
    .map((l) => l.replace(/\r/g, ''))
    .filter((l) => {
      const t = l.trimStart()
      return /^[0-9A-Fa-f]{4}\s/.test(t)
    })
}

function tryParseOpcodeByte(line: string): number | null {
  const m = /^([0-9A-Fa-f]{4})\s{2}([0-9A-Fa-f]{2})\b/.exec(line)
  if (!m) return null
  const opcodeHex = m[2]
  if (!opcodeHex) return null
  const opcode = Number.parseInt(opcodeHex, 16)
  return Number.isFinite(opcode) ? opcode : null
}

function handleScanMissingOpcodes(): void {
  const expected = filterComparableLines(referenceText.value.split('\n'))
  if (expected.length === 0) {
    compareResult.value = 'Paste some nestest.log lines first.'
    return
  }

  const counts = new Map<number, number>()
  for (const line of expected) {
    const opcode = tryParseOpcodeByte(line)
    if (opcode === null) continue
    counts.set(opcode, (counts.get(opcode) ?? 0) + 1)
  }

  const missing: Array<{ opcode: number; count: number }> = []
  for (const [opcode, count] of counts) {
    const info = OPCODES[opcode]
    if (!info || info.mnemonic === '???') {
      missing.push({ opcode, count })
    }
  }

  missing.sort((a, b) => a.opcode - b.opcode)

  if (missing.length === 0) {
    compareResult.value = `No missing opcode definitions found in pasted log (${expected.length} lines).`
    return
  }

  const lines = missing.map((x) => `0x${x.opcode.toString(16).toUpperCase().padStart(2, '0')}  (count=${x.count})`)
  compareResult.value = [
    `Missing opcode definitions in OpcodeTable (would disasm as ???): ${missing.length}`,
    ...lines,
  ].join('\n')
}

function handleCompareTrace(): void {
  const actual = filterComparableLines(emulator.trace.snapshot())
  const expected = filterComparableLines(referenceText.value.split('\n'))

  if (expected.length === 0) {
    compareResult.value = 'Paste some nestest.log lines first.'
    return
  }
  if (actual.length === 0) {
    compareResult.value = 'Trace is empty. Load ROM and step/run first.'
    return
  }

  const n = Math.min(actual.length, expected.length)
  for (let i = 0; i < n; i++) {
    const a = normalizeLine(actual[i] ?? '')
    const e = normalizeLine(expected[i] ?? '')
    if (a !== e) {
      compareResult.value = [
        `Mismatch at line ${i + 1}:`,
        `EXPECTED: ${expected[i]}`,
        `ACTUAL:   ${actual[i]}`,
      ].join('\n')
      return
    }
  }

  if (expected.length > actual.length) {
    compareResult.value = `Actual is a matching prefix (${n} lines). Need more trace: expected=${expected.length}, actual=${actual.length}.`
    return
  }

  if (actual.length > expected.length) {
    compareResult.value = `Expected is a matching prefix (${n} lines). Actual has extra lines: expected=${expected.length}, actual=${actual.length}.`
    return
  }

  compareResult.value = `All ${n} lines match (normalized) and line counts match.`
}

function handleClearReference(): void {
  referenceText.value = ''
  compareResult.value = ''
}

function presentOnce(): void {
  video.present(emulator.frameBuffer)
}

function uiTick(): void {
  // Keep WebGL presenting even if emulator runs internally.
  presentOnce()

  // Drain generated audio and push to the worklet (dual-mono stereo).
  if (audio.started) {
    const samples = emulator.drainAudioSamplesInterleaved(4096)
    if (samples.length > 0) {
      audio.pushInterleaved(samples)
    }
  }

  const ppu = emulator.getPpuDebugInfo()
  const cart = emulator.getCartDebugInfo()
  const test = emulator.getTestRomStatus()
  const joy = emulator.getJoypad1DebugInfo()
  const joy2 = emulator.getJoypad2DebugInfo()
  const spritesOn = (ppu.ppumask & 0x10) !== 0
  const bgOn = (ppu.ppumask & 0x08) !== 0
  ppuDebugText.value = [
    cart.loaded
      ? `CART mapper=${cart.mapperNumber} mirroring=${cart.mirroring} PRG=${cart.prgRomSizeBytes} CHR=${cart.chrRomSizeBytes} chrIsRam=${cart.chrIsRam ? '1' : '0'}`
      : 'CART (none)',
    `PPUCTRL=${ppu.ppuctrl.toString(16).toUpperCase().padStart(2, '0')}  PPUMASK=${ppu.ppumask.toString(16).toUpperCase().padStart(2, '0')}  PPUSTATUS=${ppu.ppustatus.toString(16).toUpperCase().padStart(2, '0')}`,
    `BG=${bgOn ? 'on' : 'off'}  SPR=${spritesOn ? 'on' : 'off'}  OAMADDR=${ppu.oamaddr.toString(16).toUpperCase().padStart(2, '0')}`,
    `OAMDMA count=${ppu.oamDmaCount}  lastPage=${ppu.lastOamDmaPage.toString(16).toUpperCase().padStart(2, '0')}  lastNonZero=${ppu.lastOamDmaNonZero}`,
    `SPR visible=${ppu.lastVisibleSpriteCount}  drawnSprites=${ppu.lastSpriteDrawnCount}  drawnPixels=${ppu.lastSpritePixelsDrawn}  spr0Hit=${ppu.lastSprite0Hit ? '1' : '0'}`,
    `SCROLL start x=${ppu.frameStartScrollX} y=${ppu.frameStartScrollY} nt=${ppu.frameStartScrollNt} | end x=${ppu.scrollX} y=${ppu.scrollY} nt=${ppu.scrollNt}`,
    `PPU writes: $2000=${ppu.frameWrite2000} $2005=${ppu.frameWrite2005} $2006=${ppu.frameWrite2006} $2007=${ppu.frameWrite2007} | last $2000=${ppu.lastWrite2000.toString(16).toUpperCase().padStart(2, '0')} $2005=[${ppu.lastWrite2005_1
      .toString(16)
      .toUpperCase()
      .padStart(2, '0')},${ppu.lastWrite2005_2.toString(16).toUpperCase().padStart(2, '0')}] NMI=${ppu.nmiCount} frame=${ppu.frameId}`,
    `PPU max $2005 this frame: [${ppu.frameMaxWrite2005_1.toString(16).toUpperCase().padStart(2, '0')},${ppu.frameMaxWrite2005_2
      .toString(16)
      .toUpperCase()
      .padStart(2, '0')}]`,
    `PPU $2005 pairs: n=${ppu.frame2005PairCount} first=[${ppu.frameFirst2005_1.toString(16).toUpperCase().padStart(2, '0')},${ppu.frameFirst2005_2
      .toString(16)
      .toUpperCase()
      .padStart(2, '0')}] last=[${ppu.frameLast2005_1.toString(16).toUpperCase().padStart(2, '0')},${ppu.frameLast2005_2
      .toString(16)
      .toUpperCase()
      .padStart(2, '0')}] lastNZ=[${ppu.frameLastNonZero2005_1.toString(16).toUpperCase().padStart(2, '0')},${ppu.frameLastNonZero2005_2
      .toString(16)
      .toUpperCase()
      .padStart(2, '0')}]`,
    `PPU loopy: v=${ppu.v.toString(16).toUpperCase().padStart(4, '0')} t=${ppu.t
      .toString(16)
      .toUpperCase()
      .padStart(4, '0')} x=${ppu.xFine} w=${ppu.wLatch}`,
    test.status !== 0 ? `TEST $6000=${test.status} ${test.message ? `| ${test.message}` : ''}` : 'TEST $6000=0',
    `JOY1 buttons=${joy.buttons.toString(16).toUpperCase().padStart(2, '0')} strobe=${joy.strobe ? '1' : '0'} shift=${joy.shift.toString(16).toUpperCase().padStart(2, '0')}`,
    `JOY2 buttons=${joy2.buttons.toString(16).toUpperCase().padStart(2, '0')} strobe=${joy2.strobe ? '1' : '0'} shift=${joy2.shift.toString(16).toUpperCase().padStart(2, '0')}`,
    ppu.oamPreview ? `OAM preview:\n${ppu.oamPreview}` : 'OAM preview: (none)',
  ].join('\n')

  if (traceEnabled.value) {
    const now = performance.now()
    if (now - lastTraceUiUpdateMs >= 100 && emulator.trace.version !== lastTraceVersion) {
      lastTraceVersion = emulator.trace.version
      traceLines.value = emulator.trace.snapshot().slice(-TRACE_VIEW_LINES)
      lastTraceUiUpdateMs = now
    }
  }

  runState.value = emulator.runState

  uiRafId = requestAnimationFrame(uiTick)
}

onMounted(() => {
  const canvas = canvasRef.value
  if (!canvas) return
  video.initialize(canvas)
  emulator.reset()
  presentOnce()

  // Start a UI refresh loop so class-based emulator state is visible in Vue.
  uiRafId = requestAnimationFrame(uiTick)

  const keyToButton = (code: string): JoypadButton | null => {
    switch (code) {
      case 'ArrowUp':
        return 'Up'
      case 'ArrowDown':
        return 'Down'
      case 'ArrowLeft':
        return 'Left'
      case 'ArrowRight':
        return 'Right'
      case 'KeyZ':
        return 'A'
      case 'KeyX':
        return 'B'
      case 'Enter':
      case 'NumpadEnter':
      case 'Space':
        return 'Start'
      case 'ShiftLeft':
      case 'ShiftRight':
        return 'Select'
      default:
        return null
    }
  }

  const onKeyDown = (e: KeyboardEvent): void => {
    const b = keyToButton(e.code)
    if (!b) return
    emulator.setJoypad1Button(b, true)
    e.preventDefault()
  }

  const onKeyUp = (e: KeyboardEvent): void => {
    const b = keyToButton(e.code)
    if (!b) return
    emulator.setJoypad1Button(b, false)
    e.preventDefault()
  }

  window.addEventListener('keydown', onKeyDown)
  window.addEventListener('keyup', onKeyUp)

  joypadKeyHandlers = { onKeyDown, onKeyUp }
})

onBeforeUnmount(() => {
  emulator.pause()
  if (uiRafId !== null) {
    cancelAnimationFrame(uiRafId)
    uiRafId = null
  }
  video.dispose()
  void audio.dispose()

  if (joypadKeyHandlers) {
    window.removeEventListener('keydown', joypadKeyHandlers.onKeyDown)
    window.removeEventListener('keyup', joypadKeyHandlers.onKeyUp)
    joypadKeyHandlers = null
  }
})
</script>

<template>
  <div class="app">
    <div class="left">
      <canvas
        ref="canvasRef"
        class="screen"
      />
    </div>

    <div class="right">
      <div class="controls">
        <input
          ref="romInputRef"
          type="file"
          accept=".nes"
          class="hiddenFile"
          @change="handleRomSelected"
        >
        <button
          type="button"
          @click="handleLoadRomClick"
        >
          Load ROM
        </button>
        <button
          type="button"
          @click="handleRunPause"
        >
          {{ runState === 'running' ? 'Pause' : 'Run' }}
        </button>
        <button
          type="button"
          @click="handleStepInstruction"
        >
          Step instr
        </button>
        <button
          type="button"
          @click="handleStepFrame"
        >
          Step frame
        </button>
        <button
          type="button"
          @click="handleReset"
        >
          Reset
        </button>
        <label class="checkbox">
          <input
            v-model="traceEnabled"
            type="checkbox"
            @change="handleTraceToggle"
          >
          Trace
        </label>
      </div>

      <div class="panel">
        <div class="panelTitle">
          Compare (paste nestest.log lines)
        </div>
        <div class="compareBody">
          <textarea
            v-model="referenceText"
            class="refInput"
            spellcheck="false"
            placeholder="Paste nestest.log lines here (e.g. starting at C000 ...)"
          />
          <div class="compareControls">
            <button
              type="button"
              @click="handleCompareTrace"
            >
              Compare
            </button>
            <button
              type="button"
              @click="handleScanMissingOpcodes"
            >
              Scan missing opcodes
            </button>
            <button
              type="button"
              @click="handleClearReference"
            >
              Clear
            </button>
          </div>
          <pre class="compareResult">{{ compareResult }}</pre>
        </div>
      </div>

      <div class="panel">
        <div class="panelTitle">
          Trace
        </div>
        <pre class="compareResult">{{ ppuDebugText }}</pre>
        <pre class="trace">{{ traceLines.join('\n') }}</pre>
      </div>
    </div>
  </div>
</template>

<style scoped>
.app {
  display: grid;
  grid-template-columns: 1fr 420px;
  gap: 16px;
  height: 100vh;
  padding: 16px;
  box-sizing: border-box;
}

.left {
  display: flex;
  align-items: center;
  justify-content: center;
  min-width: 0;
}

.screen {
  width: 100%;
  height: 100%;
  max-width: 960px;
  max-height: 900px;
  background: #000;
}

.right {
  display: grid;
  grid-template-rows: auto auto 1fr;
  gap: 12px;
  min-width: 0;
}

.controls {
  display: flex;
  gap: 8px;
  flex-wrap: wrap;
}

.controls button {
  padding: 8px 10px;
}

.checkbox {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  padding: 8px 6px;
  user-select: none;
}

.hiddenFile {
  display: none;
}

.panel {
  border: 1px solid rgba(255, 255, 255, 0.15);
  border-radius: 8px;
  overflow: hidden;
  min-height: 0;
}

.panelTitle {
  padding: 8px 10px;
  border-bottom: 1px solid rgba(255, 255, 255, 0.15);
}

.trace {
  margin: 0;
  padding: 10px;
  height: 100%;
  overflow: auto;
  white-space: pre;
  font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, 'Liberation Mono', 'Courier New', monospace;
  font-size: 12px;
}

.compareBody {
  padding: 10px;
  display: grid;
  gap: 8px;
}

.refInput {
  width: 100%;
  min-height: 120px;
  resize: vertical;
  font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, 'Liberation Mono', 'Courier New', monospace;
  font-size: 12px;
}

.compareControls {
  display: flex;
  gap: 8px;
  flex-wrap: wrap;
}

.compareResult {
  margin: 0;
  white-space: pre-wrap;
  font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, 'Liberation Mono', 'Courier New', monospace;
  font-size: 12px;
}
</style>
