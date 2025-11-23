# Source: https://github.com/akirak/flake-templates/blob/master/minimal/flake.nix
{
  inputs = {};
  outputs = { nixpkgs, ... }: let
      eachSystem = f: nixpkgs.lib.genAttrs nixpkgs.lib.systems.flakeExposed (system: f nixpkgs.legacyPackages.${system});
    in {
      src = "./flake.nix";
      devShells = eachSystem (pkgs: {
        default = pkgs.mkShell {
          buildInputs = with pkgs; [
            raylib
            ffmpeg.dev
          ];
        };
      });
    };
}
