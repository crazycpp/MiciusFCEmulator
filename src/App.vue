<script setup lang="ts">
import { onBeforeUnmount, onMounted, ref } from 'vue'
import { Emulator } from './emulator/Emulator'
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

const ppuDebugText = ref('')

type KeymapOverrides = Partial<Record<JoypadButton, string>>
const KEYMAP_STORAGE_KEY = 'miciusfc.keymap.v1'

const keymapOverrides = ref<KeymapOverrides>({})
const capturingButton = ref<JoypadButton | null>(null)
const keymapExpanded = ref(false)

const bottomExpanded = ref(true)

function loadKeymapOverrides(): void {
  try {
    const raw = localStorage.getItem(KEYMAP_STORAGE_KEY)
    if (!raw) return
    const obj = JSON.parse(raw) as unknown
    if (!obj || typeof obj !== 'object') return
    const o = obj as Record<string, unknown>
    const next: KeymapOverrides = {}
    const buttons: JoypadButton[] = ['Up', 'Down', 'Left', 'Right', 'A', 'B', 'Select', 'Start']
    for (const b of buttons) {
      const v = o[b]
      if (typeof v === 'string' && v.length > 0) next[b] = v
    }
    keymapOverrides.value = next
  } catch {
    // ignore
  }
}

function saveKeymapOverrides(): void {
  try {
    localStorage.setItem(KEYMAP_STORAGE_KEY, JSON.stringify(keymapOverrides.value))
  } catch {
    // ignore
  }
}

function clearKeymapOverrides(): void {
  keymapOverrides.value = {}
  saveKeymapOverrides()
}

function setOverride(button: JoypadButton, code: string): void {
  keymapOverrides.value = { ...keymapOverrides.value, [button]: code }
  saveKeymapOverrides()
}

function clearOverride(button: JoypadButton): void {
  const next = { ...keymapOverrides.value }
  delete next[button]
  keymapOverrides.value = next
  saveKeymapOverrides()
}

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

async function tryStartAudioFromGesture(): Promise<void> {
  try {
    await ensureAudioStarted()
  } catch (e) {
    // Surface the reason in console; without this, Pages issues look like “silent but no error”.
    console.warn('[audio] start failed:', e)
  }
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
  } catch (e) {
    console.warn('[audio] start failed:', e)
  }
  presentOnce()

  // Reset the input so selecting the same file again triggers change.
  input.value = ''
}

async function handleRunPause(): Promise<void> {
  if (emulator.runState === 'running') {
    emulator.pause()
    runState.value = emulator.runState
    return
  }

  try {
    await ensureAudioStarted()
  } catch (e) {
    // If the browser blocks audio start, still allow running.
    console.warn('[audio] start failed:', e)
  }
  emulator.start()
  runState.value = emulator.runState
}

async function handleReset(): Promise<void> {
  emulator.reset()
  try {
    await ensureAudioStarted()
  } catch (e) {
    console.warn('[audio] start failed:', e)
  }
  presentOnce()
  runState.value = emulator.runState
}

function handleTraceToggle(): void {
  emulator.setCpuTraceEnabled(traceEnabled.value)
  lastTraceVersion = emulator.trace.version
  traceLines.value = traceEnabled.value ? emulator.trace.snapshot().slice(-TRACE_VIEW_LINES) : []
}

async function handleStepFrame(): Promise<void> {
  try {
    await ensureAudioStarted()
  } catch (e) {
    console.warn('[audio] start failed:', e)
  }
  emulator.stepFrame()
  presentOnce()
  runState.value = emulator.runState
}

