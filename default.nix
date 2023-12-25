with import <nixpkgs> {};
mkShell {
  packages = [
    bashInteractive
    platformio
  ];
}
