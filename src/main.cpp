#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/modify/LevelSettingsObject.hpp>
#include <Geode/ui/TextInput.hpp>
#include <chrono>
#include <filesystem>
#include <algorithm>
#include <cmath>

#ifdef GEODE_IS_WINDOWS
#include <windows.h>
#include <shellapi.h>
#endif

using namespace geode::prelude;

static float g_bpm = 120.0f;
static float g_offset = 0.0f;
static std::string g_gridStepStr = "1/4";

class BPMGuidePopup : public Popup {
protected:
    TextInput* m_bpmInput = nullptr;
    TextInput* m_offsetInput = nullptr;
    TextInput* m_customGridInput = nullptr;
    std::string m_gridStep = "1/4";
    
    struct SubBtn {
        CCMenuItemSpriteExtra* btn;
        CCNode* greenSpr;
        CCNode* blueSpr;
        std::string valName;
    };
    std::vector<SubBtn> m_btns;
    std::vector<double> m_taps;

public:
    static BPMGuidePopup* create() {
        auto ret = new BPMGuidePopup();
        if (ret && (*ret).init(380.f, 220.f)) {
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

        float fieldX = 155.f;

        auto bpmLabel = CCLabelBMFont::create("BPM:", "goldFont.fnt");
        (*bpmLabel).setScale(0.45f);
        (*bpmLabel).setAnchorPoint({0.0f, 0.5f});
        (*bpmLabel).setPosition({25.f, m_size.height - 55.f});
        ml.addChild(bpmLabel);

        m_bpmInput = TextInput::create(75.f, fmt::format("{:.1f}", g_bpm), "bigFont.fnt");
        (*m_bpmInput).setFilter("0123456789.");
        (*m_bpmInput).setMaxCharCount(6);
        (*m_bpmInput).setScale(0.6f);
        (*m_bpmInput).setPosition({fieldX, m_size.height - 55.f});
        ml.addChild(m_bpmInput);

        auto tapSpr = ButtonSprite::create("Tap", 50, 0, 0.45f, true, "bigFont.fnt", "GJ_button_02.png");
        (*tapSpr).setScale(0.65f);
        auto tapBtn = CCMenuItemSpriteExtra::create(
            tapSpr, this, menu_selector(BPMGuidePopup::onTap)
        );
        (*tapBtn).setPosition({fieldX + 58.f, m_size.height - 55.f});
        (*menu).addChild(tapBtn);

        auto offsetLabel = CCLabelBMFont::create("Offset (ms):", "goldFont.fnt");
        (*offsetLabel).setScale(0.45f);
        (*offsetLabel).setAnchorPoint({0.0f, 0.5f});
        (*offsetLabel).setPosition({25.f, m_size.height - 95.f});
        ml.addChild(offsetLabel);

        m_offsetInput = TextInput::create(75.f, fmt::format("{:.0f}", g_offset), "bigFont.fnt");
        (*m_offsetInput).setFilter("0123456789.-");
        (*m_offsetInput).setMaxCharCount(6);
        (*m_offsetInput).setScale(0.6f);
        (*m_offsetInput).setPosition({fieldX, m_size.height - 95.f});
        ml.addChild(m_offsetInput);

        auto subLabel = CCLabelBMFont::create("Grid Step:", "goldFont.fnt");
        (*subLabel).setScale(0.45f);
        (*subLabel).setAnchorPoint({0.0f, 0.5f});
        (*subLabel).setPosition({25.f, m_size.height - 135.f});
        ml.addChild(subLabel);

        m_gridStep = g_gridStepStr;
        const char* names[] = { "1/2", "1/4", "1/8" };

        float rowStartX = 122.f;
        float spacing = 43.0f; 

        auto sampleSpr = ButtonSprite::create("1/4", 38, 24, 0.5f, true, "bigFont.fnt", "GJ_button_01.png");
        float exactBtnWidth = sampleSpr->getContentSize().width;

        for (int i = 0; i < 3; ++i) {
            bool selected = (names[i] == m_gridStep);

            auto greenBtnSpr = ButtonSprite::create(names[i], 38, 24, 0.5f, true, "bigFont.fnt", "GJ_button_01.png");
            (*greenBtnSpr).setScale(0.6f);

            auto blueBtnSpr = ButtonSprite::create(names[i], 38, 24, 0.5f, true, "bigFont.fnt", "GJ_button_02.png");
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

            auto btn = CCMenuItemSpriteExtra::create(container, this, menu_selector(BPMGuidePopup::onSubSelect));
            (*btn).setTag(i);
            (*btn).setPosition({rowStartX + (i * spacing), m_size.height - 135.f});
            (*menu).addChild(btn);
            
            m_btns.push_back({btn, greenBtnSpr, blueBtnSpr, names[i]});
        }

        bool isPreset = false;
        for (auto& name : names) {
            if (m_gridStep == name) {
                isPreset = true;
                break;
            }
        }

        std::string initialCustom = isPreset ? "" : m_gridStep;

        m_customGridInput = TextInput::create(exactBtnWidth, initialCustom, "bigFont.fnt");
        (*m_customGridInput).setFilter("0123456789./");
        (*m_customGridInput).setMaxCharCount(6);
        (*m_customGridInput).setScale(0.6f);
        (*m_customGridInput).setPosition({rowStartX + (3 * spacing), m_size.height - 135.f});
        
        (*m_customGridInput).setCallback([this](std::string const& text) {
            if (!text.empty()) {
                m_gridStep = text;
                updateButtonStates(""); 
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

    void updateButtonStates(const std::string& activeName) {
        for (auto& item : m_btns) {
            bool selected = (item.valName == activeName);
            if (item.greenSpr && item.blueSpr) {
                (*item.greenSpr).setVisible(!selected);
                (*item.blueSpr).setVisible(selected);
            }
        }
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

    void onSubSelect(CCObject* sender) {
        int idx = (*sender).getTag();
        if (idx >= 0 && idx < m_btns.size()) {
            m_gridStep = m_btns[idx].valName;
            g_gridStepStr = m_gridStep;
            if (m_customGridInput) {
                (*m_customGridInput).setString("");
            }
            updateButtonStates(m_gridStep);
        }
    }

    void onGenerate(CCObject*) {
        saveValues();
        auto editorPtr = LevelEditorLayer::get();
        if (!editorPtr || !(*editorPtr).m_levelSettings) return;
        auto& editor = *editorPtr;

        float offsetSec = g_offset / 1000.0f;
        float mult = getMultiplier(g_gridStepStr);
        float interval = (60.0f / g_bpm) * mult;
        std::string guidelinesData = "";

        for (float t = offsetSec; t <= 180.0f; t += interval) {
            if (t >= 0.0f) guidelinesData += fmt::format("{:.3f},1;", t);
        }
        (*editor.m_levelSettings).m_guidelineString = guidelinesData;

        if (auto oldNode = editor.getChildByID("bpm-guidelines-drawer")) {
            oldNode->removeFromParent();
        }
        if (editor.m_objectLayer) {
            if (auto oldNode = editor.m_objectLayer->getChildByID("bpm-guidelines-drawer")) {
                oldNode->removeFromParent();
            }
        }

        auto drawNode = CCDrawNode::create();
        (*drawNode).setID("bpm-guidelines-drawer");
        
        if (editor.m_objectLayer) {
            (*editor.m_objectLayer).addChild(drawNode, 10000);
        } else {
            editor.addChild(drawNode, 10000);
        }

        for (float t = offsetSec; t <= 180.0f; t += interval) {
            if (t < 0.0f) continue;
            float posX = t * 300.0f;
            (*drawNode).drawSegment(ccp(posX, -2000.0f), ccp(posX, 5000.0f), 1.5f, ccc4f(0.0f, 1.0f, 1.0f, 0.6f));
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