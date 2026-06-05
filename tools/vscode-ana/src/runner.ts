import { spawn } from "node:child_process";
import { formatCommand } from "./buildCommands";

export interface OutputSink {
  append(value: string): void;
  appendLine(value: string): void;
  show(preserveFocus?: boolean): void;
}

export interface RunOptions {
  cwd: string;
  label: string;
  command: string;
  args: string[];
  output: OutputSink;
  showOutput: boolean;
}

export async function runProcess(options: RunOptions): Promise<number> {
  const { cwd, label, command, args, output, showOutput } = options;

  if (showOutput) {
    output.show(true);
  }

  output.appendLine("");
  output.appendLine(`== ${label} ==`);
  output.appendLine(`cwd: ${cwd}`);
  output.appendLine(`$ ${formatCommand(command, args)}`);

  return new Promise<number>((resolve) => {
    let settled = false;
    const finish = (code: number): void => {
      if (settled) {
        return;
      }

      settled = true;
      resolve(code);
    };

    const child = spawn(command, args, {
      cwd,
      shell: false,
      windowsHide: true
    });

    child.stdout.on("data", (chunk: Buffer) => output.append(chunk.toString()));
    child.stderr.on("data", (chunk: Buffer) => output.append(chunk.toString()));

    child.on("error", (error) => {
      output.appendLine(`error: ${error.message}`);
      finish(-1);
    });

    child.on("close", (code) => {
      const exitCode = code ?? -1;
      output.appendLine(`exit: ${exitCode}`);
      finish(exitCode);
    });
  });
}
