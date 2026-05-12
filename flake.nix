{
  description = "VTR - Video Transmission Receiver for Godot";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
    flake-parts.url = "github:hercules-ci/flake-parts";
    
    rmmock = {
      url = "github:vixhentx/RMMock";
    };
    
    godot-cpp = {
      url = "github:godotengine/godot-cpp";
      flake = false;
    };
  };

  outputs = inputs@{ flake-parts, ... }:
    flake-parts.lib.mkFlake { inherit inputs; } {
      systems = [ "x86_64-linux" "aarch64-linux" ];
      imports = [
        ./nix/parts
      ];
    };
}
