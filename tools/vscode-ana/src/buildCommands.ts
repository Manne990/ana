export type BuildCommandKey =
  | "buildHost"
  | "test"
  | "buildAmiga"
  | "buildAdf"
  | "buildA1200Adf"
  | "buildTools";

export interface BuildCommand {
  label: string;
  command: string;
  args: string[];
}

const BUILD_ARGS: Record<BuildCommandKey, { label: string; args: string[] }> = {
  buildHost: {
    label: "Build Host",
    args: ["all"]
  },
  test: {
    label: "Test",
    args: ["test"]
  },
  buildAmiga: {
    label: "Build Amiga",
    args: ["amiga-examples"]
  },
  buildAdf: {
    label: "Build ADF",
    args: ["adfs"]
  },
  buildA1200Adf: {
    label: "Build A1200 ADF",
    args: [
      "amiga-a1200-examples",
      "invaders-a1200-adf",
      "amaze-a1200-adf",
      "byte-brothers-a1200-adf"
    ]
  },
  buildTools: {
    label: "Build Tools",
    args: ["tools"]
  }
};

export function getBuildCommand(key: BuildCommandKey, makePath = "make"): BuildCommand {
  const definition = BUILD_ARGS[key];

  return {
    label: definition.label,
    command: makePath,
    args: [...definition.args]
  };
}

export function quoteArg(arg: string): string {
  if (/^[A-Za-z0-9_./:=@%+-]+$/.test(arg)) {
    return arg;
  }

  return `"${arg.replace(/(["\\$`])/g, "\\$1")}"`;
}

export function formatCommand(command: string, args: string[]): string {
  return [command, ...args].map(quoteArg).join(" ");
}
