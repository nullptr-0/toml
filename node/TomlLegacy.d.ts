interface TomlModuleInstance {
    FS: {
      init: (input: () => number | null, output?: (x: string) => void, err?: (x: string) => void) => void;
    };
    print: (x: string) => void;
    printErr: (x: string) => void;
    callMain: (args: string[]) => void;
  }
  
  type TomlModuleFactoryLegacy = (moduleArgs: any) => Promise<TomlModuleInstance>;
  