// ============================================================
//  Game.cpp  -  标题、游戏、暂停、死亡、结算与存档
// ============================================================
#include "Game.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace jubi {

namespace {

const wchar_t* kUiFont = L"Microsoft YaHei UI";

void titleGradient(Renderer& r, Color top, Color bottom) {
    for (int i = 0; i < 36; ++i) {
        float t = static_cast<float>(i) / 35.0f;
        r.fillRect(RectF(0, i * 20.0f, kScreenW, 21.0f), mix(top, bottom, smoothstep(t)));
    }
}

void drawKeycap(Renderer& r, float x, float y, float w, const std::wstring& label,
                Color accent) {
    r.fillRoundRect(RectF(x, y + 3, w, 28), 6, rgb(8, 8, 14));
    r.fillRoundRect(RectF(x, y, w, 28), 6, mix(rgb(44, 42, 56), accent, 0.18f));
    r.strokeRoundRect(RectF(x, y, w, 28), 6, mix(accent, rgb(255, 255, 255), 0.25f), 1);
    r.text(label, x + w * 0.5f, y + 5, 14, rgb(245, 240, 225), TextAlign::Center, true, kUiFont);
}

void drawTinyFeather(Renderer& r, float cx, float cy, float s, Color c) {
    Vec2 pts[4] = {
        Vec2(cx, cy - 9 * s), Vec2(cx + 6 * s, cy), Vec2(cx, cy + 9 * s), Vec2(cx - 4 * s, cy)
    };
    r.polygon(pts, 4, c);
    r.line(Vec2(cx, cy - 8 * s), Vec2(cx, cy + 9 * s), mix(c, rgb(255, 120, 60), 0.55f), 1);
}

std::wstring intText(int v) { return std::to_wstring(v); }

} // namespace

Game::Game() = default;

Game::~Game() { shutdown(); }

bool Game::init(HWND hwnd) {
    hwnd_ = hwnd;
    if (!renderer_.init(hwnd)) return false;
    audio_.init();
    readRecord(record_);
    refreshSaveFlag();
    screen_ = Screen::Title;
    titleMenu_ = 0;
    fade_ = 1.0f;
    return true;
}

void Game::shutdown() {
    audio_.shutdown();
    renderer_.shutdown();
    hwnd_ = nullptr;
}

void Game::present(HDC dc, int clientW, int clientH) {
    renderer_.present(dc, clientW, clientH);
}

// ============================================================
//  路径与存档
// ============================================================
std::wstring Game::exeDir() const {
    wchar_t buf[MAX_PATH]{};
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring s(buf, n);
    size_t p = s.find_last_of(L"\\/");
    if (p == std::wstring::npos) return L".";
    return s.substr(0, p);
}

std::wstring Game::savePath() const { return exeDir() + L"\\jubi_save.txt"; }
std::wstring Game::recordPath() const { return exeDir() + L"\\record.dat"; }

bool Game::readSave(SaveData& out) const {
    std::ifstream f{std::filesystem::path(savePath())};
    if (!f) return false;
    SaveData d;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        try {
            if (key == "room") d.room = std::stoi(val);
            else if (key == "deaths") d.deaths = std::stoi(val);
            else if (key == "time") d.time = std::stof(val);
            else if (key == "feathers") d.featherMask = std::stoi(val);
            else if (key == "checkpoint") d.checkpoint = std::stoi(val);
        } catch (...) {
            return false;
        }
    }
    if (d.room < 0 || d.room >= Level::roomCount()) return false;
    if (d.deaths < 0) d.deaths = 0;
    if (d.time < 0.0f) d.time = 0.0f;
    d.featherMask &= 63;
    if (d.checkpoint < -1 || d.checkpoint > 12) d.checkpoint = -1;
    d.valid = true;
    out = d;
    return true;
}

void Game::writeSave() const {
    std::ofstream f{std::filesystem::path(savePath())};
    if (!f) return;
    f << "version=1\n";
    f << "room=" << roomIndex_ << "\n";
    f << "deaths=" << deaths_ << "\n";
    f << "time=" << std::fixed << std::setprecision(2) << playTime_ << "\n";
    f << "feathers=" << featherMask_ << "\n";
    f << "checkpoint=" << level_.activeCheckpoint() << "\n";
}

bool Game::readRecord(Record& out) const {
    std::ifstream f{std::filesystem::path(recordPath())};
    if (!f) return false;
    Record r;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        try {
            if (key == "best_time") r.bestTime = std::stof(val);
            else if (key == "best_deaths") r.bestDeaths = std::stoi(val);
        } catch (...) {
            return false;
        }
    }
    if (r.bestTime <= 0.0f || r.bestDeaths < 0) return false;
    r.valid = true;
    out = r;
    return true;
}

void Game::writeRecord() const {
    std::ofstream f{std::filesystem::path(recordPath())};
    if (!f) return;
    f << "best_time=" << std::fixed << std::setprecision(2) << record_.bestTime << "\n";
    f << "best_deaths=" << record_.bestDeaths << "\n";
}

void Game::refreshSaveFlag() {
    SaveData d;
    hasSave_ = readSave(d);
    if (hasSave_) savePreview_ = d;
}

int Game::saveCheckpointIndex() const { return level_.activeCheckpoint(); }

// ============================================================
//  流程
// ============================================================
void Game::startNewGame() {
    roomIndex_ = 0;
    deaths_ = 0;
    playTime_ = 0.0f;
    featherMask_ = 0;
    level_.load(0);
    level_.setFeatherMask(0);
    player_.reset(level_.spawnPoint());
    particles_.clear();
    screen_ = Screen::Playing;
    fade_ = 1.0f;
    bannerTimer_ = 3.2f;
    hintTimer_ = 10.0f;
    shake_ = 0.0f;
    deathTimer_ = 0.0f;
    showDebug_ = false;
    writeSave();
    refreshSaveFlag();
    audio_.play(Sfx::Door);
}

void Game::continueGame() {
    SaveData s;
    if (!readSave(s)) {
        hasSave_ = false;
        audio_.play(Sfx::Deny);
        return;
    }
    roomIndex_ = s.room;
    deaths_ = s.deaths;
    playTime_ = s.time;
    featherMask_ = s.featherMask & 63;
    level_.load(roomIndex_);
    level_.setFeatherMask((featherMask_ >> roomIndex_) & 1);
    if (s.checkpoint >= 0) level_.setCheckpoint(s.checkpoint);
    player_.reset(level_.respawnPoint());
    particles_.clear();
    screen_ = Screen::Playing;
    fade_ = 1.0f;
    bannerTimer_ = 2.6f;
    hintTimer_ = 6.0f;
    shake_ = 0.0f;
    audio_.play(Sfx::Door);
    showToast(L"继续前进。");
}

