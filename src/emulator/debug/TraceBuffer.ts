export class TraceBuffer {
  private readonly lines: string[]
  private writeIndex = 0
  private isFull = false
  private _version = 0

  public constructor(private readonly capacity: number) {
    if (capacity <= 0) {
      throw new Error('capacity must be > 0')
    }
    this.lines = new Array<string>(capacity)
    this.lines.fill('')
  }

  public get version(): number {
    return this._version
  }

  public push(line: string): void {
    this.lines[this.writeIndex] = line
    this.writeIndex = (this.writeIndex + 1) % this.capacity
    if (this.writeIndex === 0) {
      this.isFull = true
    }

    this._version++
  }

  public snapshot(): string[] {
    if (!this.isFull) {
      return this.lines.slice(0, this.writeIndex)
    }

    const result: string[] = []
    for (let i = 0; i < this.capacity; i++) {
      const idx = (this.writeIndex + i) % this.capacity
      result.push(this.lines[idx] ?? '')
    }
    return result
  }

  public clear(): void {
    this.writeIndex = 0
    this.isFull = false
    this.lines.fill('')
    this._version++
  }
}