async function handleStepInstruction(): Promise<void> {
  try {
    await ensureAudioStarted()
  } catch (e) {
    console.warn('[audio] start failed:', e)
  }
  emulator.stepInstruction()
  presentOnce()
  runState.value = emulator.runState
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
  const cpu = emulator.getCpuDebugInfo()
  const mapperState = cart.loaded ? emulator.getMapperDebugState() : null
  const test = emulator.getTestRomStatus()
  const joy = emulator.getJoypad1DebugInfo()
  const joy2 = emulator.getJoypad2DebugInfo()
  const spritesOn = (ppu.ppumask & 0x10) !== 0
  const bgOn = (ppu.ppumask & 0x08) !== 0
  ppuDebugText.value = [
    `AUDIO started=${audio.started ? '1' : '0'} state=${audio.state} rate=${audio.sampleRate ?? 0} pushedFrames=${audio.totalPushedFrames} err=${audio.lastError ?? ''}`,
    cart.loaded
      ? `CART mapper=${cart.mapperNumber} mirroring=${cart.mirroring} PRG=${cart.prgRomSizeBytes} CHR=${cart.chrRomSizeBytes} chrIsRam=${cart.chrIsRam ? '1' : '0'}`
      : cart.lastHeader
        ? `CART (load failed) mapper=${cart.lastHeader.mapperNumber} mirroring=${cart.lastHeader.mirroring} PRG=${cart.lastHeader.prgRomSizeBytes} CHR=${cart.lastHeader.chrRomSizeBytes} trainer=${cart.lastHeader.hasTrainer ? '1' : '0'} nes2=${cart.lastHeader.isNes2_0 ? '1' : '0'} | error=${cart.lastLoadError ?? '(unknown)'}`
        : `CART (none)${cart.lastLoadError ? ' | error=' + cart.lastLoadError : ''}`,
    `PPUCTRL=${ppu.ppuctrl.toString(16).toUpperCase().padStart(2, '0')}  PPUMASK=${ppu.ppumask.toString(16).toUpperCase().padStart(2, '0')}  PPUSTATUS=${ppu.ppustatus.toString(16).toUpperCase().padStart(2, '0')}`,
    `CPU PC=${cpu.pc.toString(16).toUpperCase().padStart(4, '0')} A=${cpu.a.toString(16).toUpperCase().padStart(2, '0')} X=${cpu.x.toString(16).toUpperCase().padStart(2, '0')} Y=${cpu.y.toString(16).toUpperCase().padStart(2, '0')} P=${cpu.p.toString(16).toUpperCase().padStart(2, '0')} SP=${cpu.sp.toString(16).toUpperCase().padStart(2, '0')} | IRQsvc=${cpu.irqServiceCount} NMIsvc=${cpu.nmiServiceCount} IRQline=${cpu.irqLine ? '1' : '0'}`,
    cart.loaded && cart.mapperNumber === 1 && mapperState
      ? (() => {
          const s = mapperState as any
          const ctl = (s.control ?? 0) & 0x1f
          const chrMode = (ctl >> 4) & 1
          const prgMode = (ctl >> 2) & 3
          const mm = ctl & 3
          const mmName = mm === 0 ? '1sc0' : mm === 1 ? '1sc1' : mm === 2 ? 'V' : 'H'
          const chr0 = ((s.chrBank0 ?? 0) & 0x1f).toString(16).toUpperCase().padStart(2, '0')
          const chr1 = ((s.chrBank1 ?? 0) & 0x1f).toString(16).toUpperCase().padStart(2, '0')
          const prg = ((s.prgBank ?? 0) & 0x1f).toString(16).toUpperCase().padStart(2, '0')
          return `M1 ctl=${ctl.toString(16).toUpperCase().padStart(2, '0')} mm=${mmName} chrMode=${chrMode} prgMode=${prgMode} | chr0=${chr0} chr1=${chr1} prg=${prg}`
        })()
      : '',
    cart.loaded && cart.mapperNumber === 67 && mapperState
      ? (() => {
          const s = mapperState as any
          const cnt = (s.irqCounter ?? 0) & 0xffff
          const chr = (s.chrBank2k instanceof Uint8Array)
            ? Array.from(s.chrBank2k as Uint8Array).map((b) => (b & 0xff).toString(16).toUpperCase().padStart(2, '0')).join(' ')
            : '(?)'
          return `M67 prg=${((s.prgBank16k ?? 0) & 0xff).toString(16).toUpperCase().padStart(2, '0')} chr2k=[${chr}] mm=${cart.mirroring} | irqEn=${s.irqEnabled ? '1' : '0'} pend=${s.irqPending ? '1' : '0'} cnt=${cnt.toString(16).toUpperCase().padStart(4, '0')} fire=${s.irqFireCount ?? 0} ack=${s.irqAckCount ?? 0} | W:D8=${((s.lastWriteD800 ?? 0) & 0xff).toString(16).toUpperCase().padStart(2, '0')} C8=${((s.lastWriteC800 ?? 0) & 0xff).toString(16).toUpperCase().padStart(2, '0')} 80=${((s.lastWrite8000 ?? 0) & 0xff).toString(16).toUpperCase().padStart(2, '0')} E8=${((s.lastWriteE800 ?? 0) & 0xff).toString(16).toUpperCase().padStart(2, '0')} F8=${((s.lastWriteF800 ?? 0) & 0xff).toString(16).toUpperCase().padStart(2, '0')}`
        })()
      : '',
    `RENDER PPUCTRL=${ppu.lastRenderedPpuctrl.toString(16).toUpperCase().padStart(2, '0')}  PPUMASK=${ppu.lastRenderedPpumask.toString(16).toUpperCase().padStart(2, '0')}  CHR nz: lo4k=${ppu.chrNonZeroLo4k} hi4k=${ppu.chrNonZeroHi4k}`,
    `BG=${bgOn ? 'on' : 'off'}  SPR=${spritesOn ? 'on' : 'off'}  OAMADDR=${ppu.oamaddr.toString(16).toUpperCase().padStart(2, '0')}`,
    `OAMDMA count=${ppu.oamDmaCount}  lastPage=${ppu.lastOamDmaPage.toString(16).toUpperCase().padStart(2, '0')}  lastNonZero=${ppu.lastOamDmaNonZero}`,
    `OAM hidden(Y=EF) count=${ppu.oamHiddenEfCount}`,
    ppu.oamRawHead ? `OAM head (first 8 sprites):\n${ppu.oamRawHead}` : 'OAM head (first 8 sprites): (none)',
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

  // Fallback: a single user gesture anywhere can unlock audio on restrictive origins.
  // This helps GitHub Pages where the first start attempt might not be considered user-activated.
  const onFirstPointerDown = (): void => {
    void tryStartAudioFromGesture()
    window.removeEventListener('pointerdown', onFirstPointerDown, { capture: true } as any)
  }
  window.addEventListener('pointerdown', onFirstPointerDown, { capture: true, once: true })

  loadKeymapOverrides()

  // Default hardcoded mapping (must remain):
  // Arrows = D-pad, Z = A, X = B, Enter/NumpadEnter/Space = Start, Shift = Select.
  const defaultKeyToButton = (code: string): JoypadButton | null => {
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

  const overrideKeyToButton = (code: string): JoypadButton | null => {
    // Overrides are stored as button->code; resolve by scanning.
    for (const [k, v] of Object.entries(keymapOverrides.value)) {
      if (v === code) return k as JoypadButton
    }
    return null
  }

  const onKeyDown = (e: KeyboardEvent): void => {
    // Key capture for remapping (top bar).
    if (capturingButton.value) {
      if (e.code === 'Escape') {
        capturingButton.value = null
        e.preventDefault()
        return
      }
      setOverride(capturingButton.value, e.code)
      capturingButton.value = null
      e.preventDefault()
      return
    }

    const b = overrideKeyToButton(e.code) ?? defaultKeyToButton(e.code)
    if (!b) return
    emulator.setJoypad1Button(b, true)
    e.preventDefault()
  }

  const onKeyUp = (e: KeyboardEvent): void => {
    // While capturing, ignore keyup.
    if (capturingButton.value) {
      e.preventDefault()
      return
    }

    const b = overrideKeyToButton(e.code) ?? defaultKeyToButton(e.code)
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
  <div class="layout">
    <!-- Top: first row controls -->
    <header class="top">
      <input
        ref="romInputRef"
        type="file"
        accept=".nes"
        class="hiddenFile"
        @change="handleRomSelected"
      >

      <div class="topLeft">
        <button type="button" @click="handleLoadRomClick">加载 ROM</button>
        <button type="button" @click="handleRunPause">{{ runState === 'running' ? '暂停' : '运行' }}</button>
        <button type="button" @click="handleReset">重置</button>
      </div>

      <div class="topRight">
        <div class="mappingTitleRow">
          <div class="mappingTitle">
            按键映射（默认硬编码仍有效；自定义为额外按键）
          </div>
          <button
            type="button"
            class="mini"
            @click="keymapExpanded = !keymapExpanded"
          >
            {{ keymapExpanded ? '收起' : '展开' }}
          </button>
        </div>

        <div v-show="keymapExpanded" class="mappingGrid">
          <div class="mappingRow">
            <span class="mappingLabel">↑</span>
            <span class="mappingValue">默认：ArrowUp；自定义：{{ keymapOverrides.Up ?? '-' }}</span>
            <span class="mappingActions">
              <button type="button" class="mini" @click="capturingButton = 'Up'">设置</button>
              <button type="button" class="mini" @click="clearOverride('Up')">清除</button>
            </span>
          </div>

          <div class="mappingRow">
            <span class="mappingLabel">↓</span>
            <span class="mappingValue">默认：ArrowDown；自定义：{{ keymapOverrides.Down ?? '-' }}</span>
            <span class="mappingActions">
              <button type="button" class="mini" @click="capturingButton = 'Down'">设置</button>
              <button type="button" class="mini" @click="clearOverride('Down')">清除</button>
            </span>
          </div>

          <div class="mappingRow">
            <span class="mappingLabel">←</span>
            <span class="mappingValue">默认：ArrowLeft；自定义：{{ keymapOverrides.Left ?? '-' }}</span>
            <span class="mappingActions">
              <button type="button" class="mini" @click="capturingButton = 'Left'">设置</button>
              <button type="button" class="mini" @click="clearOverride('Left')">清除</button>
            </span>
          </div>

          <div class="mappingRow">
            <span class="mappingLabel">→</span>
            <span class="mappingValue">默认：ArrowRight；自定义：{{ keymapOverrides.Right ?? '-' }}</span>
            <span class="mappingActions">
              <button type="button" class="mini" @click="capturingButton = 'Right'">设置</button>
              <button type="button" class="mini" @click="clearOverride('Right')">清除</button>
            </span>
          </div>

          <div class="mappingRow">
            <span class="mappingLabel">A</span>
            <span class="mappingValue">默认：KeyZ；自定义：{{ keymapOverrides.A ?? '-' }}</span>
            <span class="mappingActions">
              <button type="button" class="mini" @click="capturingButton = 'A'">设置</button>
              <button type="button" class="mini" @click="clearOverride('A')">清除</button>
            </span>
          </div>

          <div class="mappingRow">
            <span class="mappingLabel">B</span>
            <span class="mappingValue">默认：KeyX；自定义：{{ keymapOverrides.B ?? '-' }}</span>
            <span class="mappingActions">
              <button type="button" class="mini" @click="capturingButton = 'B'">设置</button>
              <button type="button" class="mini" @click="clearOverride('B')">清除</button>
            </span>
          </div>

          <div class="mappingRow">
            <span class="mappingLabel">选择</span>
            <span class="mappingValue">默认：Shift；自定义：{{ keymapOverrides.Select ?? '-' }}</span>
            <span class="mappingActions">
              <button type="button" class="mini" @click="capturingButton = 'Select'">设置</button>
              <button type="button" class="mini" @click="clearOverride('Select')">清除</button>
            </span>
          </div>

          <div class="mappingRow">
            <span class="mappingLabel">开始</span>
            <span class="mappingValue">默认：Enter/Space；自定义：{{ keymapOverrides.Start ?? '-' }}</span>
            <span class="mappingActions">
              <button type="button" class="mini" @click="capturingButton = 'Start'">设置</button>
              <button type="button" class="mini" @click="clearOverride('Start')">清除</button>
            </span>
          </div>

          <div class="mappingRow">
            <span class="mappingLabel">提示</span>
            <span class="mappingValue">{{ capturingButton ? `请按下要绑定到 ${capturingButton} 的键…(Esc 取消)` : ' ' }}</span>
            <span class="mappingActions">
              <button type="button" class="mini" @click="clearKeymapOverrides">恢复默认</button>
            </span>
          </div>
        </div>

        <div v-show="!keymapExpanded" class="mappingSummary">
          <span>默认：↑↓←→ / Z=A / X=B / Shift=选择 / Enter|Space=开始</span>
          <span v-if="Object.keys(keymapOverrides).length" class="mappingSummaryHint">（已设置自定义）</span>
          <span v-if="capturingButton" class="mappingSummaryHint">（正在捕获：{{ capturingButton }}）</span>
        </div>
      </div>
    </header>

    <!-- Middle: game area with left/center/right -->
    <main class="middle">
      <!-- Left: virtual joystick (D-pad) -->
      <section class="side">
        <div class="touchPad" aria-label="虚拟摇杆">
          <div />
          <button class="padBtn up" type="button" @pointerdown.prevent="emulator.setJoypad1Button('Up', true)" @pointerup.prevent="emulator.setJoypad1Button('Up', false)" @pointercancel.prevent="emulator.setJoypad1Button('Up', false)" @pointerleave.prevent="emulator.setJoypad1Button('Up', false)">↑</button>
          <div />
          <button class="padBtn left" type="button" @pointerdown.prevent="emulator.setJoypad1Button('Left', true)" @pointerup.prevent="emulator.setJoypad1Button('Left', false)" @pointercancel.prevent="emulator.setJoypad1Button('Left', false)" @pointerleave.prevent="emulator.setJoypad1Button('Left', false)">←</button>
          <div class="padCenter" />
          <button class="padBtn right" type="button" @pointerdown.prevent="emulator.setJoypad1Button('Right', true)" @pointerup.prevent="emulator.setJoypad1Button('Right', false)" @pointercancel.prevent="emulator.setJoypad1Button('Right', false)" @pointerleave.prevent="emulator.setJoypad1Button('Right', false)">→</button>
          <div />
          <button class="padBtn down" type="button" @pointerdown.prevent="emulator.setJoypad1Button('Down', true)" @pointerup.prevent="emulator.setJoypad1Button('Down', false)" @pointercancel.prevent="emulator.setJoypad1Button('Down', false)" @pointerleave.prevent="emulator.setJoypad1Button('Down', false)">↓</button>
          <div />
        </div>
      </section>

      <!-- Center: screen + Select/Start under screen -->
      <section class="center">
        <div class="screenFrame">
          <canvas ref="canvasRef" class="screen" />
        </div>
        <div class="belowScreen">
          <button class="actionBtn" type="button" @pointerdown.prevent="emulator.setJoypad1Button('Select', true)" @pointerup.prevent="emulator.setJoypad1Button('Select', false)" @pointercancel.prevent="emulator.setJoypad1Button('Select', false)" @pointerleave.prevent="emulator.setJoypad1Button('Select', false)">选择</button>
          <button class="actionBtn" type="button" @pointerdown.prevent="emulator.setJoypad1Button('Start', true)" @pointerup.prevent="emulator.setJoypad1Button('Start', false)" @pointercancel.prevent="emulator.setJoypad1Button('Start', false)" @pointerleave.prevent="emulator.setJoypad1Button('Start', false)">开始</button>
        </div>
      </section>

      <!-- Right: AB buttons -->
      <section class="side">
        <div class="abPad" aria-label="A/B 按键">
          <button class="abBtn" type="button" @pointerdown.prevent="emulator.setJoypad1Button('A', true)" @pointerup.prevent="emulator.setJoypad1Button('A', false)" @pointercancel.prevent="emulator.setJoypad1Button('A', false)" @pointerleave.prevent="emulator.setJoypad1Button('A', false)">A</button>
          <button class="abBtn" type="button" @pointerdown.prevent="emulator.setJoypad1Button('B', true)" @pointerup.prevent="emulator.setJoypad1Button('B', false)" @pointercancel.prevent="emulator.setJoypad1Button('B', false)" @pointerleave.prevent="emulator.setJoypad1Button('B', false)">B</button>
        </div>
      </section>
    </main>

    <!-- Bottom: debug/tools -->
    <footer class="bottom" :class="{ collapsed: !bottomExpanded }">
      <div class="bottomRow">
        <div class="bottomTitle">调试</div>
        <div class="controls">
          <button type="button" @click="handleStepInstruction">单步指令</button>
          <button type="button" @click="handleStepFrame">单步帧</button>
          <label class="checkbox">
            <input v-model="traceEnabled" type="checkbox" @change="handleTraceToggle">
            Trace
          </label>
        </div>

        <button type="button" class="mini" @click="bottomExpanded = !bottomExpanded">
          {{ bottomExpanded ? '收起' : '展开' }}
        </button>
      </div>

      <div v-show="bottomExpanded" class="panes">
        <div class="panel">
          <div class="panelTitle">Trace</div>
          <pre class="compareResult">{{ ppuDebugText }}</pre>
          <pre class="trace">{{ traceLines.join('\n') }}</pre>
        </div>
      </div>
    </footer>
  </div>
</template>

<style scoped>

.layout {
  display: flex;
  flex-direction: column;
  height: 100vh;
  padding: 12px;
  box-sizing: border-box;
  gap: 12px;
  max-width: 1280px;
  margin: 0 auto;
}

.top {
  display: grid;
  grid-template-columns: auto 1fr;
  gap: 12px;
  align-items: start;
}

.topLeft {
  display: flex;
  gap: 8px;
  flex-wrap: wrap;
}

.topRight {
  border: 1px solid rgba(255, 255, 255, 0.15);
  border-radius: 8px;
  padding: 8px 10px;
}

.mappingTitleRow {
  display: flex;
  gap: 8px;
  align-items: center;
  justify-content: space-between;
  margin-bottom: 6px;
}

.mappingTitle {
  font-size: 12px;
  opacity: 0.9;
}

.mappingGrid {
  display: grid;
  gap: 6px;
}

.mappingSummary {
  font-size: 12px;
  opacity: 0.9;
  line-height: 1.4;
}

.mappingSummaryHint {
  margin-left: 6px;
  opacity: 0.85;
}

.mappingRow {
  display: grid;
  grid-template-columns: 56px 1fr auto;
  gap: 8px;
  align-items: center;
}

.mappingLabel {
  font-weight: 600;
}

.mappingValue {
  font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, 'Liberation Mono', 'Courier New', monospace;
  font-size: 12px;
  opacity: 0.9;
}

.mini {
  padding: 6px 8px;
}

.middle {
  flex: 1;
  min-height: 0;
  display: grid;
  grid-template-columns: 160px 1fr 160px;
  gap: 12px;
}

.side {
  display: flex;
  align-items: center;
  justify-content: center;
  min-width: 0;
}

.center {
  min-width: 0;
  min-height: 0;
  display: grid;
  grid-template-rows: 1fr auto;
  gap: 10px;
  align-items: center;
}

.screenFrame {
  width: 100%;
  height: 100%;
  min-height: 0;
  display: flex;
  align-items: center;
  justify-content: center;
}

.screen {
  width: 100%;
  height: 100%;
  max-width: 960px;
  max-height: 100%;
  background: #000;
  border-radius: 8px;
}

.belowScreen {
  display: flex;
  justify-content: center;
  gap: 12px;
}

.actionBtn {
  min-width: 96px;
  padding: 12px 12px;
  user-select: none;
  touch-action: none;
}

.touchPad {
  width: 140px;
  height: 140px;
  display: grid;
  grid-template-columns: 1fr 1fr 1fr;
  grid-template-rows: 1fr 1fr 1fr;
  gap: 8px;
  touch-action: none;
  user-select: none;
}

.padBtn {
  padding: 0;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 18px;
}

.padCenter {
  border: 1px solid rgba(255, 255, 255, 0.15);
  border-radius: 8px;
}

@media (max-width: 860px) {
  .middle {
    grid-template-columns: 140px 1fr 140px;
  }

  .panes {
    grid-template-columns: 1fr;
  }
}

.abPad {
  display: grid;
  gap: 12px;
  touch-action: none;
  user-select: none;
}

.abBtn {
  width: 120px;
  height: 56px;
  font-size: 18px;
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

.bottom {
  min-height: 220px;
  max-height: 40vh;
  overflow: auto;
  display: grid;
  grid-template-rows: auto 1fr;
  gap: 12px;
}

.bottom.collapsed {
  min-height: 0;
  max-height: none;
  overflow: visible;
  grid-template-rows: auto;
}

.bottomRow {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 10px;
}

.bottomTitle {
  font-weight: 600;
  opacity: 0.9;
}

.panes {
  display: grid;
  grid-template-columns: 1fr;
  gap: 12px;
  min-height: 0;
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

.compareResult {
  margin: 0;
  white-space: pre-wrap;
  font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, 'Liberation Mono', 'Courier New', monospace;
  font-size: 12px;
}
</style>