void Game::loadRoom(int index, bool useCheckpoint) {
    if (index < 0) index = 0;
    if (index >= Level::roomCount()) index = Level::roomCount() - 1;
    roomIndex_ = index;
    level_.load(index);
    level_.setFeatherMask((featherMask_ >> index) & 1);
    if (useCheckpoint && savePreview_.checkpoint >= 0) {
        level_.setCheckpoint(savePreview_.checkpoint);
    }
    player_.reset(level_.spawnPoint());
    particles_.clear();
    bannerTimer_ = 3.2f;
    hintTimer_ = (index == 0) ? 10.0f : 6.0f;
    fade_ = 1.0f;
    shake_ = 0.0f;
    screen_ = Screen::Playing;
    writeSave();
    refreshSaveFlag();
}

void Game::nextRoom() {
    if (roomIndex_ >= Level::roomCount() - 1) {
        win();
        return;
    }
    audio_.play(Sfx::Door);
    loadRoom(roomIndex_ + 1, false);
}

void Game::restartRoom() {
    level_.resetRuntime();
    level_.setFeatherMask((featherMask_ >> roomIndex_) & 1);
    player_.reset(level_.spawnPoint());
    particles_.clear();
    screen_ = Screen::Playing;
    bannerTimer_ = 0.0f;
    hintTimer_ = 2.5f;
    fade_ = 0.55f;
    shake_ = 0.0f;
    writeSave();
    refreshSaveFlag();
    showToast(L"再来一次。");
}

void Game::respawn() {
    player_.reset(level_.respawnPoint());
    screen_ = Screen::Playing;
    fade_ = 0.6f;
    shake_ = 0.0f;
    bannerTimer_ = 0.0f;
    deathTimer_ = 0.0f;
}

void Game::die() {
    if (screen_ != Screen::Playing) return;
    deaths_++;
    screen_ = Screen::Dying;
    deathTimer_ = 0.72f;
    shake_ = 17.0f;
    audio_.play(Sfx::Death);
    Vec2 p = player_.center();
    if (p.y > 720.0f) p.y = 700.0f;
    spawnFeatherBurst(p, 28);
    spawnBurst(p, rgb(255, 238, 190), 14, 220.0f, 700.0f);
    writeSave();
    refreshSaveFlag();
}

void Game::win() {
    screen_ = Screen::Victory;
    fade_ = 0.0f;
    shake_ = 0.0f;
    particles_.clear();
    audio_.play(Sfx::Win);
    for (int i = 0; i < 90; ++i) {
        Particle p;
        p.pos = Vec2(hash01(i, 3, 17) * kScreenW, -20.0f - hash01(i, 5, 17) * 300.0f);
        p.vel = Vec2((hash01(i, 7, 17) - 0.5f) * 180.0f, 90.0f + hash01(i, 9, 17) * 180.0f);
        p.color = (i % 3 == 0) ? rgb(120, 180, 255) : ((i % 3 == 1) ? rgb(255, 224, 120) : rgb(255, 255, 255));
        p.life = 4.0f + hash01(i, 11, 17) * 3.0f;
        p.maxLife = p.life;
        p.size = 3.0f + hash01(i, 13, 17) * 4.0f;
        p.gravity = 20.0f;
        p.shape = 1;
        addParticle(p);
    }
    if (!record_.valid || playTime_ < record_.bestTime) record_.bestTime = playTime_;
    if (!record_.valid || deaths_ < record_.bestDeaths) record_.bestDeaths = deaths_;
    record_.valid = true;
    writeRecord();
    std::error_code ec;
    std::filesystem::remove(std::filesystem::path(savePath()), ec);
    refreshSaveFlag();
}

void Game::enterPause() {
    if (screen_ != Screen::Playing) return;
    screen_ = Screen::Paused;
    pauseMenu_ = 0;
    pausedByFocus_ = false;
}

void Game::leavePause() {
    if (screen_ != Screen::Paused) return;
    screen_ = Screen::Playing;
    pausedByFocus_ = false;
}

void Game::returnToTitle() {
    screen_ = Screen::Title;
    titleMenu_ = 0;
    particles_.clear();
    bannerTimer_ = 0.0f;
    fade_ = 1.0f;
    refreshSaveFlag();
}

void Game::notifyFocusLost() {
    if (screen_ == Screen::Playing) {
        enterPause();
        pausedByFocus_ = true;
    }
}

// ============================================================
//  更新
// ============================================================
void Game::update(float dt, const InputState& input) {
    time_ += dt;
    if (input.mutePressed) audio_.toggleMute();
    if (input.debugPressed) showDebug_ = !showDebug_;
    if (toastTimer_ > 0.0f) toastTimer_ -= dt;
    if (fade_ > 0.0f) fade_ = (std::max)(0.0f, fade_ - dt * 2.4f);
    updateCamera(dt);
    if (screen_ != Screen::Paused) updateParticles(dt);

    switch (screen_) {
        case Screen::Title: updateTitle(dt, input); break;
        case Screen::Controls:
            if (input.confirmPressed || input.backPressed) {
                audio_.play(Sfx::MenuSelect);
                screen_ = controlsReturn_;
            }
            break;
        case Screen::Playing: updatePlaying(dt, input); break;
        case Screen::Paused: updatePaused(dt, input); break;
        case Screen::Dying: updateDying(dt, input); break;
        case Screen::Victory: updateVictory(dt, input); break;
    }
}

void Game::updateTitle(float dt, const InputState& input) {
    (void)dt;
    if (input.upPressed) {
        moveTitleMenu(-1);
    }
    if (input.downPressed) {
        moveTitleMenu(1);
    }
    if (input.confirmPressed) confirmTitleMenu();
    if (input.backPressed) quitRequested_ = true;
}

void Game::updatePlaying(float dt, const InputState& input) {
    if (input.backPressed) {
        enterPause();
        return;
    }
    if (input.restartPressed) {
        restartRoom();
        return;
    }

    playTime_ += dt;
    if (bannerTimer_ > 0.0f) bannerTimer_ -= dt;
    if (hintTimer_ > 0.0f) hintTimer_ -= dt;

    level_.update(dt, player_.box, player_.onGround, audio_);
    player_.update(dt, input, level_, audio_);
    if (player_.onGround) level_.triggerCrumbleUnder(player_.box);

    int cp = level_.touchCheckpoint(player_.box);
    if (cp >= 0) {
        audio_.play(Sfx::Checkpoint);
        spawnBurst(Vec2(level_.checkpoints()[cp].spawn.x + kPlayerW * 0.5f,
                        level_.checkpoints()[cp].spawn.y + kPlayerH * 0.5f),
                   level_.accent(), 22, 170.0f, 220.0f);
        shake_ = (std::max)(shake_, 4.0f);
        writeSave();
        refreshSaveFlag();
        showToast(L"检查点已点亮。");
    }

    int fi = level_.collectFeather(player_.box);
    if (fi >= 0) {
        featherMask_ |= (1 << roomIndex_);
        audio_.play(Sfx::Feather);
        spawnFeatherBurst(player_.center(), 24);
        shake_ = (std::max)(shake_, 5.0f);
        writeSave();
        refreshSaveFlag();
        showToast(L"星羽 +1  ·  共 " + intText(featherCount()) + L" / 6");
    }

    if (level_.hazardAt(player_.box) || player_.box.y > 780.0f) {
        die();
        return;
    }
    if (level_.goalAt(player_.box)) {
        nextRoom();
        return;
    }

    if (player_.onGround && std::fabs(player_.vel.x) > 210.0f &&
        std::fmod(time_, 0.085f) < dt) {
        spawnDust(Vec2(player_.box.center().x, player_.box.y + player_.box.h), 2,
                  mix(level_.accent(), rgb(255, 255, 255), 0.35f));
    }
}

