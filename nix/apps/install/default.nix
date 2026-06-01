{ vtr, pkgs }:
{
  type = "app";
  program = pkgs.writeShellApplication {
    name = "install";
    runtimeInputs = [ pkgs.coreutils ];
    text = ''
      addons_path="./example/addons"
      mkdir -p $addons_path
      ln -sf ${vtr + /addons}/* $addons_path/
    '';
  };
  meta = {
    description = "Install debugging output into example folder.";
  };
}