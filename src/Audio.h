// ============================================================
//  Audio.h  -  程序化 8-bit 音效（不依赖任何音频文件）
// ============================================================
#pragma once

#include "Common.h"

#include <map>
#include <vector>

namespace jubi {

enum class Sfx {
    Jump,
    DoubleJump,
    Land,
    Death,
    Checkpoint,
    Feather,
    Bounce,
    Crumble,
    Shoot,
    Win,
    MenuMove,
    MenuSelect,
    Deny,
    Door
};

class Audio {
public:
    Audio() = default;
    ~Audio();

    Audio(const Audio&) = delete;
    Audio& operator=(const Audio&) = delete;

    void init();
    void shutdown();

    void play(Sfx sfx);
    void setMuted(bool muted);
    bool muted() const { return muted_; }
    void toggleMute() { setMuted(!muted_); }

private:
    void build(Sfx sfx, const std::vector<BYTE>& wav);
    void generateAll();

    std::map<Sfx, std::vector<BYTE>> sounds_;
    bool muted_ = false;
    bool initialized_ = false;
};

} // namespace jubi