void Game::updatePaused(float dt, const InputState& input) {
    (void)dt;
    if (input.backPressed) {
        leavePause();
        audio_.play(Sfx::MenuSelect);
        return;
    }
    if (input.upPressed) movePauseMenu(-1);
    if (input.downPressed) movePauseMenu(1);
    if (input.confirmPressed) confirmPauseMenu();
}

void Game::updateDying(float dt, const InputState& input) {
    (void)input;
    deathTimer_ -= dt;
    if (deathTimer_ <= 0.0f) respawn();
}

void Game::updateVictory(float dt, const InputState& input) {
    (void)dt;
    if (input.confirmPressed || input.backPressed) {
        audio_.play(Sfx::MenuSelect);
        returnToTitle();
    }
}

void Game::updateParticles(float dt) {
    for (auto& p : particles_) {
        p.life -= dt;
        p.vel.y += p.gravity * dt;
        p.vel.x *= (1.0f - p.drag * dt);
        p.vel.y *= (1.0f - p.drag * dt);
        p.pos += p.vel * dt;
    }
    particles_.erase(std::remove_if(particles_.begin(), particles_.end(),
                                    [](const Particle& p) { return p.life <= 0.0f; }),
                     particles_.end());
}

// ============================================================
//  菜单动作
// ============================================================
void Game::moveTitleMenu(int dir) {
    for (int k = 0; k < 4; ++k) {
        titleMenu_ = (titleMenu_ + dir + 4) % 4;
        if (titleMenu_ != 1 || hasSave_) break;
    }
    audio_.play(Sfx::MenuMove);
}

void Game::confirmTitleMenu() {
    if (titleMenu_ == 0) {
        audio_.play(Sfx::MenuSelect);
        startNewGame();
    } else if (titleMenu_ == 1) {
        if (hasSave_) {
            audio_.play(Sfx::MenuSelect);
            continueGame();
        } else {
            audio_.play(Sfx::Deny);
            showToast(L"还没有可以继续的旅程。");
        }
    } else if (titleMenu_ == 2) {
        audio_.play(Sfx::MenuSelect);
        controlsReturn_ = Screen::Title;
        screen_ = Screen::Controls;
    } else {
        quitRequested_ = true;
    }
}

void Game::movePauseMenu(int dir) {
    pauseMenu_ = (pauseMenu_ + dir + 3) % 3;
    audio_.play(Sfx::MenuMove);
}

void Game::confirmPauseMenu() {
    if (pauseMenu_ == 0) {
        audio_.play(Sfx::MenuSelect);
        leavePause();
    } else if (pauseMenu_ == 1) {
        audio_.play(Sfx::MenuSelect);
        restartRoom();
    } else {
        audio_.play(Sfx::MenuSelect);
        returnToTitle();
    }
}

// ============================================================
//  鼠标：标题 / 暂停 / 操作说明 / 胜利界面
// ============================================================
bool Game::wantsCursor() const {
    return screen_ == Screen::Title || screen_ == Screen::Controls ||
           screen_ == Screen::Paused || screen_ == Screen::Victory;
}

void Game::handleMouseMove(int x, int y) {
    mouseX_ = x;
    mouseY_ = y;
    if (screen_ == Screen::Title) {
        for (int i = 0; i < 4; ++i) {
            RectF r(100, 380 + static_cast<float>(i * 64), 360, 54);
            if (r.contains(Vec2(static_cast<float>(x), static_cast<float>(y)))) {
                if (titleMenu_ != i && !(i == 1 && !hasSave_)) {
                    titleMenu_ = i;
                    audio_.play(Sfx::MenuMove);
                }
                return;
            }
        }
    } else if (screen_ == Screen::Paused) {
        for (int i = 0; i < 3; ++i) {
            RectF r(430, 274 + static_cast<float>(i * 64), 420, 50);
            if (r.contains(Vec2(static_cast<float>(x), static_cast<float>(y)))) {
                if (pauseMenu_ != i) {
                    pauseMenu_ = i;
                    audio_.play(Sfx::MenuMove);
                }
                return;
            }
        }
    }
}

void Game::handleMouseClick(int x, int y) {
    mouseX_ = x;
    mouseY_ = y;
    Vec2 p(static_cast<float>(x), static_cast<float>(y));

    if (screen_ == Screen::Title) {
        for (int i = 0; i < 4; ++i) {
            if (RectF(100, 380 + static_cast<float>(i * 64), 360, 54).contains(p)) {
                titleMenu_ = i;
                confirmTitleMenu();
                return;
            }
        }
    } else if (screen_ == Screen::Paused) {
        for (int i = 0; i < 3; ++i) {
            if (RectF(430, 274 + static_cast<float>(i * 64), 420, 50).contains(p)) {
                pauseMenu_ = i;
                confirmPauseMenu();
                return;
            }
        }
    } else if (screen_ == Screen::Controls) {
        audio_.play(Sfx::MenuSelect);
        screen_ = controlsReturn_;
    } else if (screen_ == Screen::Victory) {
        audio_.play(Sfx::MenuSelect);
        returnToTitle();
    }
}

// ============================================================
//  粒子
// ============================================================
void Game::addParticle(const Particle& p) {
    if (particles_.size() > 700) return;
    particles_.push_back(p);
}

void Game::spawnBurst(Vec2 pos, Color color, int count, float speed, float gravity) {
    for (int i = 0; i < count; ++i) {
        float a = static_cast<float>(i) * (2.0f * kPi / count) +
                  hash01(i, iround(time_ * 100.0f), 31) * 0.7f;
        float sp = speed * (0.35f + hash01(i, 9, 31) * 0.75f);
        Particle p;
        p.pos = pos;
        p.vel = Vec2(std::cos(a) * sp, std::sin(a) * sp);
        p.color = mix(color, rgb(255, 255, 255), hash01(i, 11, 31) * 0.5f);
        p.life = 0.45f + hash01(i, 13, 31) * 0.55f;
        p.maxLife = p.life;
        p.size = 1.8f + hash01(i, 15, 31) * 3.2f;
        p.gravity = gravity;
        p.drag = 1.8f;
        p.shape = 0;
        addParticle(p);
    }
}

