// ============================================================
//  Game.h  -  游戏状态机、界面、存档与演出
// ============================================================
#pragma once

#include "Common.h"
#include "Audio.h"
#include "Level.h"
#include "Player.h"
#include "Renderer.h"

#include <string>
#include <vector>

namespace jubi {

class Game {
public:
    Game();
    ~Game();

    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    bool init(HWND hwnd);
    void shutdown();

    void update(float dt, const InputState& input);
    void render();
    void present(HDC dc, int clientW, int clientH);

    void prepareScreenshot(int room, const std::wstring& state);
    bool saveScreenshot(const std::wstring& path);
    bool runAutomatedTest(int room, const std::wstring& test, const std::wstring& outPath);
    void showToast(const std::wstring& text);

    void handleMouseMove(int x, int y);
    void handleMouseClick(int x, int y);
    bool wantsCursor() const;

    bool quitRequested() const { return quitRequested_; }
    void notifyFocusLost();

private:
    enum class Screen { Title, Controls, Playing, Paused, Dying, Victory };

    struct Particle {
        Vec2 pos;
        Vec2 vel;
        Color color;
        float life = 0.0f;
        float maxLife = 1.0f;
        float size = 3.0f;
        float gravity = 0.0f;
        float drag = 0.0f;
        int shape = 0; // 0 circle, 1 feather
    };

    struct SaveData {
        int room = 0;
        int deaths = 0;
        float time = 0.0f;
        int featherMask = 0;
        int checkpoint = -1;
        bool valid = false;
    };

    struct Record {
        float bestTime = 0.0f;
        int bestDeaths = 0;
        bool valid = false;
    };

    // 流程
    void startNewGame();
    void continueGame();
    void loadRoom(int index, bool useCheckpoint);
    void nextRoom();
    void restartRoom();
    void respawn();
    void die();
    void win();
    void enterPause();
    void leavePause();
    void returnToTitle();

    // 更新子状态
    void updateTitle(float dt, const InputState& input);
    void updatePlaying(float dt, const InputState& input);
    void updatePaused(float dt, const InputState& input);
    void updateDying(float dt, const InputState& input);
    void updateVictory(float dt, const InputState& input);
    void updateParticles(float dt);

    // 绘制子状态
    void drawTitle();
    void drawControls();
    void drawPlaying();
    void drawWorldScene();
    void drawHud();
    void drawBanner();
    void drawPaused();
    void drawDying();
    void drawVictory();
    void drawParticles();
    void drawToast();
    void drawDebug();

    // 菜单
    void moveTitleMenu(int dir);
    void confirmTitleMenu();
    void movePauseMenu(int dir);
    void confirmPauseMenu();
    void drawMenuItem(const RectF& rect, const std::wstring& label, bool selected,
                      bool disabled, bool arrow = true);

    // 存档
    std::wstring exeDir() const;
    std::wstring savePath() const;
    std::wstring recordPath() const;
    bool readSave(SaveData& out) const;
    void writeSave() const;
    bool readRecord(Record& out) const;
    void writeRecord() const;
    void refreshSaveFlag();

    // 辅助
    void spawnBurst(Vec2 pos, Color color, int count, float speed, float gravity = 900.0f);
    void spawnFeatherBurst(Vec2 pos, int count);
    void spawnDust(Vec2 pos, int count, Color color);
    void addParticle(const Particle& p);
    int featherCount() const;
    std::wstring rankText() const;
    Color rankColor() const;
    void updateCamera(float dt);
    int saveCheckpointIndex() const;

    Renderer renderer_;
    Audio audio_;
    Level level_;
    Player player_;

    Screen screen_ = Screen::Title;
    Screen controlsReturn_ = Screen::Title;
    HWND hwnd_ = nullptr;

    int roomIndex_ = 0;
    int deaths_ = 0;
    float playTime_ = 0.0f;
    int featherMask_ = 0;
    bool quitRequested_ = false;

    int titleMenu_ = 0;
    int pauseMenu_ = 0;
    bool hasSave_ = false;
    SaveData savePreview_;
    int mouseX_ = -1;
    int mouseY_ = -1;

    float time_ = 0.0f;
    float fade_ = 1.0f;
    float bannerTimer_ = 0.0f;
    float shake_ = 0.0f;
    float deathTimer_ = 0.0f;
    float toastTimer_ = 0.0f;
    float hintTimer_ = 0.0f;
    std::wstring toast_;
    bool showDebug_ = false;
    bool pausedByFocus_ = false;

    std::vector<Particle> particles_;
    Record record_;
    std::wstring screenshotPath_;
};

} // namespace jubi
