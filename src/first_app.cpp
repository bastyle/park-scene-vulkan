#include "first_app.hpp"

#include "keyboard_movement_controller.hpp"
#include "lve_buffer.hpp"
#include "lve_camera.hpp"
#include "systems/point_light_system.hpp"
#include "systems/simple_render_system.hpp"

// libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

// std
#include <array>
#include <cassert>
#include <chrono>
#include <stdexcept>
#include <cstdlib> // for rand() and srand()
#include <ctime> // for time()

namespace lve {

FirstApp::FirstApp() {
  globalPool =
      LveDescriptorPool::Builder(lveDevice)
          .setMaxSets(LveSwapChain::MAX_FRAMES_IN_FLIGHT)
          .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, LveSwapChain::MAX_FRAMES_IN_FLIGHT)
          .build();
  loadGameObjects();
  loadTreeObjects();
  loadBenchObjects();
  loadBushObjects();
  loadSolarLight();
  loadPlantObjects();
}

FirstApp::~FirstApp() {}

void FirstApp::run() {
  std::vector<std::unique_ptr<LveBuffer>> uboBuffers(LveSwapChain::MAX_FRAMES_IN_FLIGHT);
  for (int i = 0; i < uboBuffers.size(); i++) {
    uboBuffers[i] = std::make_unique<LveBuffer>(
        lveDevice,
        sizeof(GlobalUbo),
        1,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    uboBuffers[i]->map();
  }

  auto globalSetLayout =
      LveDescriptorSetLayout::Builder(lveDevice)
          .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
          .build();

  std::vector<VkDescriptorSet> globalDescriptorSets(LveSwapChain::MAX_FRAMES_IN_FLIGHT);
  for (int i = 0; i < globalDescriptorSets.size(); i++) {
    auto bufferInfo = uboBuffers[i]->descriptorInfo();
    LveDescriptorWriter(*globalSetLayout, *globalPool)
        .writeBuffer(0, &bufferInfo)
        .build(globalDescriptorSets[i]);
  }

  SimpleRenderSystem simpleRenderSystem{
      lveDevice,
      lveRenderer.getSwapChainRenderPass(),
      globalSetLayout->getDescriptorSetLayout()};
  PointLightSystem pointLightSystem{
      lveDevice,
      lveRenderer.getSwapChainRenderPass(),
      globalSetLayout->getDescriptorSetLayout()};
  LveCamera camera{};

  auto viewerObject = LveGameObject::createGameObject();
  viewerObject.transform.translation.z = -2.5f;
  KeyboardMovementController cameraController{};

  auto currentTime = std::chrono::high_resolution_clock::now();
  while (!lveWindow.shouldClose()) {
    glfwPollEvents();

    auto newTime = std::chrono::high_resolution_clock::now();
    float frameTime =
        std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
    currentTime = newTime;

    cameraController.moveInPlaneXZ(lveWindow.getGLFWwindow(), frameTime, viewerObject);
    camera.setViewYXZ(viewerObject.transform.translation, viewerObject.transform.rotation);

    float aspect = lveRenderer.getAspectRatio();
    camera.setPerspectiveProjection(glm::radians(50.f), aspect, 0.1f, 100.f);

    if (auto commandBuffer = lveRenderer.beginFrame()) {
      int frameIndex = lveRenderer.getFrameIndex();
      FrameInfo frameInfo{
          frameIndex,
          frameTime,
          commandBuffer,
          camera,
          globalDescriptorSets[frameIndex],
          gameObjects};

      // update
      GlobalUbo ubo{};
      ubo.projection = camera.getProjection();
      ubo.view = camera.getView();
      ubo.inverseView = camera.getInverseView();
      pointLightSystem.update(frameInfo, ubo);
      uboBuffers[frameIndex]->writeToBuffer(&ubo);
      uboBuffers[frameIndex]->flush();

      // render
      lveRenderer.beginSwapChainRenderPass(commandBuffer);

      // order here matters
      simpleRenderSystem.renderGameObjects(frameInfo);
      pointLightSystem.render(frameInfo);

      lveRenderer.endSwapChainRenderPass(commandBuffer);
      lveRenderer.endFrame();
    }
  }

  vkDeviceWaitIdle(lveDevice.device());
}

void FirstApp::loadGameObjects() {
  std::shared_ptr<LveModel> lveModel = LveModel::createModelFromFile(lveDevice, "models/quad.obj");
  auto floor = LveGameObject::createGameObject();
  floor.model = lveModel;
  floor.transform.translation = {0.f, .5f, 0.f};
  floor.transform.scale = {10.f, 2.f, 10.f};
  gameObjects.emplace(floor.getId(), std::move(floor));

  lveModel = LveModel::createModelFromFile(lveDevice, "models/simple_model.obj");
  auto characterlowpoly2 = LveGameObject::createGameObject();
  characterlowpoly2.model = lveModel;
  characterlowpoly2.transform.translation = {1.f, -.2f, 0.f};
  characterlowpoly2.transform.scale = {.2f, .2f, .2f};
  gameObjects.emplace(characterlowpoly2.getId(), std::move(characterlowpoly2));

  /*
  std::vector<glm::vec3> lightColors{
      {1.f, .1f, .1f},
      {.1f, .1f, 1.f},
      {.1f, 1.f, .1f},
      {1.f, 1.f, .1f},
      {.1f, 1.f, 1.f},
      {1.f, 1.f, 1.f},  //
      {1.f, 0.5f, 0.f}
  };

  for (int i = 0; i < lightColors.size(); i++) {
    auto pointLight = LveGameObject::makePointLight(5.2f);
    pointLight.color = lightColors[i];
    auto rotateLight = glm::rotate(
        glm::mat4(1.f),
        (i * glm::two_pi<float>()) / lightColors.size(),
        {0.f, -1.f, 0.f});
    pointLight.transform.translation = glm::vec3(rotateLight * glm::vec4(-1.f, -1.f, -1.f, 1.f));
    gameObjects.emplace(pointLight.getId(), std::move(pointLight));
  }*/
}

void FirstApp::loadSolarLight() {
  auto orangeLight = LveGameObject::makePointLight(350.2f);
  orangeLight.color = {1.f, 0.5f, 0.f};
  // orangeLight.transform.translation = {-2.f, -20.f, -5.f};
  orangeLight.transform.translation = {-2.f, -30.f, -5.f};  // radio? altura ?
  // orangeLight.transform.scale = {10.f, 10.f, 10.f};
  orangeLight.transform.scale = {1.f, 1.f, 1.f};
  gameObjects.emplace(orangeLight.getId(), std::move(orangeLight));
}

void FirstApp::loadTreeObjects() {
  std::shared_ptr<LveModel> lveModel =
      LveModel::createModelFromFile(lveDevice, "models/park/Tree/3Trees.obj");
  auto tree = LveGameObject::createGameObject();
  tree.model = lveModel;
  tree.transform.scale = {.3f, .3f, .3f};
  tree.transform.translation = {-8.5f, .5f, 0.f};
  //gameObjects.emplace(tree.getId(), std::move(tree));

  lveModel = LveModel::createModelFromFile(lveDevice, "models/park/Tree01/tree01.obj");
  auto tree2 = LveGameObject::createGameObject();
  tree2.model = lveModel;
  tree2.transform.scale = {.3f, .3f, .3f};
  tree2.transform.translation = {-8.5f, .5f, 0.f};
  gameObjects.emplace(tree2.getId(), std::move(tree2));



}

void FirstApp::loadBenchObjects() {
  std::shared_ptr<LveModel> lveModel =
      LveModel::createModelFromFile(lveDevice, "models/park/bench/bench-1.obj");

  auto tree = LveGameObject::createGameObject();
  tree.model = lveModel;
  tree.transform.translation = {
      -2.f,
      .5f,
      4.f};  // x (positivo derecha) / y (positivo-> abajo .5 base) / z profundidad
  tree.transform.scale = {.5f, .5f, .5f};
  tree.transform.rotation = {
      0.f,
      2.f,
      0.f};  // x inclinacion arriba abajo / giro sobre si misma / inclinacion hacia los lados

  // tree.transform.rotation = {0.f,0.f,5.f};
  gameObjects.emplace(tree.getId(), std::move(tree));
}

void FirstApp::loadBushObjects() {
  std::shared_ptr<LveModel> lveModel =
      LveModel::createModelFromFile(lveDevice, "models/park/bush/bush-1.obj");

  /*auto bush = LveGameObject::createGameObject();
  bush.model = lveModel;
  bush.transform.translation = {-2.f, .13f, 9.7f};
  bush.transform.scale = {.5f, 1.f, .5f};
  bush.transform.rotation = {0.f,2.f,0.f};  // x inclinacion arriba abajo / giro sobre si misma /
  inclinacion hacia los lados gameObjects.emplace(bush.getId(), std::move(bush));*/

  for (int i = -10; i <= 10; i++) {
    auto bush = LveGameObject::createGameObject();
    bush.model = lveModel;
    // bush.transform.translation = {-2.f, .13f, 9.7f};
    bush.transform.translation = {-10.f, .13f, i * .9f};
    bush.transform.scale = {.5f, 1.f, .5f};
    bush.transform.rotation = {0.f, 2.f, 0.f};
    gameObjects.emplace(bush.getId(), std::move(bush));
    //
    auto bushL = LveGameObject::createGameObject();
    bushL.model = lveModel;
    bushL.transform.translation = {10.f, .2f, i * .9f};
    bushL.transform.scale = {.5f, 1.f, .5f};
    bushL.transform.rotation = {0.f, 2.f, 0.f};
    gameObjects.emplace(bushL.getId(), std::move(bushL));
    //
    auto bush3 = LveGameObject::createGameObject();
    bush3.model = lveModel;
    bush3.transform.translation = {i * 1.f, .2f, 9.7f};
    bush3.transform.scale = {.5f, 1.f, .5f};
    bush3.transform.rotation = {0.f, 0.f, 0.f};
    gameObjects.emplace(bush3.getId(), std::move(bush3));

    auto bush4 = LveGameObject::createGameObject();
    bush4.model = lveModel;
    bush4.transform.translation = {i * 1.f, .2f, -9.7f};
    bush4.transform.scale = {.5f, 1.f, .5f};
    bush4.transform.rotation = {0.f, 0.f, 0.f};
    gameObjects.emplace(bush4.getId(), std::move(bush4));
  }
}

void FirstApp::loadPlantObjects() {
  /*std::shared_ptr<LveModel> lveModel =
      LveModel::createModelFromFile(lveDevice, "models/park/plant/plant-1.obj");
  auto plant = LveGameObject::createGameObject();
  plant.model = lveModel;
  plant.transform.translation = {-2.f,.5f,4.f};
  plant.transform.scale = {.5f, .5f, .5f};
  plant.transform.rotation = {0.f,2.f,3.1f};
  gameObjects.emplace(plant.getId(), std::move(plant));*/
  std::srand(std::time(nullptr));  // use current time as seed for random generator

  std::shared_ptr<LveModel> lveModel =
      LveModel::createModelFromFile(lveDevice, "models/park/plant/plant-1.obj");

  int numPlants = 8 + std::rand() % 8;  // Random number between 8 and 15

  for (int i = 0; i < numPlants; ++i) {
    auto plant = LveGameObject::createGameObject();
    plant.model = lveModel;

    // Generate random position within the plane
    float x = -10.f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / (20.f)));
    float y = .5f;
    float z = -10.f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / (20.f)));
    // Generate random scale
    float scale = 0.3f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / (0.7f)));

    plant.transform.translation = {x, y, z};
    //plant.transform.scale = {.5f, .5f, .5f};
    plant.transform.scale = {scale, scale, scale};
    plant.transform.rotation = {0.f, 2.f, 3.1f};
    gameObjects.emplace(plant.getId(), std::move(plant));
  }
}

}  // namespace lve