void Game::spawnFeatherBurst(Vec2 pos, int count) {
    for (int i = 0; i < count; ++i) {
        float a = hash01(i, 21, 37) * 2.0f * kPi;
        float sp = 80.0f + hash01(i, 23, 37) * 240.0f;
        Particle p;
        p.pos = pos;
        p.vel = Vec2(std::cos(a) * sp, std::sin(a) * sp - 40.0f);
        p.color = (i % 3 == 0) ? rgb(122, 178, 255)
                               : ((i % 3 == 1) ? rgb(255, 224, 120) : rgb(255, 255, 240));
        p.life = 0.7f + hash01(i, 25, 37) * 1.0f;
        p.maxLife = p.life;
        p.size = 2.5f + hash01(i, 27, 37) * 3.5f;
        p.gravity = 340.0f;
        p.drag = 0.8f;
        p.shape = 1;
        addParticle(p);
    }
}

void Game::spawnDust(Vec2 pos, int count, Color color) {
    for (int i = 0; i < count; ++i) {
        Particle p;
        p.pos = Vec2(pos.x + (hash01(i, 31, 41) - 0.5f) * 20.0f, pos.y - 2.0f);
        p.vel = Vec2((hash01(i, 33, 41) - 0.5f) * 70.0f, -20.0f - hash01(i, 35, 41) * 55.0f);
        p.color = color;
        p.life = 0.25f + hash01(i, 37, 41) * 0.3f;
        p.maxLife = p.life;
        p.size = 1.5f + hash01(i, 39, 41) * 2.2f;
        p.gravity = 120.0f;
        p.drag = 2.0f;
        p.shape = 0;
        addParticle(p);
    }
}

// ============================================================
//  绘制
// ============================================================
void Game::render() {
    switch (screen_) {
        case Screen::Title: drawTitle(); break;
        case Screen::Controls: drawControls(); break;
        case Screen::Playing: drawPlaying(); break;
        case Screen::Paused:
            drawWorldScene();
            drawHud();
            drawBanner();
            drawPaused();
            break;
        case Screen::Dying:
            drawWorldScene();
            drawHud();
            drawDying();
            break;
        case Screen::Victory: drawVictory(); break;
    }
}

void Game::drawWorldScene() {
    renderer_.clear(rgb(0, 0, 0));
    float ox = std::sin(time_ * 71.0f) * shake_ + std::sin(time_ * 43.0f) * shake_ * 0.45f;
    float oy = std::cos(time_ * 59.0f) * shake_ * 0.7f;
    renderer_.setOffset(iround(ox), iround(oy));
    level_.drawBackdrop(renderer_, time_);
    level_.drawWorld(renderer_, time_);
    if (screen_ != Screen::Dying) player_.draw(renderer_);
    drawParticles();
    level_.drawForeground(renderer_, time_);
    renderer_.resetOffset();
    renderer_.vignette(190);
}

void Game::drawPlaying() {
    drawWorldScene();
    drawHud();
    drawBanner();
    drawToast();
    if (showDebug_) drawDebug();
    if (fade_ > 0.0f) {
        renderer_.alphaRect(RectF(0, 0, kScreenW, kScreenH), rgb(0, 0, 0),
                            iround(fade_ * 255.0f));
    }
}

void Game::drawTitle() {
    titleGradient(renderer_, rgb(10, 9, 28), rgb(45, 30, 66));

    for (int i = 0; i < 120; ++i) {
        float x = hash01(i, 3, 71) * kScreenW;
        float y = hash01(i, 5, 71) * 520.0f;
        float tw = 0.45f + 0.55f * (0.5f + 0.5f * std::sin(time_ * 1.5f + i));
        renderer_.circle(x, y, 0.8f + hash01(i, 7, 71) * 1.3f,
                         mix(rgb(220, 220, 255), rgb(255, 240, 190), hash01(i, 9, 71) * 0.5f));
        if (i % 9 == 0) renderer_.circle(x, y, 2.4f * tw, rgb(255, 250, 220));
    }

    renderer_.circle(1020, 170, 150, rgb(25, 22, 48));
    renderer_.circle(1020, 170, 112, rgb(44, 38, 72));
    renderer_.circle(1020, 170, 82, rgb(255, 236, 190));
    renderer_.circle(1004, 154, 12, rgb(238, 216, 170));
    renderer_.circle(1040, 190, 8, rgb(242, 222, 176));

    for (int i = 0; i < 6; ++i) {
        float x = std::fmod(time_ * (7.0f + i * 2.0f) + i * 245.0f, 1550.0f) - 130.0f;
        float y = 250.0f + i * 44.0f;
        Color c = mix(rgb(54, 44, 80), rgb(30, 26, 52), 0.35f + i * 0.07f);
        renderer_.fillRoundRect(RectF(x - 74, y - 16, 148, 32), 16, c);
        renderer_.fillRoundRect(RectF(x - 42, y - 30, 84, 28), 14, brighten(c, 0.03f));
        renderer_.fillRoundRect(RectF(x + 14, y - 24, 58, 24), 12, c);
    }

    Vec2 m1[3] = {Vec2(-80, 640), Vec2(280, 330), Vec2(640, 640)};
    renderer_.polygon(m1, 3, rgb(32, 25, 52));
    Vec2 m2[3] = {Vec2(260, 660), Vec2(700, 290), Vec2(1160, 660)};
    renderer_.polygon(m2, 3, rgb(24, 20, 42));
    Vec2 snow[3] = {Vec2(650, 350), Vec2(700, 290), Vec2(750, 350)};
    renderer_.polygon(snow, 3, rgb(78, 70, 108));
    Vec2 m3[3] = {Vec2(900, 670), Vec2(1200, 380), Vec2(1460, 670)};
    renderer_.polygon(m3, 3, rgb(36, 28, 56));

    for (int i = 0; i < 22; ++i) {
        float x = std::fmod(hash01(i, 11, 73) * kScreenW + time_ * (9.0f + i * 0.9f), 1400.0f) - 60.0f;
        float y = 100.0f + hash01(i, 13, 73) * 480.0f + std::sin(time_ * 1.2f + i) * 20.0f;
        renderer_.circle(x - 2, y, 2.2f, rgb(255, 224, 120));
        renderer_.line(Vec2(x - 2, y - 4), Vec2(x + 4, y + 5), rgb(255, 190, 90), 1);
    }

    renderer_.vignette(210);

    // 标题
    renderer_.textShadow(L"I WANNA", 92, 78, 104, rgb(255, 248, 232),
                         rgb(18, 10, 26), 6, TextAlign::Left, true, L"Arial Black");
    renderer_.text(L"啾比大冒险", 98, 206, 54, rgb(255, 214, 100), TextAlign::Left, true, kUiFont);
    renderer_.text(L"一只不会飞的小鸟，和一座不打算讲道理的高塔。", 102, 280, 18,
                   rgb(190, 184, 210), TextAlign::Left, false, kUiFont);
    renderer_.fillRect(RectF(100, 316, 96, 3), rgb(255, 214, 100));
    renderer_.fillRect(RectF(202, 316, 36, 3), rgb(100, 88, 130));

    // 菜单
    const wchar_t* labels[4] = {L"开始游戏", L"继续游戏", L"操作说明", L"退出"};
    for (int i = 0; i < 4; ++i) {
        bool disabled = (i == 1 && !hasSave_);
        std::wstring label = labels[i];
        if (i == 1 && hasSave_) {
            label += L"   · 第 " + intText(savePreview_.room + 1) + L" 关";
        }
        drawMenuItem(RectF(100, 380 + static_cast<float>(i * 64), 360, 54), label,
                     titleMenu_ == i, disabled);
    }

    // 吉祥物
    float bob = std::sin(time_ * 2.2f) * 7.0f;
    renderer_.ellipse(RectF(850, 512, 280, 28), rgb(18, 14, 32));
    renderer_.fillRoundRect(RectF(852, 468, 276, 44), 22, rgb(38, 30, 60));
    renderer_.fillRoundRect(RectF(872, 458, 236, 36), 18, rgb(52, 40, 76));
    renderer_.line(Vec2(920, 468), Vec2(920, 500), rgb(96, 76, 130), 3);
    renderer_.line(Vec2(1060, 468), Vec2(1060, 500), rgb(96, 76, 130), 3);
    drawJubi(renderer_, 992, 404 + bob, 2.35f, true, time_, time_ * 2.0f, false, 0.0f, 0.0f);

    // 对话气泡
    float bx = 1080;
    float by = 278 + std::sin(time_ * 1.7f) * 4.0f;
    renderer_.fillRoundRect(RectF(bx, by, 138, 58), 18, rgb(255, 250, 232));
    renderer_.triangle(Vec2(bx + 14, by + 42), Vec2(bx - 8, by + 64), Vec2(bx + 34, by + 52),
                       rgb(255, 250, 232));
    renderer_.text(L"啾！", bx + 69, by + 12, 26, rgb(54, 40, 38), TextAlign::Center, true, kUiFont);

    renderer_.text(L"↑ ↓ 选择     ENTER / 鼠标点击 确认", 100, 646, 14, rgb(160, 152, 180),
                   TextAlign::Left, false, kUiFont);
    renderer_.text(L"A / D 移动 · SPACE 二段跳 · R 重来 · ESC 暂停 · F11 全屏",
                   1252, 681, 13, rgb(132, 126, 150), TextAlign::Right, false, kUiFont);
    renderer_.text(L"C++ 课程设计 · Win32 / GDI · 无外部素材", 1252, 700, 11,
                   rgb(96, 90, 112), TextAlign::Right, false, kUiFont);

    if (toastTimer_ > 0.0f) drawToast();
    if (fade_ > 0.0f) {
        renderer_.alphaRect(RectF(0, 0, kScreenW, kScreenH), rgb(0, 0, 0),
                            iround(fade_ * 255.0f));
    }
}

