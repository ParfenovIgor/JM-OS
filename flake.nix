{
  description = "A very basic flake";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    flake-utils.url = "github:numtide/flake-utils";
    devshell.url = "github:deemp/flakes?dir=devshell";
  };
  outputs = { self, nixpkgs, flake-utils, devshell }: flake-utils.lib.eachDefaultSystem (system:
    let
      pkgs = nixpkgs.legacyPackages.${system};
      inherit (devshell.functions.${system}) mkShell mkCommands;
      tools = [
        pkgs.nasm
        pkgs.xorriso
        pkgs.gnumake
        pkgs.mtools
        pkgs.wget
        pkgs.qemu
        pkgs.grub2
      ];
    in
    {
      devShells.default = mkShell {
        bash.extra =
          let
            # see https://github.com/lordmilko/i686-elf-tools/releases/tag/7.1.0
            compiler-arch = "i686";
            cross-compiler = "${compiler-arch}-elf-tools";
            archive = "${compiler-arch}-elf-tools-linux.zip";
          in
          ''
            if [[ ! -d ${cross-compiler} ]];
            then
              wget https://github.com/lordmilko/i686-elf-tools/releases/download/7.1.0/${archive}
              unzip ${archive} -d ${cross-compiler}
              rm ${archive}
            fi
            export PATH="$PWD/${cross-compiler}/bin:$PATH"
          '';
        packages = tools;
        commands = mkCommands "tools" tools;
      };
    });
}
