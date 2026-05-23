{ vtr, pkgs }:
{
  type = "app";
  program = pkgs.writeShellApplication {
    name = "install";
    runtimeInputs = [ pkgs.coreutils ];
    text = ''
      ln -sf ${vtr + /addons}/* ./example/addons/
    '';
  };
  meta = {
    description = "Install debugging output into example folder.";
  };
}