void Game::drawMenuItem(const RectF& rect, const std::wstring& label, bool selected,
                        bool disabled, bool arrow) {
    if (selected) {
        renderer_.alphaRect(RectF(rect.x + 4, rect.y + 8, rect.w, rect.h), rgb(0, 0, 0), 80);
        renderer_.fillRoundRect(rect, 12, rgb(255, 214, 100));
        renderer_.fillRoundRect(RectF(rect.x + 2, rect.y + 2, rect.w - 4, 5), 3,
                                rgb(255, 246, 190));
        renderer_.strokeRoundRect(rect, 12, rgb(255, 248, 210), 2);
        if (arrow) {
            renderer_.triangle(Vec2(rect.x + 18, rect.y + rect.h * 0.5f - 6),
                               Vec2(rect.x + 30, rect.y + rect.h * 0.5f),
                               Vec2(rect.x + 18, rect.y + rect.h * 0.5f + 6),
                               rgb(70, 50, 30));
        }
        renderer_.text(label, rect.x + 46, rect.y + 12, 23, rgb(54, 38, 26),
                       TextAlign::Left, true, kUiFont);
    } else {
        renderer_.alphaRect(rect, rgb(10, 8, 18), 105);
        renderer_.strokeRoundRect(rect, 12, rgb(108, 96, 132), 1);
        Color c = disabled ? rgb(95, 90, 104) : rgb(238, 232, 220);
        renderer_.text(label, rect.x + 28, rect.y + 12, 23, c, TextAlign::Left, true, kUiFont);
        if (disabled) {
            renderer_.line(Vec2(rect.x + 28, rect.y + 34), Vec2(rect.x + 150, rect.y + 20),
                           rgb(120, 100, 108), 1);
        }
    }
}

void Game::drawControls() {
    titleGradient(renderer_, rgb(9, 8, 24), rgb(34, 24, 52));
    for (int i = 0; i < 80; ++i) {
        float x = hash01(i, 3, 91) * kScreenW;
        float y = hash01(i, 5, 91) * 600.0f;
        renderer_.circle(x, y, 1.0f, rgb(150, 145, 190));
    }
    renderer_.alphaRect(RectF(0, 0, kScreenW, kScreenH), rgb(0, 0, 0), 60);
    renderer_.vignette(190);
    renderer_.fillRoundRect(RectF(212, 72, 856, 570), 22, rgb(16, 14, 28));
    renderer_.strokeRoundRect(RectF(212, 72, 856, 570), 22, rgb(120, 104, 150), 1);
    renderer_.text(L"操作说明", 640, 106, 40, rgb(255, 220, 120), TextAlign::Center, true, kUiFont);
    renderer_.text(L"别担心，啾比摔不死——它只是会重新开始。", 640, 158, 16,
                   rgb(176, 168, 196), TextAlign::Center, false, kUiFont);

    drawKeycap(renderer_, 286, 210, 104, L"A / D", rgb(120, 180, 255));
    renderer_.text(L"左右移动（也可以用 ← →）", 414, 216, 18, rgb(236, 230, 220),
                   TextAlign::Left, false, kUiFont);
    drawKeycap(renderer_, 286, 258, 104, L"SPACE", rgb(255, 210, 120));
    renderer_.text(L"跳跃；在空中再按一次 = 二段跳", 414, 264, 18, rgb(236, 230, 220),
                   TextAlign::Left, false, kUiFont);
    renderer_.text(L"短按小跳，长按跳得更高。", 414, 288, 13, rgb(150, 144, 170),
                   TextAlign::Left, false, kUiFont);
    drawKeycap(renderer_, 286, 324, 104, L"R", rgb(255, 150, 120));
    renderer_.text(L"重新开始本关", 414, 330, 18, rgb(236, 230, 220),
                   TextAlign::Left, false, kUiFont);
    drawKeycap(renderer_, 286, 372, 104, L"ESC", rgb(190, 170, 255));
    renderer_.text(L"暂停 / 返回", 414, 378, 18, rgb(236, 230, 220),
                   TextAlign::Left, false, kUiFont);
    drawKeycap(renderer_, 286, 420, 104, L"M", rgb(160, 220, 200));
    renderer_.text(L"静音", 414, 426, 18, rgb(236, 230, 220),
                   TextAlign::Left, false, kUiFont);
    drawKeycap(renderer_, 286, 468, 104, L"F11", rgb(160, 200, 255));
    renderer_.text(L"全屏 / 返回窗口（不拉伸比例）", 414, 474, 18, rgb(236, 230, 220),
                   TextAlign::Left, false, kUiFont);
    drawKeycap(renderer_, 286, 516, 104, L"F12", rgb(255, 190, 140));
    renderer_.text(L"把这一刻存成 BMP 截图", 414, 522, 18, rgb(236, 230, 220),
                   TextAlign::Left, false, kUiFont);

    renderer_.text(L"目标：收集 6 根星羽，走到每一关的「出口」。", 286, 570, 17,
                   rgb(255, 214, 120), TextAlign::Left, false, kUiFont);
    renderer_.text(L"手感细节：土狼时间 0.1s · 跳跃缓冲 0.12s · 移动平台会带着你走 · 单向平台可以从下方穿过",
                   286, 596, 13, rgb(160, 152, 184), TextAlign::Left, false, kUiFont);
    renderer_.text(L"ESC / ENTER / 鼠标点击 返回标题", 640, 660, 15, rgb(220, 210, 190),
                   TextAlign::Center, false, kUiFont);
}

