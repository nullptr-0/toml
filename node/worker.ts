enum tomlModule {
  legacy = "TomlLegacy.js",
  wasm = "TomlWasm.js",
  bundle = "TomlWasmBundle.js"
}

const TomlModule: TomlModuleFactoryLegacy | TomlModuleFactoryWasm | TomlModuleFactoryBundle = require('./' + tomlModule.bundle);

process.stdin.setEncoding('utf8');

const path = require('path');

function replaceRootDir(originalPath: string, newRoot: string): string {
  const parsed = path.parse(originalPath);

  // Break the path into segments, excluding the root
  const relativeSegments = originalPath
    .slice(parsed.root.length) // Remove the root from the original path
    .split(path.sep)
    .filter(Boolean); // Remove empty strings

  // Join the new root with the remaining path
  return path.posix.join(newRoot, ...relativeSegments);
}

TomlModule({
  noInitialRun: true
})
  .then((Module: any) => {
    let args = process.argv.slice(2);
    if (args.length >= 2 && args[0] === '--parse') {
      const mountDir = '/mounted';
      Module.FS.mkdir(mountDir);
      Module.FS.mount(Module.FS.filesystems.NODEFS, { root: '/' }, mountDir);
      let input = args[1];
      const absInputPath = path.resolve(input!);
      args[1] = replaceRootDir(absInputPath, mountDir);
      let output = '';
      if (args.length >= 4 && args[2] === '--output') {
        output = args[3];
        const absOutputPath = output ? path.resolve(output) : path.resolve(__dirname, 'output.txt');
        args[3] = replaceRootDir(absOutputPath, mountDir);
      }
    }
    else if (args.length == 3 && args[0] === '--langsvr') {
      if (args[2].startsWith('--clientProcessId=')) {
        args = args.slice(0, 2);
      }
    }

    Module.print = (text: string) => {
      process.stdout.write(text + '\n');
    };

    Module.printErr = (err: string) => {
      process.stderr.write('[TOML WASM error] ' + err + '\n');
    };

    Module.callMain(args);

    Module.FS.quit();
    process.exit(0);
  })
  .catch((err: any) => {
    process.stderr.write('[TOML module error] ' + err + '\n');
  });
