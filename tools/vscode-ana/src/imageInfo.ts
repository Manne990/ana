import * as fs from "node:fs/promises";

export interface ImageInfo {
  width: number;
  height: number;
  colorCount?: number;
  colorCountKind: "exact" | "palette" | "unknown";
  colors?: Map<string, number>;
}

function readUint32(buffer: Buffer, offset: number): number {
  return buffer.readUInt32BE(offset);
}

function hexColor(r: number, g: number, b: number): string {
  return `#${r.toString(16).padStart(2, "0")}${g.toString(16).padStart(2, "0")}${b.toString(16).padStart(2, "0")}`;
}

function addColor(colors: Map<string, number>, color: string): void {
  colors.set(color, (colors.get(color) ?? 0) + 1);
}

async function readPngInfo(filePath: string): Promise<ImageInfo | undefined> {
  const buffer = await fs.readFile(filePath);
  const signature = buffer.subarray(0, 8);

  if (!signature.equals(Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]))) {
    return undefined;
  }

  let offset = 8;
  let width = 0;
  let height = 0;
  const palette = new Map<string, number>();

  while (offset + 12 <= buffer.length) {
    const length = readUint32(buffer, offset);
    const type = buffer.toString("ascii", offset + 4, offset + 8);
    const dataOffset = offset + 8;

    if (type === "IHDR") {
      width = readUint32(buffer, dataOffset);
      height = readUint32(buffer, dataOffset + 4);
    } else if (type === "PLTE") {
      for (let i = 0; i + 2 < length; i += 3) {
        addColor(
          palette,
          hexColor(buffer[dataOffset + i], buffer[dataOffset + i + 1], buffer[dataOffset + i + 2])
        );
      }
    } else if (type === "IDAT" || type === "IEND") {
      break;
    }

    offset = dataOffset + length + 4;
  }

  if (width === 0 || height === 0) {
    return undefined;
  }

  return {
    width,
    height,
    colorCount: palette.size > 0 ? palette.size : undefined,
    colorCountKind: palette.size > 0 ? "palette" : "unknown",
    colors: palette.size > 0 ? palette : undefined
  };
}

function nextPpmToken(buffer: Buffer, cursor: { offset: number }): string | undefined {
  while (cursor.offset < buffer.length) {
    const char = String.fromCharCode(buffer[cursor.offset]);

    if (/\s/.test(char)) {
      cursor.offset++;
      continue;
    }

    if (char === "#") {
      while (cursor.offset < buffer.length && buffer[cursor.offset] !== 0x0a) {
        cursor.offset++;
      }

      continue;
    }

    break;
  }

  if (cursor.offset >= buffer.length) {
    return undefined;
  }

  const start = cursor.offset;

  while (cursor.offset < buffer.length && !/\s/.test(String.fromCharCode(buffer[cursor.offset]))) {
    cursor.offset++;
  }

  return buffer.toString("ascii", start, cursor.offset);
}

async function readPpmInfo(filePath: string): Promise<ImageInfo | undefined> {
  const buffer = await fs.readFile(filePath);
  const cursor = { offset: 0 };
  const magic = nextPpmToken(buffer, cursor);
  const width = Number(nextPpmToken(buffer, cursor));
  const height = Number(nextPpmToken(buffer, cursor));
  const maxValue = Number(nextPpmToken(buffer, cursor));

  if ((magic !== "P3" && magic !== "P6") || !width || !height || !maxValue) {
    return undefined;
  }

  const colors = new Map<string, number>();

  if (magic === "P3") {
    for (let i = 0; i < width * height; i++) {
      const r = nextPpmToken(buffer, cursor);
      const g = nextPpmToken(buffer, cursor);
      const b = nextPpmToken(buffer, cursor);

      if (r === undefined || g === undefined || b === undefined) {
        break;
      }

      addColor(
        colors,
        hexColor(
          Math.round(Number(r) * 255 / maxValue),
          Math.round(Number(g) * 255 / maxValue),
          Math.round(Number(b) * 255 / maxValue)
        )
      );
    }
  } else {
    while (cursor.offset < buffer.length && /\s/.test(String.fromCharCode(buffer[cursor.offset]))) {
      cursor.offset++;
    }

    const bytesPerSample = maxValue > 255 ? 2 : 1;
    const bytesPerPixel = bytesPerSample * 3;

    for (let offset = cursor.offset; offset + bytesPerPixel <= buffer.length; offset += bytesPerPixel) {
      if (bytesPerSample === 1) {
        addColor(colors, hexColor(buffer[offset], buffer[offset + 1], buffer[offset + 2]));
      } else {
        addColor(
          colors,
          hexColor(
            Math.round(buffer.readUInt16BE(offset) * 255 / maxValue),
            Math.round(buffer.readUInt16BE(offset + 2) * 255 / maxValue),
            Math.round(buffer.readUInt16BE(offset + 4) * 255 / maxValue)
          )
        );
      }
    }
  }

  return {
    width,
    height,
    colorCount: colors.size,
    colorCountKind: "exact",
    colors
  };
}

export async function readImageInfo(filePath: string): Promise<ImageInfo | undefined> {
  const lower = filePath.toLowerCase();

  if (lower.endsWith(".png")) {
    return readPngInfo(filePath);
  }

  if (lower.endsWith(".ppm")) {
    return readPpmInfo(filePath);
  }

  return undefined;
}