void Game::drawHud() {
    renderer_.alphaRect(RectF(0, 0, kScreenW, 84), rgb(8, 8, 16), 148);
    renderer_.fillRect(RectF(0, 82, kScreenW, 2), mix(level_.accent(), rgb(0, 0, 0), 0.25f));

    renderer_.text(L"I WANNA", 26, 7, 11, level_.accent(), TextAlign::Left, true, L"Arial Black");
    renderer_.text(level_.name(), 26, 22, 26, rgb(250, 246, 234), TextAlign::Left, true, kUiFont);
    renderer_.text(L"第 " + intText(roomIndex_ + 1) + L" 关 / 6   ·   " + level_.subtitle(),
                   26, 59, 11, rgb(174, 168, 190), TextAlign::Left, false, kUiFont);

    renderer_.fillRect(RectF(1032, 20, 1, 44), rgb(64, 60, 80));
    renderer_.fillRect(RectF(1150, 20, 1, 44), rgb(64, 60, 80));

    drawTinyFeather(renderer_, 938, 22, 0.78f, rgb(255, 220, 110));
    renderer_.text(intText(featherCount()) + L" / 6", 988, 8, 22, rgb(255, 226, 140),
                   TextAlign::Center, true, kUiFont);
    renderer_.text(L"星羽", 988, 43, 10, rgb(154, 148, 172), TextAlign::Center, false, kUiFont);

    renderer_.text(intText(deaths_) + L" 次", 1092, 8, 22, rgb(255, 168, 140),
                   TextAlign::Center, true, kUiFont);
    renderer_.text(L"死亡", 1092, 43, 10, rgb(154, 148, 172), TextAlign::Center, false, kUiFont);

    renderer_.text(formatTime(playTime_), 1216, 8, 21, rgb(248, 244, 232),
                   TextAlign::Center, true, L"Consolas");
    renderer_.text(L"时间", 1216, 43, 10, rgb(154, 148, 172), TextAlign::Center, false, kUiFont);

    if (hintTimer_ > 0.0f && screen_ == Screen::Playing) {
        float a = clampf(hintTimer_, 0.0f, 1.0f);
        Color c = mix(rgb(90, 84, 104), rgb(220, 214, 226), a);
        renderer_.textShadow(L"A / D 移动   ·   SPACE 跳跃（可二段）   ·   R 重来   ·   ESC 暂停",
                             640, 701, 11, c, rgb(10, 8, 16), 1,
                             TextAlign::Center, false, kUiFont);
    }
    if (showDebug_) {
        renderer_.text(L"F3 调试", 1252, 664, 10, rgb(120, 220, 160),
                       TextAlign::Right, false, kUiFont);
    }
}

void Game::drawBanner() {
    if (bannerTimer_ <= 0.0f) return;
    float in = clampf((3.2f - bannerTimer_) / 0.28f, 0.0f, 1.0f);
    float out = clampf(bannerTimer_ / 0.65f, 0.0f, 1.0f);
    float a = in * out;
    if (a <= 0.01f) return;
    float slide = (1.0f - smoothstep(in)) * -20.0f;
    renderer_.alphaRect(RectF(220, 112 + slide, 840, 144), rgb(4, 4, 10), iround(105.0f * a));
    renderer_.fillRect(RectF(220, 112 + slide, 5, 144), level_.accent());
    renderer_.fillRect(RectF(1055, 112 + slide, 5, 144), level_.accent());
    renderer_.text(level_.name(), 640, 128 + slide, 46, rgb(250, 246, 232),
                   TextAlign::Center, true, kUiFont);
    renderer_.text(level_.subtitle(), 640, 186 + slide, 16, level_.accent(),
                   TextAlign::Center, false, kUiFont);
    renderer_.text(level_.quote(), 640, 216 + slide, 15, rgb(180, 174, 196),
                   TextAlign::Center, false, kUiFont);
}

void Game::drawPaused() {
    renderer_.alphaRect(RectF(0, 0, kScreenW, kScreenH), rgb(3, 3, 8), 175);
    RectF panel(370, 150, 540, 410);
    renderer_.alphaRect(RectF(panel.x + 10, panel.y + 12, panel.w, panel.h), rgb(0, 0, 0), 100);
    renderer_.fillRoundRect(panel, 20, rgb(18, 16, 30));
    renderer_.strokeRoundRect(panel, 20, rgb(130, 114, 158), 1);
    renderer_.text(L"暂停", 640, 176, 44, rgb(255, 220, 120), TextAlign::Center, true, kUiFont);
    renderer_.text(L"深呼吸。尖刺不会跑。", 640, 228, 14, rgb(176, 168, 196),
                   TextAlign::Center, false, kUiFont);

    const wchar_t* items[3] = {L"继续游戏", L"重新开始本关", L"返回标题"};
    for (int i = 0; i < 3; ++i) {
        drawMenuItem(RectF(430, 274 + static_cast<float>(i * 64), 420, 50), items[i],
                     pauseMenu_ == i, false);
    }
    renderer_.text(L"时间 " + formatTime(playTime_) + L"    死亡 " + intText(deaths_) +
                       L"    星羽 " + intText(featherCount()) + L" / 6",
                   640, 492, 13, rgb(160, 154, 178), TextAlign::Center, false, kUiFont);
    renderer_.text(L"A / D 移动   ·   SPACE 跳跃   ·   ESC 继续   ·   也可点击菜单", 640, 522, 12,
                   rgb(130, 124, 146), TextAlign::Center, false, kUiFont);
}

