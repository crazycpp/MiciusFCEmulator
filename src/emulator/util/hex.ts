export function hex8(value: number): string {
  return (value & 0xff).toString(16).padStart(2, '0').toUpperCase()
}

export function hex16(value: number): string {
  return (value & 0xffff).toString(16).padStart(4, '0').toUpperCase()
}
