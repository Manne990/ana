export type TargetProfileId =
  | "a1200-baseline"
  | "portable-amiga"
  | "a500-experimental"
  | "aga-experimental";

export interface TargetProfile {
  id: TargetProfileId;
  label: string;
  cpu: string;
  chipRamKb: number;
  fastRamKb: number;
  maxColors: number;
  diagnosticOnly: boolean;
  notes: string[];
}

const PROFILES: Record<TargetProfileId, TargetProfile> = {
  "a1200-baseline": {
    id: "a1200-baseline",
    label: "A1200 baseline",
    cpu: "68020",
    chipRamKb: 2048,
    fastRamKb: 0,
    maxColors: 16,
    diagnosticOnly: false,
    notes: ["Stock A1200 without Fast RAM.", "PAL lores 16-color workflow."]
  },
  "portable-amiga": {
    id: "portable-amiga",
    label: "Portable Amiga",
    cpu: "68000+",
    chipRamKb: 1024,
    fastRamKb: 0,
    maxColors: 16,
    diagnosticOnly: false,
    notes: ["Portable profile for current ANA Amiga builds."]
  },
  "a500-experimental": {
    id: "a500-experimental",
    label: "A500 experimental",
    cpu: "68000",
    chipRamKb: 512,
    fastRamKb: 0,
    maxColors: 16,
    diagnosticOnly: true,
    notes: ["Diagnostic-only profile until ANA has stable A500 runtime support."]
  },
  "aga-experimental": {
    id: "aga-experimental",
    label: "AGA experimental",
    cpu: "68020",
    chipRamKb: 2048,
    fastRamKb: 0,
    maxColors: 256,
    diagnosticOnly: true,
    notes: ["Diagnostic-only profile until ANA has stable AGA runtime and asset support."]
  }
};

export function targetProfileFromId(value: string): TargetProfile {
  return PROFILES[value as TargetProfileId] ?? PROFILES["a1200-baseline"];
}
