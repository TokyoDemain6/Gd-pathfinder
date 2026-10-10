// GD Pathfinder Tools: in-game helpers for the gd-pathfinder project.
//
//   * Level page and "my level" page: a PF button that runs the pathfinder's
//     level check on the level and can export it as a .gmd file.
//   * Pause menu: an OBJ button that writes every loaded object's type and
//     hitbox to a CSV file, next to a .gmd of the same level.
//
// Everything is written to the "exports" folder inside this mod's save
// directory. The mod never changes a level and never plays one.
//
// NOTE: compiled by CI but not yet run in the game. See the pull request that
// added it for what has and has not been checked.

#include <Geode/Geode.hpp>
#include <Geode/modify/EditLevelLayer.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>

#include <filesystem>
#include <memory>
#include <string>

#include "format.hpp"
#include "gdpf/analyze.hpp"
#include "gdpf/level.hpp"

using namespace geode::prelude;

namespace {

std::filesystem::path exportDir() {
    return Mod::get()->getSaveDir() / "exports";
}

Result<std::filesystem::path> writeExport(std::string const& fileName, std::string const& data) {
    const auto dir = exportDir();
    GEODE_UNWRAP(file::createDirectoryAll(dir));
    const auto path = dir / fileName;
    GEODE_UNWRAP(file::writeString(path, data));
    return Ok(path);
}

std::string fileNameFor(GJGameLevel* level) {
    return pfmod::safe_file_name(std::string(level->m_levelName), level->m_levelID.value());
}

// Writes the level in the same format GDShare uses for .gmd files: the
// game's own serialisation of the level.
Result<std::filesystem::path> exportLevel(GJGameLevel* level) {
    auto dict = std::make_unique<DS_Dictionary>();
    level->encodeWithCoder(dict.get());
    const std::string data(dict->saveRootSubDictToString());
    if (data.empty()) return Err("the game returned no data for this level");
    return writeExport(fileNameFor(level) + ".gmd", data);
}

// Writes one CSV row per object in the loaded level.
Result<std::filesystem::path> dumpObjects(GJBaseGameLayer* layer) {
    std::string csv = pfmod::kObjectCsvHeader;
    csv += '\n';
    if (layer->m_objects) {
        for (auto obj : CCArrayExt<GameObject*>(layer->m_objects)) {
            if (!obj) continue;
            pfmod::ObjectRow row;
            row.id = obj->m_objectID;
            row.type = static_cast<int>(obj->m_objectType);
            row.x = obj->getPositionX();
            row.y = obj->getPositionY();
            row.start_x = obj->m_startPosition.x;
            row.start_y = obj->m_startPosition.y;
            row.rotation = obj->getRotation();
            row.scale_x = obj->getScaleX();
            row.scale_y = obj->getScaleY();
            row.flip_x = obj->m_isFlipX;
            row.flip_y = obj->m_isFlipY;
            const CCRect rect = obj->getObjectRect();
            row.rect_x = rect.origin.x;
            row.rect_y = rect.origin.y;
            row.rect_w = rect.size.width;
            row.rect_h = rect.size.height;
            row.radius = obj->m_objectRadius;
            row.no_touch = obj->m_isNoTouch;
            row.passable = obj->m_isPassable;
            row.trigger = obj->m_isTrigger;
            pfmod::append_object_row(csv, row);
        }
    }
    return writeExport(fileNameFor(layer->m_level) + ".objects.csv", csv);
}

void showError(std::string const& what) {
    FLAlertLayer::create("Pathfinder", "<cr>Could not export:</c> " + what, "OK")->show();
}

void showSaved(std::string const& what, std::filesystem::path const& path) {
    createQuickPopup(
        "Pathfinder",
        "Saved " + what + " as\n<cy>" + geode::utils::string::pathToString(path.filename()) + "</c>",
        "OK", "Open Folder",
        [dir = path.parent_path()](FLAlertLayer*, bool openFolder) {
            if (openFolder) file::openFolder(dir);
        }
    );
}

void exportAndReport(GJGameLevel* level) {
    auto result = exportLevel(level);
    if (result.isErr()) return showError(result.unwrapErr());
    showSaved("the level", result.unwrap());
}

// Runs the pathfinder's level check and offers to export the level.
void showLevelReport(GJGameLevel* level) {
    if (!level) return;
    const std::string levelString(level->m_levelString);
    if (levelString.empty()) {
        FLAlertLayer::create(
            "Pathfinder",
            "This level has no data yet. If it is still downloading, wait for it to finish and try again.",
            "OK"
        )->show();
        return;
    }

    const gdpf::ParseResult parsed = gdpf::parse_level(levelString);
    const std::string message = parsed.ok
        ? pfmod::report_message(parsed.level, gdpf::analyze(parsed.level))
        : "<cr>Could not read this level:</c> " + parsed.error;

    createQuickPopup(
        "Pathfinder", message, "Close", "Export", 380.f,
        [level = Ref<GJGameLevel>(level)](FLAlertLayer*, bool doExport) {
            if (doExport) exportAndReport(level);
        }
    );
}

CCMenuItemSpriteExtra* makeButton(char const* text, CircleBaseSize size, CCObject* target, SEL_MenuHandler callback) {
    auto label = CCLabelBMFont::create(text, "bigFont.fnt");
    auto sprite = CircleButtonSprite::create(label, CircleBaseColor::Cyan, size);
    return CCMenuItemSpriteExtra::create(sprite, target, callback);
}

}  // namespace

class $modify(PfLevelInfoLayer, LevelInfoLayer) {
    bool init(GJGameLevel* level, bool challenge) {
        if (!LevelInfoLayer::init(level, challenge)) return false;

        if (auto menu = this->getChildByID("left-side-menu")) {
            auto button = makeButton("PF", CircleBaseSize::Medium, this, menu_selector(PfLevelInfoLayer::onPathfinder));
            button->setID("pathfinder-button"_spr);
            menu->addChild(button);
            menu->updateLayout();
        }
        return true;
    }

    void onPathfinder(CCObject*) {
        showLevelReport(m_level);
    }
};

class $modify(PfEditLevelLayer, EditLevelLayer) {
    bool init(GJGameLevel* level) {
        if (!EditLevelLayer::init(level)) return false;

        if (auto menu = this->getChildByID("level-actions-menu")) {
            auto button = makeButton("PF", CircleBaseSize::MediumAlt, this, menu_selector(PfEditLevelLayer::onPathfinder));
            button->setID("pathfinder-button"_spr);
            menu->addChild(button);
            menu->updateLayout();
        }
        return true;
    }

    void onPathfinder(CCObject*) {
        showLevelReport(m_level);
    }
};

class $modify(PfPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        if (auto menu = this->getChildByID("right-button-menu")) {
            auto button = makeButton("OBJ", CircleBaseSize::Small, this, menu_selector(PfPauseLayer::onDumpObjects));
            button->setID("dump-objects-button"_spr);
            menu->addChild(button);
            menu->updateLayout();
        }
    }

    void onDumpObjects(CCObject*) {
        auto layer = PlayLayer::get();
        if (!layer || !layer->m_level) return;

        auto objects = dumpObjects(layer);
        if (objects.isErr()) return showError(objects.unwrapErr());

        // The matching .gmd, so the dump can be lined up with the level string.
        auto level = exportLevel(layer->m_level);
        if (level.isErr()) return showError(level.unwrapErr());

        showSaved("the objects and the level", objects.unwrap());
    }
};
