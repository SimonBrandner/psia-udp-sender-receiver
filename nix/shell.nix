{pkgs, ...}:
pkgs.mkShell rec {
  buildInputs = with pkgs; [
    zlib
    openssl
  ];
  LD_LIBRARY_PATH = "${pkgs.lib.makeLibraryPath buildInputs}";
}
