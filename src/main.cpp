#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/modify/LevelSettingsObject.hpp>
#include <Geode/ui/TextInput.hpp>
#include <chrono>
#include <filesystem>

#ifdef GEODE_IS_WINDOWS
#include <windows.h>
#include <shellapi.h>
#endif

using namespace geode::prelude;
static float g_savedBPM = 120.0f;
static float g_savedOffset = 0.0f;
static int g_savedSubdivision = 4;

class BPMGuidePopup : public Popup {
protected:
    TextInput* m_bpmInput = nullptr;
    TextInput* m_offsetInput = nullptr;
    int m_selectedSubdivision = 4;
    
    struct SubButtonData {
        CCMenuItemSpriteExtra* btn;
        int value;
    };
    std::vector<SubButtonData> m_subButtonsData;
    std::vector<double> m_tapTimes;

public:
    static BPMGuidePopup* create() {
        auto ret = new BPMGuidePopup();
        if (ret && ret->init(380.f, 290.f)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    bool init(float width, float height) {
        if (!Popup::init(width, height)) return false;

        auto title = CCLabelBMFont::create("BPM Guide Generator", "bigFont.fnt");
        title->setScale(0.5f);
        title->setPosition({m_size.width / 2.0f, m_size.height - 25.0f});
        m_mainLayer->addChild(title);

        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        m_mainLayer->addChild(menu);

        auto bpmLabel = CCLabelBMFont::create("BPM:", "goldFont.fnt");
        bpmLabel->setScale(0.5f);
        bpmLabel->setPosition({m_size.width / 2.0f - 110.f, m_size.height - 65.f});
        m_mainLayer->addChild(bpmLabel);

        m_bpmInput = TextInput::create(80.f, fmt::format("{:.1f}", g_savedBPM), "bigFont.fnt");
        m_bpmInput->setFilter("0123456789.");
        m_bpmInput->setMaxCharCount(6);
        m_bpmInput->setPosition({m_size.width / 2.0f - 15.f, m_size.height - 65.f});
        m_mainLayer->addChild(m_bpmInput);

        auto tapSprite = ButtonSprite::create("Tap", 50, 0, 0.5f, true, "bigFont.fnt", "GJ_button_02.png");
        if (tapSprite) {
            tapSprite->setScale(0.65f);
            auto tapBtn = CCMenuItemSpriteExtra::create(
                tapSprite,
                this,
                menu_selector(BPMGuidePopup::onTapTempoClicked)
            );
            tapBtn->setPosition({m_size.width / 2.0f + 65.f, m_size.height - 65.f});
            menu->addChild(tapBtn);
        }

        auto offsetLabel = CCLabelBMFont::create("Offset (ms):", "goldFont.fnt");
        offsetLabel->setScale(0.5f);
        offsetLabel->setPosition({m_size.width / 2.0f - 110.f, m_size.height - 110.f});
        m_mainLayer->addChild(offsetLabel);

        m_offsetInput = TextInput::create(100.f, fmt::format("{:.0f}", g_savedOffset), "bigFont.fnt");
        m_offsetInput->setFilter("0123456789.-");
        m_offsetInput->setMaxCharCount(6);
        m_offsetInput->setPosition({m_size.width / 2.0f + 25.f, m_size.height - 110.f});
        m_mainLayer->addChild(m_offsetInput);

        auto subLabel = CCLabelBMFont::create("Grid Step:", "goldFont.fnt");
        subLabel->setScale(0.5f);
        subLabel->setPosition({m_size.width / 2.0f - 110.f, m_size.height - 155.f});
        m_mainLayer->addChild(subLabel);

        m_selectedSubdivision = g_savedSubdivision;
        const char* subNames[] = { "1/2", "1/4", "1/8" };
        int subValues[] = { 2, 4, 8 };

        for (int i = 0; i < 3; ++i) {
            bool selected = (subValues[i] == m_selectedSubdivision);
            auto btnSprite = ButtonSprite::create(
                subNames[i], 55, 30, 0.6f, true, 
                "bigFont.fnt", 
                "GJ_button_01.png"
            );
            if (btnSprite) {
                btnSprite->setScale(selected ? 0.85f : 0.8f);
                if (selected) {
                    btnSprite->setColor({0, 255, 0});
                }
                auto btn = CCMenuItemSpriteExtra::create(
                    btnSprite,
                    this,
                    menu_selector(BPMGuidePopup::onSubdivisionSelected)
                );
                btn->setTag(subValues[i]);
                btn->setPosition({m_size.width / 2.0f - 35.f + (i * 65.0f), m_size.height - 155.f});
                menu->addChild(btn);
                m_subButtonsData.push_back({btn, subValues[i]});
            }
        }

        auto folderSprite = ButtonSprite::create("Song Folder", 180, 0, 0.5f, true, "bigFont.fnt", "GJ_button_03.png");
        if (folderSprite) {
            folderSprite->setScale(0.65f);
            auto folderBtn = CCMenuItemSpriteExtra::create(
                folderSprite,
                this,
                menu_selector(BPMGuidePopup::onOpenSongFolderClicked)
            );
            folderBtn->setPosition({m_size.width / 2.0f, m_size.height - 200.f});
            menu->addChild(folderBtn);
        }

        auto genSprite = ButtonSprite::create("Generate", 120, 0, 0.5f, true, "bigFont.fnt", "GJ_button_01.png");
        if (genSprite) {
            genSprite->setScale(0.75f);
            auto genBtn = CCMenuItemSpriteExtra::create(
                genSprite,
                this,
                menu_selector(BPMGuidePopup::onGenerateClicked)
            );
            genBtn->setPosition({m_size.width / 2.0f - 70.f, 30.f});
            menu->addChild(genBtn);
        }

        auto clearSprite = ButtonSprite::create("Clear", 120, 0, 0.5f, true, "bigFont.fnt", "GJ_button_06.png");
        if (clearSprite) {
            clearSprite->setScale(0.75f);
            auto clearBtn = CCMenuItemSpriteExtra::create(
                clearSprite,
                this,
                menu_selector(BPMGuidePopup::onClearClicked)
            );
            clearBtn->setPosition({m_size.width / 2.0f + 70.f, 30.f});
            menu->addChild(clearBtn);
        }

        return true;
    }

    void saveValuesToGlobals() {
        if (m_bpmInput) {
            std::string s = m_bpmInput->getString();
            std::replace(s.begin(), s.end(), ',', '.');
            g_savedBPM = utils::numFromString<float>(s).unwrapOr(120.0f);
        }
        if (m_offsetInput) {
            std::string s = m_offsetInput->getString();
            std::replace(s.begin(), s.end(), ',', '.');
            g_savedOffset = utils::numFromString<float>(s).unwrapOr(0.0f);
        }
        g_savedSubdivision = m_selectedSubdivision;
    }

    void onClose(CCObject* sender) {
        saveValuesToGlobals();
        Popup::onClose(sender);
    }

    void onTapTempoClicked(CCObject*) {
        auto now = std::chrono::steady_clock::now();
        double currentTime = std::chrono::duration<double>(now.time_since_epoch()).count();
        if (!m_tapTimes.empty() && (currentTime - m_tapTimes.back() > 1.5)) m_tapTimes.clear();
        m_tapTimes.push_back(currentTime);
        if (m_tapTimes.size() >= 5) {
            std::vector<double> intervals;
            for (size_t i = 1; i < m_tapTimes.size(); ++i) {
                double diff = m_tapTimes[i] - m_tapTimes[i - 1];
                if (diff > 0.1 && diff < 3.0) intervals.push_back(diff);
            }
            if (intervals.size() >= 4) {
                double sum = 0.0;
                for (double iv : intervals) sum += iv;
                float calculatedBpm = 60.0f / static_cast<float>(sum / intervals.size());
                while (calculatedBpm < 60.0f && calculatedBpm > 0.0f) calculatedBpm *= 2.0f;
                while (calculatedBpm > 200.0f) calculatedBpm /= 2.0f;
                if (m_bpmInput) m_bpmInput->setString(fmt::format("{:.1f}", calculatedBpm));
            }
        }
    }

    void onOpenSongFolderClicked(CCObject*) {
        auto editor = LevelEditorLayer::get();
        if (!editor || !editor->m_level) return;
        
        int songID = editor->m_level->m_songID;
        if (songID <= 0) {
            Notification::create("Song ID not selected!", NotificationIcon::Error, 2.0f)->show();
            return;
        }

        namespace fs = std::filesystem;
        auto savePath = dirs::getSaveDir();
        
        fs::path targetSongPath;
        bool found = false;

        if (fs::exists(savePath)) {
            for (const auto& entry : fs::directory_iterator(savePath)) {
                if (entry.is_regular_file()) {
                    auto path = entry.path();
                    if (path.stem().string() == std::to_string(songID)) {
                        targetSongPath = path;
                        found = true;
                        break;
                    }
                }
            }
        }

        if (found && fs::exists(targetSongPath)) {
#ifdef GEODE_IS_WINDOWS
            std::wstring wpath = targetSongPath.wstring();
            std::wstring params = L"/select,\"" + wpath + L"\"";
            ShellExecuteW(NULL, L"open", L"explorer.exe", params.c_str(), NULL, SW_SHOW);
#else
            file::openFolder(targetSongPath.parent_path());
#endif
            Notification::create("Song found and highlighted!", NotificationIcon::Success, 2.0f)->show();
        } else {
            file::openFolder(savePath);
            Notification::create("Song not downloaded, opened saves folder", NotificationIcon::Warning, 2.5f)->show();
        }
    }

    void onSubdivisionSelected(CCObject* sender) {
        m_selectedSubdivision = sender->getTag();
        g_savedSubdivision = m_selectedSubdivision;
        updateSubdivisionButtons();
    }

    void updateSubdivisionButtons() {
        for (auto& info : m_subButtonsData) {
            bool selected = (info.value == m_selectedSubdivision);
            if (info.btn && info.btn->getNormalImage()) {
                if (auto sprite = static_cast<ButtonSprite*>(info.btn->getNormalImage())) {
                    if (selected) {
                        sprite->setColor({0, 255, 0});
                        info.btn->setScale(0.85f);
                    } else {
                        sprite->setColor({255, 255, 255});
                        info.btn->setScale(0.8f);
                    }
                }
            }
        }
    }

    void onGenerateClicked(CCObject*) {
        saveValuesToGlobals();
        auto editor = LevelEditorLayer::get();
        if (!editor || !editor->m_levelSettings) return;

        float offsetSec = g_savedOffset / 1000.0f;
        float beatInterval = (60.0f / g_savedBPM) * (4.0f / static_cast<float>(g_savedSubdivision));
        std::string guidelinesData = "";

        for (float time = offsetSec; time <= 180.0f; time += beatInterval) {
            if (time >= 0.0f) guidelinesData += fmt::format("{:.3f},1;", time);
        }
        editor->m_levelSettings->m_guidelineString = guidelinesData;

        auto existingNode = editor->m_objectLayer ? editor->m_objectLayer->getChildByID("bpm-guidelines-drawer") : editor->getChildByID("bpm-guidelines-drawer");
        if (existingNode) existingNode->removeFromParent();

        auto drawNode = CCDrawNode::create();
        drawNode->setID("bpm-guidelines-drawer");
        if (editor->m_objectLayer) {
            editor->m_objectLayer->addChild(drawNode, 10000);
        } else {
            editor->addChild(drawNode, 10000);
        }

        for (float time = offsetSec; time <= 180.0f; time += beatInterval) {
            if (time < 0.0f) continue;
            float posX = time * 300.0f;
            drawNode->drawSegment(
                ccp(posX, -2000.0f), 
                ccp(posX, 5000.0f), 
                1.5f, 
                ccc4f(0.0f, 1.0f, 1.0f, 0.6f)
            );
        }

        Notification::create("Guidelines and metronome configured!", NotificationIcon::Success, 2.0f)->show();
        this->onClose(nullptr);
    }

    void onClearClicked(CCObject*) {
        auto editor = LevelEditorLayer::get();
        if (!editor || !editor->m_levelSettings) return;
        editor->m_levelSettings->m_guidelineString = "";

        auto existingNode = editor->m_objectLayer ? editor->m_objectLayer->getChildByID("bpm-guidelines-drawer") : editor->getChildByID("bpm-guidelines-drawer");
        if (existingNode) existingNode->removeFromParent();

        Notification::create("Guidelines removed!", NotificationIcon::Error, 2.0f)->show();
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

        if (isPlaying && g_savedBPM > 0.0f) {
            float offsetSec = g_savedOffset / 1000.0f;
            float interval = (60.0f / g_savedBPM) * (4.0f / static_cast<float>(g_savedSubdivision));

            if (!m_fields->wasPlaying) {
                m_fields->playTestTime = 0.0f;
                m_fields->nextTickTime = offsetSec;
                m_fields->wasPlaying = true;
            }

            m_fields->playTestTime += dt;

            if (m_fields->playTestTime >= m_fields->nextTickTime) {
                auto fmod = FMODAudioEngine::get();
                if (fmod) {
                    if (fmod->m_globalChannel) {
                        fmod->m_globalChannel->setPaused(false);
                    }
                    fmod->playEffectAsync("buttonSound.ogg");
                }
                
                m_fields->nextTickTime += interval;

                if (m_fields->nextTickTime <= m_fields->playTestTime) {
                    m_fields->nextTickTime = m_fields->playTestTime + interval;
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
            auto btnSprite = ButtonSprite::create("BPM", 36, 36, 0.45f, true, "bigFont.fnt", "GJ_button_01.png");
            if (btnSprite) {
                btnSprite->setScale(0.85f);
                auto btn = CCMenuItemSpriteExtra::create(
                    btnSprite,
                    this,
                    menu_selector(BPMEditorUI::onOpenBPMModal)
                );
                btn->setID("bpm-generator-button"_spr);
                togglesMenu->addChild(btn);
                togglesMenu->updateLayout();
            }
        }
        return true;
    }

    void onOpenBPMModal(CCObject*) {
        BPMGuidePopup::create()->show();
    }
};