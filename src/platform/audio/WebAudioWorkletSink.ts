export class WebAudioWorkletSink {
  private ctx: AudioContext | null = null
  private node: AudioWorkletNode | null = null

  public get sampleRate(): number | null {
    return this.ctx?.sampleRate ?? null
  }

  public get started(): boolean {
    return this.ctx !== null && this.node !== null
  }

  public async ensureStarted(): Promise<void> {
    if (this.started) return

    const ctx = new AudioContext()
    // Must be called from a user gesture handler in most browsers.
    if (ctx.state !== 'running') {
      await ctx.resume()
    }

    // Load worklet module (bundled by Vite).
    const url = new URL('./apu.worklet.ts', import.meta.url)
    await ctx.audioWorklet.addModule(url)

    const node = new AudioWorkletNode(ctx, 'apu-player', {
      numberOfInputs: 0,
      numberOfOutputs: 1,
      outputChannelCount: [2],
    })

    node.connect(ctx.destination)

    this.ctx = ctx
    this.node = node
  }

  public pushInterleaved(samples: Float32Array): void {
    if (!this.node) return
    if (samples.length === 0) return

    // Transfer the buffer to avoid copying. The caller should not reuse the array.
    this.node.port.postMessage({ type: 'push', samples }, [samples.buffer])
  }

  public async dispose(): Promise<void> {
    try {
      this.node?.disconnect()
    } catch {
      // ignore
    }
    this.node = null

    const ctx = this.ctx
    this.ctx = null
    if (ctx) {
      try {
        await ctx.close()
      } catch {
        // ignore
      }
    }
  }
}
