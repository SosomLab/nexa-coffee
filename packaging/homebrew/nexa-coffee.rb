# Homebrew Cask — Nexa Coffee(macOS 유니버설 .app). 버전·체크섬은 render-manifests.sh가 실제 산출물 해시로 채운다.
#
# ★ quarantine을 postflight에서 뗀다(nexa-clip 08-11 실측: 서명 없는 앱 + quarantine → SIGKILL · 공증 없이는 우회 불가).
#   Homebrew Cask는 기본으로 quarantine을 붙이므로, 인증서를 갖추기 전까지는 여기서 떼는 것 외에 방법이 없다.
#   무엇을 왜 하는지 caveats에 밝힌다.
cask "nexa-coffee" do
  version "@VERSION@"
  sha256 "@SHA_MAC@"

  url "https://github.com/SosomLab/nexa-coffee/releases/download/v#{version}/nexa-coffee-#{version}-macos-universal.zip"
  name "Nexa Coffee"
  desc "Tiny menu-bar timer that keeps the computer awake"
  homepage "https://github.com/SosomLab/nexa-coffee"

  livecheck do
    url :url
    strategy :github_latest
  end

  depends_on macos: :big_sur

  app "Nexa Coffee.app"

  postflight do
    system_command "/usr/bin/xattr",
                   args: ["-dr", "com.apple.quarantine", "#{appdir}/Nexa Coffee.app"],
                   sudo: false
  end

  zap trash: [
    "~/Library/Application Support/nexa-coffee",
    "~/Library/LaunchAgents/com.sosomlab.nexa-coffee.plist", # 메뉴 "로그인 시 실행"이 만든 LaunchAgent(DR-15)
  ]

  caveats <<~EOS
    This app is not code-signed or notarized (v1, no certificate).
    The install step removed the macOS quarantine flag (com.apple.quarantine) so it runs right away.
    To verify the download, compare with SHA256SUMS.txt on the release page:
      https://github.com/SosomLab/nexa-coffee/releases

    Nexa Coffee lives in the menu bar (no Dock icon). To start it at login,
    tick "Start at login" in its menu (creates ~/Library/LaunchAgents/com.sosomlab.nexa-coffee.plist).
  EOS
end