void Game::drawDying() {
    float t = clampf(deathTimer_ / 0.72f, 0.0f, 1.0f);
    renderer_.alphaRect(RectF(0, 0, kScreenW, kScreenH), rgb(170, 20, 34),
                        iround(55.0f * t));
    renderer_.text(L"啾——！", 640, 270, 54, rgb(255, 220, 190), TextAlign::Center, true, kUiFont);
    renderer_.text(L"你死了 " + intText(deaths_) + L" 次", 640, 344, 20,
                   rgb(255, 190, 160), TextAlign::Center, true, kUiFont);
    renderer_.text(L"回到最近的检查点……", 640, 382, 13, rgb(210, 180, 180),
                   TextAlign::Center, false, kUiFont);
    if (fade_ > 0.0f) {
        renderer_.alphaRect(RectF(0, 0, kScreenW, kScreenH), rgb(0, 0, 0),
                            iround(fade_ * 255.0f));
    }
}

void Game::drawVictory() {
    titleGradient(renderer_, rgb(12, 10, 28), rgb(44, 26, 58));
    for (int i = 0; i < 110; ++i) {
        float x = hash01(i, 3, 111) * kScreenW;
        float y = hash01(i, 5, 111) * 560.0f;
        renderer_.circle(x, y, 1.0f, rgb(255, 232, 180));
    }
    renderer_.circle(640, 300, 340, rgb(26, 20, 42));
    renderer_.circle(640, 300, 250, rgb(40, 30, 58));
    renderer_.vignette(200);

    drawParticles();

    renderer_.textShadow(L"通关！", 640, 62, 88, rgb(255, 232, 160),
                         rgb(40, 18, 20), 6, TextAlign::Center, true, kUiFont);
    renderer_.text(L"啾比飞出了高塔。", 640, 172, 26, rgb(240, 232, 220),
                   TextAlign::Center, false, kUiFont);
    renderer_.text(L"虽然它还是不会飞，但它学会了不往下看。", 640, 210, 15,
                   rgb(178, 172, 198), TextAlign::Center, false, kUiFont);

    RectF panel(310, 256, 660, 300);
    renderer_.alphaRect(RectF(panel.x + 8, panel.y + 10, panel.w, panel.h), rgb(0, 0, 0), 90);
    renderer_.fillRoundRect(panel, 22, rgb(20, 17, 32));
    renderer_.strokeRoundRect(panel, 22, rgb(150, 126, 100), 1);
    renderer_.fillRect(RectF(350, 292, 180, 2), rgb(255, 214, 120));

    auto stat = [&](float y, const wchar_t* label, const std::wstring& value, Color c) {
        renderer_.text(label, 360, y, 17, rgb(178, 170, 192), TextAlign::Left, false, kUiFont);
        renderer_.text(value, 870, y - 2, 24, c, TextAlign::Right, true, kUiFont);
    };
    stat(308, L"总用时", formatTime(playTime_), rgb(245, 240, 228));
    stat(356, L"死亡次数", intText(deaths_) + L" 次", rgb(255, 168, 136));
    stat(404, L"星羽收集", intText(featherCount()) + L" / 6", rgb(255, 222, 128));

    Color rc = rankColor();
    renderer_.text(L"评价", 360, 456, 17, rgb(178, 170, 192), TextAlign::Left, false, kUiFont);
    renderer_.text(rankText(), 870, 438, 62, rc, TextAlign::Right, true, kUiFont);

    std::wstring recordLine = L"最佳纪录  " +
        (record_.valid ? formatTime(record_.bestTime) : std::wstring(L"未记录"));
    if (record_.valid) {
        recordLine += L"   ·   " + intText(record_.bestDeaths) + L" 次死亡";
    }
    renderer_.text(recordLine, 640, 574, 12, rgb(148, 142, 166),
                   TextAlign::Center, false, kUiFont);
    renderer_.text(L"按 ENTER 返回标题", 640, 650, 17, rgb(240, 222, 180),
                   TextAlign::Center, false, kUiFont);

    if (fade_ > 0.0f) {
        renderer_.alphaRect(RectF(0, 0, kScreenW, kScreenH), rgb(0, 0, 0),
                            iround(fade_ * 255.0f));
    }
}

void Game::drawParticles() {
    for (const auto& p : particles_) {
        float a = clampf(p.life / p.maxLife, 0.0f, 1.0f);
        float s = p.size * (0.35f + 0.65f * a);
        if (s < 0.3f) continue;
        if (p.shape == 1) {
            renderer_.line(Vec2(p.pos.x, p.pos.y - s * 1.4f), Vec2(p.pos.x, p.pos.y + s * 1.4f),
                           p.color, (std::max)(1, iround(s * 0.55f)));
            renderer_.ellipse(RectF(p.pos.x - s * 0.45f, p.pos.y - s * 0.9f, s * 1.1f, s * 1.8f),
                              p.color);
        } else {
            renderer_.circle(p.pos.x, p.pos.y, s, p.color);
        }
    }
}

void Game::drawToast() {
    if (toastTimer_ <= 0.0f || toast_.empty()) return;
    float a = clampf(toastTimer_ / 0.25f, 0.0f, 1.0f);
    if (a <= 0.01f) return;
    int w = renderer_.measureText(toast_, 14, true, kUiFont) + 48;
    RectF box(640 - w * 0.5f, 636, static_cast<float>(w), 34);
    renderer_.alphaRect(RectF(box.x + 4, box.y + 5, box.w, box.h), rgb(0, 0, 0), 90);
    renderer_.fillRoundRect(box, 10, rgb(26, 24, 38));
    renderer_.strokeRoundRect(box, 10, mix(level_.accent(), rgb(255, 255, 255), 0.15f), 1);
    renderer_.text(toast_, 640, 643, 14, rgb(255, 240, 208), TextAlign::Center, true, kUiFont);
}

void Game::drawDebug() {
    renderer_.alphaRect(RectF(24, 96, 380, 150), rgb(0, 0, 0), 150);
    wchar_t buf[256];
    std::swprintf(buf, 256, L"room %d   particles %d   deaths %d",
                  roomIndex_ + 1, static_cast<int>(particles_.size()), deaths_);
    renderer_.text(buf, 36, 104, 12, rgb(140, 240, 180), TextAlign::Left, false, L"Consolas");
    std::swprintf(buf, 256, L"pos %.1f, %.1f   vel %.1f, %.1f",
                  player_.box.x, player_.box.y, player_.vel.x, player_.vel.y);
    renderer_.text(buf, 36, 124, 12, rgb(140, 240, 180), TextAlign::Left, false, L"Consolas");
    std::swprintf(buf, 256, L"ground %d   jumps %d   mover %d",
                  player_.onGround ? 1 : 0, player_.jumps, player_.standingMover);
    renderer_.text(buf, 36, 144, 12, rgb(140, 240, 180), TextAlign::Left, false, L"Consolas");
    std::swprintf(buf, 256, L"time %.2f   fade %.2f   shake %.1f",
                  playTime_, fade_, shake_);
    renderer_.text(buf, 36, 164, 12, rgb(140, 240, 180), TextAlign::Left, false, L"Consolas");
    renderer_.text(L"draw: hitboxes / collider debug", 36, 184, 12, rgb(140, 240, 180),
                   TextAlign::Left, false, L"Consolas");
}

