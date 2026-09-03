#include <SDL3/SDL.h>

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlgpu3.h"

int main() {
  SDL_Init(SDL_INIT_VIDEO);
  SDL_Window* window = SDL_CreateWindow("spn + imgui", 1280, 800, SDL_WINDOW_RESIZABLE);
  SDL_GPUDevice* gpu = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_METALLIB, true, nullptr);
  SDL_ClaimWindowForGPUDevice(gpu, window);

  ImGui::CreateContext();
  ImGuiIO* io = &ImGui::GetIO();
  io->ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleFonts;
  ImGui::StyleColorsDark();
  ImGui_ImplSDL3_InitForSDLGPU(window);

  ImGui_ImplSDLGPU3_InitInfo init = {};
  init.Device = gpu;
  init.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(gpu, window);
  init.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
  ImGui_ImplSDLGPU3_Init(&init);

  int counter = 0;
  float value = 0.5f;
  bool running = true;
  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL3_ProcessEvent(&event);
      if (event.type == SDL_EVENT_QUIT) {
        running = false;
      }
    }

    ImGui_ImplSDLGPU3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("hello");
    ImGui::Text("built with spn");
    ImGui::SliderFloat("value", &value, 0.0f, 1.0f);
    if (ImGui::Button("click me")) {
      counter++;
    }
    ImGui::SameLine();
    ImGui::Text("clicked %d times", counter);
    ImGui::End();

    ImGui::ShowDemoWindow();
    ImGui::Render();

    ImDrawData* draw_data = ImGui::GetDrawData();
    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(gpu);
    SDL_GPUTexture* swapchain = nullptr;
    SDL_WaitAndAcquireGPUSwapchainTexture(commands, window, &swapchain, nullptr, nullptr);
    if (swapchain) {
      ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, commands);

      SDL_GPUColorTargetInfo target = {};
      target.texture = swapchain;
      target.clear_color = { 0.1f, 0.1f, 0.15f, 1.0f };
      target.load_op = SDL_GPU_LOADOP_CLEAR;
      target.store_op = SDL_GPU_STOREOP_STORE;

      SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &target, 1, nullptr);
      ImGui_ImplSDLGPU3_RenderDrawData(draw_data, commands, pass);
      SDL_EndGPURenderPass(pass);
    }
    SDL_SubmitGPUCommandBuffer(commands);
  }

  SDL_WaitForGPUIdle(gpu);
  ImGui_ImplSDL3_Shutdown();
  ImGui_ImplSDLGPU3_Shutdown();
  ImGui::DestroyContext();
  SDL_ReleaseWindowFromGPUDevice(gpu, window);
  SDL_DestroyGPUDevice(gpu);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
