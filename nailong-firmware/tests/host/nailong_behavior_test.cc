#include <cassert>
#include <string>

#include "boards/otto-robot/nailong_scene_state.h"
#include "mcp_server.h"

int main() {
    using Status = NailongSceneState::Status;
    NailongSceneState scene;
    assert(scene.Image(true) == -1);
    scene.SetStatus(Status::Speaking, 0);
    assert(scene.Image(true) == -1);
    scene.SetStatus(Status::Standby, 0);
    assert(scene.Image(true) == 0);
    scene.SetStatus(Status::Speaking, 0);
    int first = scene.Image(false);
    assert(first >= 2 && first <= 8);
    scene.SetStatus(Status::Speaking, 6);
    assert(scene.Image(false) == first);  // More sentences do not change the reply picture.
    scene.SetStatus(Status::Listening, 0);
    assert(scene.Image(false) == first);  // Preserve it while the audio tail drains.
    assert(scene.Image(true) == 0);
    scene.SetStatus(Status::Speaking, 0);
    assert(scene.Image(false) != first);
    for (unsigned random = 0; random < 100; ++random) {
        int previous = scene.Image(false);
        scene.SetStatus(Status::Listening, 0);
        scene.Image(true);
        scene.SetStatus(Status::Speaking, random);
        int selected = scene.Image(false);
        assert(selected >= 2 && selected <= 8 && selected != previous);
    }
    scene.SetLaughing(true);
    assert(scene.Image(false) == 1);
    scene.SetStatus(Status::Listening, 0);
    assert(scene.Image(false) == 1);
    scene.SetLaughing(false);
    assert(scene.Image(true) == 0);
    scene.SetStatus(Status::System, 0);
    assert(scene.Image(true) == -1);
    scene.SetStatus(Status::Standby, 0);
    scene.SetLaughing(true);
    scene.SetStatus(Status::Speaking, 0);
    assert(scene.Image(false) == 1);
    scene.SetLaughing(false);
    assert(scene.Image(true) == 0);  // A local laugh did not consume a reply picture.

    McpTool sync("test.sync", "sync", PropertyList(),
                 [](const PropertyList&) -> ReturnValue { return std::string("ordinary result"); });
    assert(!sync.is_deferred());
    std::string result = sync.Call(PropertyList());
    cJSON* root = cJSON_Parse(result.c_str());
    assert(root && cJSON_IsFalse(cJSON_GetObjectItem(root, "isError")));
    auto* item = cJSON_GetArrayItem(cJSON_GetObjectItem(root, "content"), 0);
    assert(std::string(cJSON_GetObjectItem(item, "text")->valuestring) == "ordinary result");
    cJSON_Delete(root);

    McpTool::Completion pending;
    bool replied = false;
    McpTool deferred("test.deferred", "deferred", PropertyList(),
                     [&pending](const PropertyList&, McpTool::Completion completion) {
                         pending = std::move(completion);
                     });
    assert(deferred.is_deferred());
    deferred.CallDeferred(PropertyList(), [&replied, &result](ReturnValue value) {
        replied = true;
        result = McpTool::FormatResult(std::move(value));
    });
    assert(!replied && pending);  // Cloud continuation waits until playback finishes.
    pending(std::string("playback finished"));
    assert(replied && result.find("playback finished") != std::string::npos);
    root = cJSON_Parse(result.c_str());
    assert(root && cJSON_IsFalse(cJSON_GetObjectItem(root, "isError")));
    cJSON_Delete(root);
}