// ============================================================
//  辅助
// ============================================================
int Game::featherCount() const {
    int n = 0;
    for (int i = 0; i < 6; ++i) if (featherMask_ & (1 << i)) ++n;
    return n;
}

std::wstring Game::rankText() const {
    if (deaths_ <= 5 && playTime_ < 240.0f) return L"S";
    if (deaths_ <= 12) return L"A";
    if (deaths_ <= 25) return L"B";
    if (deaths_ <= 45) return L"C";
    return L"D";
}

Color Game::rankColor() const {
    std::wstring r = rankText();
    if (r == L"S") return rgb(255, 220, 100);
    if (r == L"A") return rgb(150, 230, 180);
    if (r == L"B") return rgb(140, 190, 255);
    if (r == L"C") return rgb(230, 170, 120);
    return rgb(170, 150, 160);
}

void Game::updateCamera(float dt) {
    shake_ = approach(shake_, 0.0f, dt * 38.0f);
}

void Game::showToast(const std::wstring& text) {
    toast_ = text;
    toastTimer_ = 2.0f;
}

void Game::prepareScreenshot(int room, const std::wstring& state) {
    if (state == L"title" || (state.empty() && room < 0)) {
        screen_ = Screen::Title;
        titleMenu_ = 0;
        fade_ = 0.0f;
        time_ = 2.0f;
        return;
    }
    if (state == L"controls") {
        controlsReturn_ = Screen::Title;
        screen_ = Screen::Controls;
        fade_ = 0.0f;
        return;
    }

    startNewGame();
    if (room >= 0) loadRoom(room, false);
    bannerTimer_ = 0.0f;
    hintTimer_ = 0.0f;
    fade_ = 0.0f;
    if (state == L"paused") {
        screen_ = Screen::Paused;
        pauseMenu_ = 0;
    } else if (state == L"victory") {
        deaths_ = 17;
        playTime_ = 192.4f;
        featherMask_ = 63;
        screen_ = Screen::Victory;
        particles_.clear();
    } else {
        screen_ = Screen::Playing;
        InputState none;
        level_.update(1.0f / 120.0f, player_.box, player_.onGround, audio_);
        player_.update(1.0f / 120.0f, none, level_, audio_);
    }
}

bool Game::runAutomatedTest(int room, const std::wstring& test, const std::wstring& outPath) {
    room = (std::max)(0, (std::min)(5, room));
    roomIndex_ = room;
    featherMask_ = 0;
    deaths_ = 0;
    playTime_ = 0.0f;
    level_.setFeatherMask(0);
    level_.load(room);
    player_.reset(level_.spawnPoint());
    screen_ = Screen::Playing;
    fade_ = 0.0f;
    bannerTimer_ = 0.0f;
    hintTimer_ = 0.0f;
    particles_.clear();
    toastTimer_ = 0.0f;
    audio_.setMuted(true);

    std::filesystem::path logPath(outPath);
    logPath.replace_extension(L".txt");
    std::ofstream log(logPath);
    log << "room=" << room << "\n";
    log << "t,x,y,vx,vy,onGround,jumps,deaths,screen,roomNow\n";

    const float dt = 1.0f / 120.0f;
    bool jumpIssued = false;
    bool doubleIssued = false;
    bool bounceSeen = false;
    bool doorJump1 = false;
    bool doorJump2 = false;
    bool doorJump3 = false;
    bool doorJump4 = false;
    float doorHold = 0.0f;
    int maxSteps = 120 * 8;
    for (int step = 0; step < maxSteps; ++step) {
        float t = step * dt;
        InputState in;
        if (test == L"run") {
            in.right = (t < 0.8f);
        } else if (test == L"jump" || test == L"double") {
            in.right = (t < 2.0f);
            if (t >= 0.80f && !jumpIssued) {
                in.jumpPressed = true;
                jumpIssued = true;
            }
            if (t >= 0.80f && t < 1.04f) in.jumpHeld = true;
            if (test == L"double" && t >= 1.06f && !doubleIssued) {
                in.jumpPressed = true;
                doubleIssued = true;
            }
            if (test == L"double" && t >= 1.06f && t < 1.24f) in.jumpHeld = true;
        } else if (test == L"bounce") {
            in.right = (t < 0.55f);
            if (bounceSeen) in.right = (player_.box.x < 385.0f);
            if (player_.vel.y < -900.0f) bounceSeen = true;
        } else if (test == L"gap") {
            in.right = true;
            if (t >= 0.90f && !jumpIssued) {
                in.jumpPressed = true;
                jumpIssued = true;
            }
            if (t >= 0.90f && t < 1.05f) in.jumpHeld = true;
            if (t >= 1.20f && !doubleIssued) {
                in.jumpPressed = true;
                doubleIssued = true;
            }
            if (t >= 1.20f && t < 1.36f) in.jumpHeld = true;
        } else if (test == L"door") {
            in.right = true;
            float x = player_.box.x;
            if (x > 286.0f && player_.onGround && !doorJump1) {
                in.jumpPressed = true;
                doorHold = 0.22f;
                doorJump1 = true;
            } else if (x > 425.0f && !player_.onGround && !doorJump2) {
                in.jumpPressed = true;
                doorHold = 0.22f;
                doorJump2 = true;
            } else if (x > 836.0f && player_.onGround && !doorJump3) {
                in.jumpPressed = true;
                doorHold = 0.22f;
                doorJump3 = true;
            } else if (x > 905.0f && !player_.onGround && !doorJump4) {
                in.jumpPressed = true;
                doorHold = 0.22f;
                doorJump4 = true;
            }
            if (doorHold > 0.0f) {
                in.jumpHeld = true;
                doorHold -= dt;
            }
            if (x > 930.0f && player_.box.y < 560.0f) in.jumpHeld = true;
        } else {
            // idle
        }
        update(dt, in);
        if (step % 6 == 0 || step == maxSteps - 1) {
            log << t << "," << player_.box.x << "," << player_.box.y << ","
                << player_.vel.x << "," << player_.vel.y << ","
                << (player_.onGround ? 1 : 0) << "," << player_.jumps << ","
                << deaths_ << "," << static_cast<int>(screen_) << "," << roomIndex_ << "\n";
        }
        if (test == L"door" && roomIndex_ != room) break;
        if (screen_ == Screen::Victory) break;
        if (deaths_ > 0 && screen_ == Screen::Playing && step > 120 * 2) {
            // keep running a little after the first respawn for diagnostics
        }
    }
    log.close();
    render();
    return saveScreenshot(outPath);
}

bool Game::saveScreenshot(const std::wstring& path) {
    renderer_.saveBMP(path);
    return true;
}

} // namespace jubi
