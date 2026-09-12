# Homebrew Cask — kiros33/tap 에 복사해 쓴다(release 워크플로가 sha256·버전을 채운다).
cask "nexa-coffee" do
  version "__VERSION__"
  sha256 arm: "__SHA256_ARM64__", intel: "__SHA256_X64__"

  url "https://github.com/SosomLab/nexa-coffee/releases/download/v#{version}/nexa-coffee-#{version}-macos-universal.zip"
  name "Nexa Coffee"
  desc "Tiny menu-bar timer that keeps the Mac awake"
  homepage "https://github.com/SosomLab/nexa-coffee"

  app "Nexa Coffee.app"

  # v1은 서명·공증이 없다 — 설치 시 격리 표식을 뗀다
  postflight do
    system_command "/usr/bin/xattr", args: ["-dr", "com.apple.quarantine", "#{appdir}/Nexa Coffee.app"], sudo: false
  end

  zap trash: ["~/Library/Application Support/nexa-coffee"]
end
