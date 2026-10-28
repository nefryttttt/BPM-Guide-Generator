#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/modify/LevelSettingsObject.hpp>
#include <Geode/ui/TextInput.hpp>
#include <chrono>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <map>

#ifdef GEODE_IS_WINDOWS
#include <windows.h>
#include <shellapi.h>
#endif

using namespace geode::prelude;

static float g_bpm = 120.0f;
static float g_offset = 0.0f;
static std::string g_gridStepStr = "1/4";
static int g_selectedSpeedIndex = 1; // По умолчанию 1x (Normal)

class BPMGuidePopup : public Popup {
protected:
    TextInput* m_bpmInput = nullptr;
    TextInput* m_offsetInput = nullptr;
    TextInput* m_customGridInput = nullptr;
    std::string m_gridStep = "1/4";
    float m_exactBtnWidth = 38.0f;
    
    struct SubBtn {
        CCMenuItemSpriteExtra* btn;
        CCNode* greenSpr;
        CCNode* blueSpr;
        std::string valName;
    };
    std::vector<SubBtn> m_gridBtns;
    std::vector<SubBtn> m_speedBtns;
    std::vector<double> m_taps;

public:
    static BPMGuidePopup* create() {
        auto ret = new BPMGuidePopup();
        if (ret && (*ret).init(420.f, 250.f)) {
            (*ret).autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    bool init(float w, float h) {
        if (!Popup::init(w, h)) return false;

        auto& ml = *m_mainLayer;

        auto title = CCLabelBMFont::create("BPM Guide Generator", "bigFont.fnt");
        (*title).setScale(0.52f);
        (*title).setPosition({m_size.width / 2.0f, m_size.height - 20.f});
        ml.addChild(title);

        auto menu = CCMenu::create();
        (*menu).setPosition({0, 0});
        ml.addChild(menu);

        float fieldX = 145.f;

        auto bpmLabel = CCLabelBMFont::create("BPM:", "goldFont.fnt");
        (*bpmLabel).setScale(0.45f);
        (*bpmLabel).setAnchorPoint({0.0f, 0.5f});
        (*bpmLabel).setPosition({25.f, m_size.height - 50.f});
        ml.addChild(bpmLabel);

        m_bpmInput = TextInput::create(70.f, fmt::format("{:.1f}", g_bpm), "bigFont.fnt");
        (*m_bpmInput).setFilter("0123456789.");
        (*m_bpmInput).setMaxCharCount(6);
        (*m_bpmInput).setScale(0.6f);
        (*m_bpmInput).setPosition({fieldX, m_size.height - 50.f});
        ml.addChild(m_bpmInput);

        auto tapSpr = ButtonSprite::create("Tap", 40, 0, 0.4f, true, "bigFont.fnt", "GJ_button_02.png");
        (*tapSpr).setScale(0.65f);
        auto tapBtn = CCMenuItemSpriteExtra::create(
            tapSpr, this, menu_selector(BPMGuidePopup::onTap)
        );
        (*tapBtn).setPosition({fieldX + 52.f, m_size.height - 50.f});
        (*menu).addChild(tapBtn);

        auto autoSpr = ButtonSprite::create("Auto", 40, 0, 0.4f, true, "bigFont.fnt", "GJ_button_03.png");
        (*autoSpr).setScale(0.65f);
        auto autoBtn = CCMenuItemSpriteExtra::create(
            autoSpr, this, menu_selector(BPMGuidePopup::onAutoBPM)
        );
        (*autoBtn).setPosition({fieldX + 102.f, m_size.height - 50.f});
        (*menu).addChild(autoBtn);

        auto offsetLabel = CCLabelBMFont::create("Offset (ms):", "goldFont.fnt");
        (*offsetLabel).setScale(0.45f);
        (*offsetLabel).setAnchorPoint({0.0f, 0.5f});
        (*offsetLabel).setPosition({25.f, m_size.height - 85.f});
        ml.addChild(offsetLabel);

        m_offsetInput = TextInput::create(70.f, fmt::format("{:.0f}", g_offset), "bigFont.fnt");
        (*m_offsetInput).setFilter("0123456789.-");
        (*m_offsetInput).setMaxCharCount(6);
        (*m_offsetInput).setScale(0.6f);
        (*m_offsetInput).setPosition({fieldX, m_size.height - 85.f});
        ml.addChild(m_offsetInput);

        auto speedLabel = CCLabelBMFont::create("Start Speed:", "goldFont.fnt");
        (*speedLabel).setScale(0.45f);
        (*speedLabel).setAnchorPoint({0.0f, 0.5f});
        (*speedLabel).setPosition({25.f, m_size.height - 120.f});
        ml.addChild(speedLabel);

        const char* speedNames[] = { "0.5x", "1x", "2x", "3x", "4x" };
        float speedStartX = 115.f;
        float speedSpacing = 52.0f;

        for (int i = 0; i < 5; ++i) {
            bool selected = (i == g_selectedSpeedIndex);

            auto greenSpr = ButtonSprite::create(speedNames[i], 44, 24, 0.45f, true, "bigFont.fnt", "GJ_button_01.png");
            (*greenSpr).setScale(0.55f);

            auto blueSpr = ButtonSprite::create(speedNames[i], 44, 24, 0.45f, true, "bigFont.fnt", "GJ_button_02.png");
            (*blueSpr).setScale(0.55f);

            auto container = CCNode::create();
            CCSize sprSize = (*greenSpr).getScaledContentSize();
            (*container).setContentSize(sprSize);

            (*greenSpr).setPosition(sprSize / 2.0f);
            (*blueSpr).setPosition(sprSize / 2.0f);

            (*greenSpr).setVisible(!selected);
            (*blueSpr).setVisible(selected);

            (*container).addChild(greenSpr);
            (*container).addChild(blueSpr);

            auto btn = CCMenuItemSpriteExtra::create(container, this, menu_selector(BPMGuidePopup::onSpeedSelect));
            (*btn).setTag(i);
            (*btn).setPosition({speedStartX + (i * speedSpacing), m_size.height - 120.f});
            (*menu).addChild(btn);

            m_speedBtns.push_back({btn, greenSpr, blueSpr, speedNames[i]});
        }

        auto subLabel = CCLabelBMFont::create("Grid Step:", "goldFont.fnt");
        (*subLabel).setScale(0.45f);
        (*subLabel).setAnchorPoint({0.0f, 0.5f});
        (*subLabel).setPosition({25.f, m_size.height - 155.f});
        ml.addChild(subLabel);

        m_gridStep = g_gridStepStr;
        const char* gridNames[] = { "1/2", "1/4", "1/8" };

        float rowStartX = 115.f;
        float spacing = 43.0f; 

        auto sampleSpr = ButtonSprite::create("1/4", 38, 24, 0.5f, true, "bigFont.fnt", "GJ_button_01.png");
        m_exactBtnWidth = sampleSpr->getContentSize().width;

        for (int i = 0; i < 3; ++i) {
            bool selected = (gridNames[i] == m_gridStep);

            auto greenBtnSpr = ButtonSprite::create(gridNames[i], 38, 24, 0.5f, true, "bigFont.fnt", "GJ_button_01.png");
            (*greenBtnSpr).setScale(0.6f);

            auto blueBtnSpr = ButtonSprite::create(gridNames[i], 38, 24, 0.5f, true, "bigFont.fnt", "GJ_button_02.png");
            (*blueBtnSpr).setScale(0.6f);

            auto container = CCNode::create();
            CCSize sprSize = (*greenBtnSpr).getScaledContentSize();
            (*container).setContentSize(sprSize);

            (*greenBtnSpr).setPosition(sprSize / 2.0f);
            (*blueBtnSpr).setPosition(sprSize / 2.0f);

            (*greenBtnSpr).setVisible(!selected);
            (*blueBtnSpr).setVisible(selected);

            (*container).addChild(greenBtnSpr);
            (*container).addChild(blueBtnSpr);

            auto btn = CCMenuItemSpriteExtra::create(container, this, menu_selector(BPMGuidePopup::onGridSelect));
            (*btn).setTag(i);
            (*btn).setPosition({rowStartX + (i * spacing), m_size.height - 155.f});
            (*menu).addChild(btn);
            
            m_gridBtns.push_back({btn, greenBtnSpr, blueBtnSpr, gridNames[i]});
        }

        bool isPreset = false;
        for (auto& name : gridNames) {
            if (m_gridStep == name) {
                isPreset = true;
                break;
            }
        }

        std::string placeholderVal = isPreset ? m_gridStep : "";
        std::string initialText = isPreset ? "" : m_gridStep;

        m_customGridInput = TextInput::create(m_exactBtnWidth, placeholderVal, "bigFont.fnt");
        (*m_customGridInput).setFilter("0123456789./");
        (*m_customGridInput).setMaxCharCount(6);
        (*m_customGridInput).setScale(0.6f);
        (*m_customGridInput).setPosition({rowStartX + (3 * spacing), m_size.height - 155.f});
        
        if (!initialText.empty()) {
            (*m_customGridInput).setString(initialText);
        }

        (*m_customGridInput).setCallback([this](std::string const& text) {
            if (!text.empty()) {
                m_gridStep = text;
                updateGridButtonStates(""); 
            }
        });
        ml.addChild(m_customGridInput);

        auto genSpr = ButtonSprite::create("Generate", 95, 32, 0.5f, true, "bigFont.fnt", "GJ_button_01.png");
        (*genSpr).setScale(0.7f);
        auto genBtn = CCMenuItemSpriteExtra::create(genSpr, this, menu_selector(BPMGuidePopup::onGenerate));
        (*genBtn).setPosition({m_size.width / 2.0f - 100.f, 25.f});
        (*menu).addChild(genBtn);

        auto folderSpr = ButtonSprite::create("Song Folder", 105, 32, 0.5f, true, "bigFont.fnt", "GJ_button_03.png");
        (*folderSpr).setScale(0.7f);
        auto folderBtn = CCMenuItemSpriteExtra::create(folderSpr, this, menu_selector(BPMGuidePopup::onOpenFolder));
        (*folderBtn).setPosition({m_size.width / 2.0f, 25.f});
        (*menu).addChild(folderBtn);

        auto clearSpr = ButtonSprite::create("Clear", 95, 32, 0.5f, true, "bigFont.fnt", "GJ_button_06.png");
        (*clearSpr).setScale(0.7f);
        auto clearBtn = CCMenuItemSpriteExtra::create(clearSpr, this, menu_selector(BPMGuidePopup::onClear));
        (*clearBtn).setPosition({m_size.width / 2.0f + 100.f, 25.f});
        (*menu).addChild(clearBtn);

        return true;
    }

    static float getMultiplier(const std::string& str) {
        std::string s = str;
        s.erase(std::remove_if(s.begin(), s.end(), ::isspace), s.end());
        if (s.empty()) return 1.0f;

        size_t slash = s.find('/');
        if (slash != std::string::npos) {
            std::string numStr = s.substr(0, slash);
            std::string denStr = s.substr(slash + 1);
            float num = utils::numFromString<float>(numStr).unwrapOr(1.0f);
            float den = utils::numFromString<float>(denStr).unwrapOr(4.0f);
            if (den != 0.0f) {
                return 4.0f * (num / den);
            }
        } else {
            return utils::numFromString<float>(s).unwrapOr(1.0f);
        }
        return 1.0f;
    }

    void updateGridButtonStates(const std::string& activeName) {
        for (auto& item : m_gridBtns) {
            bool selected = (item.valName == activeName);
            if (item.greenSpr && item.blueSpr) {
                (*item.greenSpr).setVisible(!selected);
                (*item.blueSpr).setVisible(selected);
            }
        }
    }

    void updateSpeedButtonStates(int activeIdx) {
        for (size_t i = 0; i < m_speedBtns.size(); ++i) {
            bool selected = (static_cast<int>(i) == activeIdx);
            if (m_speedBtns[i].greenSpr && m_speedBtns[i].blueSpr) {
                (*m_speedBtns[i].greenSpr).setVisible(!selected);
                (*m_speedBtns[i].blueSpr).setVisible(selected);
            }
        }
    }

    void onSpeedSelect(CCObject* sender) {
        int idx = (*sender).getTag();
        g_selectedSpeedIndex = idx;
        updateSpeedButtonStates(idx);
    }

    void saveValues() {
        if (m_bpmInput) {
            std::string str = (*m_bpmInput).getString();
            std::replace(str.begin(), str.end(), ',', '.');
            g_bpm = utils::numFromString<float>(str).unwrapOr(120.0f);
        }
        if (m_offsetInput) {
            std::string str = (*m_offsetInput).getString();
            std::replace(str.begin(), str.end(), ',', '.');
            g_offset = utils::numFromString<float>(str).unwrapOr(0.0f);
        }
        if (m_customGridInput) {
            std::string str = (*m_customGridInput).getString();
            if (!str.empty()) {
                m_gridStep = str;
            }
        }
        g_gridStepStr = m_gridStep;
    }

    void onClose(CCObject* sender) {
        saveValues();
        Popup::onClose(sender);
    }

    void onTap(CCObject*) {
        auto now = std::chrono::steady_clock::now();
        double cur = std::chrono::duration<double>(now.time_since_epoch()).count();
        
        if (!m_taps.empty() && (cur - m_taps.back() > 1.5)) {
            m_taps.clear();
        }
        m_taps.push_back(cur);

        if (m_taps.size() >= 5) {
            std::vector<double> diffs;
            for (size_t i = 1; i < m_taps.size(); ++i) {
                double dt = m_taps[i] - m_taps[i - 1];
                if (dt > 0.1 && dt < 3.0) diffs.push_back(dt);
            }

            if (diffs.size() >= 4) {
                double avg = 0.0;
                for (double d : diffs) avg += d;
                avg /= diffs.size();

                float calcBpm = 60.0f / static_cast<float>(avg);
                while (calcBpm < 60.0f && calcBpm > 0.0f) calcBpm *= 2.0f;
                while (calcBpm > 200.0f) calcBpm /= 2.0f;

                if (m_bpmInput) (*m_bpmInput).setString(fmt::format("{:.1f}", calcBpm));
            }
        }
    }

    void onAutoBPM(CCObject*) {
        auto editorPtr = LevelEditorLayer::get();
        if (!editorPtr || !(*editorPtr).m_level) return;
        auto& editor = *editorPtr;
        auto level = editor.m_level;

        float detectedBpm = 128.0f;
        float detectedOffset = 0.0f;

        if (level->m_songID <= 0) {
            int track = level->m_audioTrack;
            switch (track) {
                case 1: detectedBpm = 144.0f; break;
                case 2: detectedBpm = 130.0f; break;
                case 3: detectedBpm = 125.0f; break;
                case 4: detectedBpm = 115.0f; break;
                case 5: detectedBpm = 125.0f; break;
                case 6: detectedBpm = 115.0f; break;
                case 7: detectedBpm = 132.0f; break;
                case 8: detectedBpm = 130.0f; break;
                case 9: detectedBpm = 130.0f; break;
                case 10: detectedBpm = 142.0f; break;
                case 11: detectedBpm = 138.0f; break;
                case 12: detectedBpm = 142.0f; break;
                case 13: detectedBpm = 145.0f; break;
                case 14: detectedBpm = 150.0f; break;
                case 15: detectedBpm = 150.0f; break;
                case 16: detectedBpm = 145.0f; break;
                case 17: detectedBpm = 140.0f; break;
                case 18: detectedBpm = 142.0f; break;
                case 19: detectedBpm = 145.0f; break;
                case 20: detectedBpm = 150.0f; break;
                case 21: detectedBpm = 140.0f; break;
                case 22: detectedBpm = 135.0f; break;
                default: detectedBpm = 130.0f; break;
            }
        } else {
            auto fmod = FMODAudioEngine::get();
            if (fmod && fmod->m_system) {
                int songID = level->m_songID;
                namespace fs = std::filesystem;
                auto savePath = dirs::getSaveDir();
                fs::path targetPath;
                bool found = false;

                if (fs::exists(savePath)) {
                    for (const auto& entry : fs::directory_iterator(savePath)) {
                        if (entry.is_regular_file() && entry.path().stem().string() == std::to_string(songID)) {
                            targetPath = entry.path();
                            found = true;
                            break;
                        }
                    }
                }

                if (found && fs::exists(targetPath)) {
                    FMOD::Sound* sound = nullptr;
                    std::string pathStr = targetPath.string();
                    FMOD_RESULT res = fmod->m_system->createSound(pathStr.c_str(), FMOD_CREATESAMPLE, 0, &sound);
                    if (res == FMOD_OK && sound) {
                        float sampleRate = 44100.0f;
                        sound->getDefaults(&sampleRate, nullptr);
                        FMOD_SOUND_FORMAT format;
                        sound->getFormat(nullptr, &format, nullptr, nullptr);

                        unsigned int lengthBytes = 0;
                        sound->getLength(&lengthBytes, FMOD_TIMEUNIT_PCMBYTES);

                        void* ptr1 = nullptr;
                        void* ptr2 = nullptr;
                        unsigned int len1 = 0;
                        unsigned int len2 = 0;

                        if (sound->lock(0, lengthBytes, &ptr1, &ptr2, &len1, &len2) == FMOD_OK && ptr1) {
                            std::vector<float> energies;
                            const int windowSize = 2048;
                            
                            if (format == FMOD_SOUND_FORMAT_PCM16) {
                                int16_t* samples = static_cast<int16_t*>(ptr1);
                                unsigned int numSamples = len1 / sizeof(int16_t);
                                float sum = 0.0f;
                                unsigned int count = 0;
                                for (unsigned int i = 0; i < numSamples; ++i) {
                                    float val = static_cast<float>(samples[i]) / 32768.0f;
                                    sum += val * val;
                                    count++;
                                    if (count >= windowSize) {
                                        energies.push_back(std::sqrt(sum / windowSize));
                                        sum = 0.0f;
                                        count = 0;
                                    }
                                }
                            } else if (format == FMOD_SOUND_FORMAT_PCMFLOAT) {
                                float* samples = static_cast<float*>(ptr1);
                                unsigned int numSamples = len1 / sizeof(float);
                                float sum = 0.0f;
                                unsigned int count = 0;
                                for (unsigned int i = 0; i < numSamples; ++i) {
                                    float val = samples[i];
                                    sum += val * val;
                                    count++;
                                    if (count >= windowSize) {
                                        energies.push_back(std::sqrt(sum / windowSize));
                                        sum = 0.0f;
                                        count = 0;
                                    }
                                }
                            }

                            sound->unlock(ptr1, ptr2, len1, len2);

                            if (energies.size() > 50) {
                                std::vector<float> movingAvg(energies.size(), 0.0f);
                                int avgWindow = 43;
                                float currentSum = 0.0f;
                                
                                for (size_t i = 0; i < energies.size(); ++i) {
                                    currentSum += energies[i];
                                    if (i >= (size_t)avgWindow) {
                                        currentSum -= energies[i - avgWindow];
                                        movingAvg[i] = currentSum / avgWindow;
                                    } else {
                                        movingAvg[i] = currentSum / (i + 1);
                                    }
                                }

                                std::vector<float> peakTimes;
                                float timeStep = static_cast<float>(windowSize) / sampleRate;

                                for (size_t i = 1; i < energies.size() - 1; ++i) {
                                    if (energies[i] > movingAvg[i] * 1.35f && energies[i] >= energies[i - 1] && energies[i] >= energies[i + 1]) {
                                        if (peakTimes.empty() || (i * timeStep - peakTimes.back() >= 0.25f)) {
                                            peakTimes.push_back(i * timeStep);
                                        }
                                    }
                                }

                                if (peakTimes.size() >= 10) {
                                    std::map<int, int> intervalBins;
                                    for (size_t i = 0; i < peakTimes.size(); ++i) {
                                        for (size_t j = i + 1; j < peakTimes.size(); ++j) {
                                            float dt = peakTimes[j] - peakTimes[i];
                                            if (dt >= 0.3f && dt <= 1.0f) {
                                                int bin = static_cast<int>(std::round(dt * 10.0f));
                                                intervalBins[bin]++;
                                            }
                                        }
                                    }

                                    int bestBin = 0;
                                    int maxCount = 0;
                                    for (auto const& [bin, count] : intervalBins) {
                                        if (count > maxCount) {
                                            maxCount = count;
                                            bestBin = bin;
                                        }
                                    }

                                    if (bestBin > 0) {
                                        float bestInterval = static_cast<float>(bestBin) / 10.0f;
                                        float calcBpm = 60.0f / bestInterval;
                                        while (calcBpm < 90.0f) calcBpm *= 2.0f;
                                        while (calcBpm > 180.0f) calcBpm *= 0.5f;
                                        detectedBpm = std::round(calcBpm * 10.0f) / 10.0f;

                                        if (!peakTimes.empty()) {
                                            float period = 60.0f / detectedBpm;
                                            float firstPeak = peakTimes[0];
                                            float offsetVal = std::fmod(firstPeak, period);
                                            detectedOffset = std::round(offsetVal * 1000.0f);
                                            if (detectedOffset < 0.0f) detectedOffset += (period * 1000.0f);
                                        }
                                    }
                                }
                            }
                        }
                        sound->release();
                    }
                }
            }
        }

        if (m_bpmInput) {
            (*m_bpmInput).setString(fmt::format("{:.1f}", detectedBpm));
        }
        if (m_offsetInput) {
            (*m_offsetInput).setString(fmt::format("{:.0f}", detectedOffset));
        }

        auto notif = Notification::create(fmt::format("Auto BPM: {:.1f}", detectedBpm), NotificationIcon::Success, 2.0f);
        (*notif).show();
    }

    void onOpenFolder(CCObject*) {
        auto editorPtr = LevelEditorLayer::get();
        if (!editorPtr || !(*editorPtr).m_level) return;
        auto& editor = *editorPtr;
        
        int songID = (*editor.m_level).m_songID;
        if (songID <= 0) {
            return;
        }

        namespace fs = std::filesystem;
        auto savePath = dirs::getSaveDir();
        fs::path targetPath;
        bool found = false;

        if (fs::exists(savePath)) {
            for (const auto& entry : fs::directory_iterator(savePath)) {
                if (entry.is_regular_file() && entry.path().stem().string() == std::to_string(songID)) {
                    targetPath = entry.path();
                    found = true;
                    break;
                }
            }
        }

        if (found && fs::exists(targetPath)) {
#ifdef GEODE_IS_WINDOWS
            std::wstring wpath = targetPath.wstring();
            std::wstring params = L"/select,\"" + wpath + L"\"";
            ShellExecuteW(NULL, L"open", L"explorer.exe", params.c_str(), NULL, SW_SHOW);
#else
            file::openFolder(targetPath.parent_path());
#endif
        } else {
            file::openFolder(savePath);
        }
    }

    void onGridSelect(CCObject* sender) {
        int idx = (*sender).getTag();
        if (idx >= 0 && idx < m_gridBtns.size()) {
            m_gridStep = m_gridBtns[idx].valName;
            g_gridStepStr = m_gridStep;
            
            float posX = m_customGridInput ? m_customGridInput->getPositionX() : 0.0f;
            float posY = m_customGridInput ? m_customGridInput->getPositionY() : 0.0f;
            
            if (m_customGridInput) {
                m_customGridInput->removeFromParent();
            }
            
            m_customGridInput = TextInput::create(m_exactBtnWidth, m_gridStep, "bigFont.fnt");
            (*m_customGridInput).setFilter("0123456789./");
            (*m_customGridInput).setMaxCharCount(6);
            (*m_customGridInput).setScale(0.6f);
            (*m_customGridInput).setPosition({posX, posY});
            
            (*m_customGridInput).setCallback([this](std::string const& text) {
                if (!text.empty()) {
                    m_gridStep = text;
                    updateGridButtonStates(""); 
                }
            });
            m_mainLayer->addChild(m_customGridInput);
            
            updateGridButtonStates(m_gridStep);
        }
    }

    void onGenerate(CCObject*) {
        saveValues();
        auto editorPtr = LevelEditorLayer::get();
        if (!editorPtr || !(*editorPtr).m_level) return;
        auto& editor = *editorPtr;

        // Точные физические коэффициенты порталов скорости игры
        float speedMultipliers[] = { 0.7f, 1.0f, 1.3f, 1.6f, 1.9f };
        float speedFactor = speedMultipliers[std::clamp(g_selectedSpeedIndex, 0, 4)];

        float offsetSec = g_offset / 1000.0f;
        float mult = getMultiplier(g_gridStepStr);
        
        // Чистый и точный интервал времени между ударами
        float baseInterval = ((60.0f / g_bpm) * mult) / speedFactor;

        // Физическая скорость кубика в пикселях/единицах редактора за секунду
        float pointsPerSecond = 311.04f * speedFactor;

        std::string guidelinesData = "";
        auto drawNode = CCDrawNode::create();
        (*drawNode).setID("bpm-guidelines-drawer");

        for (int i = 0; i < 5000; ++i) {
            float t = offsetSec + (i * baseInterval);
            if (t > 180.0f) break;
            
            if (t >= 0.0f) {
                guidelinesData += fmt::format("{:.3f},1;", t);
                
                // Идеальная математическая привязка к сетке редактора
                float posX = t * pointsPerSecond;
                (*drawNode).drawSegment(ccp(posX, -2000.0f), ccp(posX, 5000.0f), 1.5f, ccc4f(0.0f, 1.0f, 1.0f, 0.6f));
            }
        }

        if (editor.m_levelSettings) {
            (*editor.m_levelSettings).m_guidelineString = guidelinesData;
        }

        if (auto oldNode = editor.getChildByID("bpm-guidelines-drawer")) {
            oldNode->removeFromParent();
        }
        if (editor.m_objectLayer) {
            if (auto oldNode = editor.m_objectLayer->getChildByID("bpm-guidelines-drawer")) {
                oldNode->removeFromParent();
            }
            (*editor.m_objectLayer).addChild(drawNode, 10000);
        } else {
            editor.addChild(drawNode, 10000);
        }

        auto notif = Notification::create("Guidelines generated!", NotificationIcon::Success, 2.0f);
        (*notif).show();
        this->onClose(nullptr);
    }

    void onClear(CCObject*) {
        auto editorPtr = LevelEditorLayer::get();
        if (!editorPtr || !(*editorPtr).m_levelSettings) return;
        auto& editor = *editorPtr;
        
        (*editor.m_levelSettings).m_guidelineString = "";

        if (auto oldNode = editor.getChildByID("bpm-guidelines-drawer")) {
            oldNode->removeFromParent();
        }
        if (editor.m_objectLayer) {
            if (auto oldNode = editor.m_objectLayer->getChildByID("bpm-guidelines-drawer")) {
                oldNode->removeFromParent();
            }
        }

        auto notif = Notification::create("Guidelines cleared!", NotificationIcon::Error, 2.0f);
        (*notif).show();
        this->onClose(nullptr);
    }
};

class $modify(MetronomeLevelLayer, LevelEditorLayer) {
    struct Fields {
        float playTestTime = 0.0f;
        float nextTickTime = 0.0f;
        bool wasPlaying = false;
    };

    void update(float dt) {
        LevelEditorLayer::update(dt);

        bool isPlaying = (this->m_playbackMode == PlaybackMode::Playing);

        if (isPlaying && g_bpm > 0.0f) {
            float offsetSec = g_offset / 1000.0f;
            float mult = BPMGuidePopup::getMultiplier(g_gridStepStr);
            float interval = (60.0f / g_bpm) * mult;

            auto& pt = m_fields->nextTickTime;
            auto& nt = m_fields->playTestTime;
            auto& wp = m_fields->wasPlaying;
            if (!wp) {
                pt = 0.0f;
                nt = offsetSec;
                wp = true;
            }
            pt += dt;

            if (pt >= nt) {
                auto fmod = FMODAudioEngine::get();
                if (fmod) {
                    if ((*fmod).m_globalChannel) {
                        auto& channel = *(*fmod).m_globalChannel;
                        channel.setPaused(false);
                    }
                    (*fmod).playEffectAsync("chestClick.ogg");
                }
                
                nt += interval;
                if (nt <= pt) {
                    nt = pt + interval;
                }
            }
        } else {
            m_fields->wasPlaying = false;
        }
    }
};

class $modify(BPMEditorUI, EditorUI) {
    bool init(LevelEditorLayer* editorLayer) {
        if (!EditorUI::init(editorLayer)) return false;
        
        if (auto togglesMenu = getChildByID("toolbar-toggles-menu")) {
            auto spr = ButtonSprite::create("BPM", 36, 36, 0.45f, true, "bigFont.fnt", "GJ_button_01.png");
            (*spr).setScale(0.85f);

            auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(BPMEditorUI::onOpenBPMModal));
            (*btn).setID("bpm-generator-button"_spr);
            
            (*togglesMenu).addChild(btn);
            (*togglesMenu).updateLayout();
        }
        return true;
    }

    void onOpenBPMModal(CCObject*) {
        auto popup = BPMGuidePopup::create();
        (*popup).show();
    }
};