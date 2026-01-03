export class WebAudioWorkletSink {
  private ctx: AudioContext | null = null
  private node: AudioWorkletNode | null = null
  private _lastError: string | null = null
  private pushedFrames = 0

  public get sampleRate(): number | null {
    return this.ctx?.sampleRate ?? null
  }

  public get state(): AudioContextState | 'none' {
    return this.ctx?.state ?? 'none'
  }

  public get lastError(): string | null {
    return this._lastError
  }

  public get totalPushedFrames(): number {
    return this.pushedFrames
  }

  public get started(): boolean {
    return this.ctx !== null && this.node !== null
  }

  public async ensureStarted(): Promise<void> {
    if (this.started) return

    this._lastError = null

    try {
      const ctx = new AudioContext()
      // Must be called from a user gesture handler in most browsers.
      if (ctx.state !== 'running') {
        await ctx.resume()
      }

      // Load worklet module from public/ so it's always available in production builds.
      // BASE_URL includes the GitHub Pages repo subpath (e.g. /MiciusFCEmulator/).
      const workletUrl = `${import.meta.env.BASE_URL}apu.worklet.js`
      await ctx.audioWorklet.addModule(workletUrl)

      const node = new AudioWorkletNode(ctx, 'apu-player', {
        numberOfInputs: 0,
        numberOfOutputs: 1,
        outputChannelCount: [2],
      })

      node.connect(ctx.destination)

      this.ctx = ctx
      this.node = node
    } catch (e) {
      const msg = e instanceof Error ? e.message : String(e)
      this._lastError = msg
      throw e
    }
  }

  public pushInterleaved(samples: Float32Array): void {
    if (!this.node) return
    if (samples.length === 0) return

    this.pushedFrames += (samples.length / 2) | 0

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
    this.pushedFrames = 0

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
